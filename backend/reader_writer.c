#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

#define OPS_PER_THREAD 3

static sem_t resource;
static pthread_mutex_t reader_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t output_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t gate_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t gate_cond = PTHREAD_COND_INITIALIZER;

static int reader_count = 0;
static int resource_value = 0;
static int paused = 0;
static int active_threads = 0;
static int next_reader_id = 1;
static int next_writer_id = 1;

typedef struct {
    char role;
    int id;
} Worker;

static void emit_event(const char *type, char role, int id, const char *message) {
    pthread_mutex_lock(&output_mutex);
    printf("{\"type\":\"%s\",\"role\":\"%c\",\"id\":%d,\"message\":\"%s\",\"readers\":%d,\"value\":%d}\n",
           type, role, id, message, reader_count, resource_value);
    fflush(stdout);
    pthread_mutex_unlock(&output_mutex);
}

static void wait_if_paused(void) {
    pthread_mutex_lock(&gate_mutex);
    while (paused) pthread_cond_wait(&gate_cond, &gate_mutex);
    pthread_mutex_unlock(&gate_mutex);
}

static void *reader_thread(void *arg) {
    Worker *w = (Worker *)arg;
    int id = w->id;
    free(w);

    emit_event("created", 'R', id, "Reader thread created");
    for (int i = 0; i < OPS_PER_THREAD; i++) {
        wait_if_paused();

        pthread_mutex_lock(&reader_mutex);
        reader_count++;
        if (reader_count == 1) sem_wait(&resource);
        pthread_mutex_unlock(&reader_mutex);

        char msg[100];
        snprintf(msg, sizeof(msg), "Reader %d entered the critical section", id);
        emit_event("read_start", 'R', id, msg);

        usleep(550000);

        snprintf(msg, sizeof(msg), "Reader %d completed reading", id);
        emit_event("read_end", 'R', id, msg);

        pthread_mutex_lock(&reader_mutex);
        reader_count--;
        if (reader_count == 0) sem_post(&resource);
        pthread_mutex_unlock(&reader_mutex);

        usleep(250000);
    }

    pthread_mutex_lock(&gate_mutex);
    active_threads--;
    pthread_mutex_unlock(&gate_mutex);
    emit_event("finished", 'R', id, "Reader thread finished");
    return NULL;
}

static void *writer_thread(void *arg) {
    Worker *w = (Worker *)arg;
    int id = w->id;
    free(w);

    emit_event("created", 'W', id, "Writer thread created");
    for (int i = 0; i < OPS_PER_THREAD; i++) {
        wait_if_paused();
        sem_wait(&resource);

        resource_value++;
        char msg[100];
        snprintf(msg, sizeof(msg), "Writer %d is updating shared resource", id);
        emit_event("write_start", 'W', id, msg);

        usleep(750000);

        snprintf(msg, sizeof(msg), "Writer %d completed writing", id);
        emit_event("write_end", 'W', id, msg);

        sem_post(&resource);
        usleep(350000);
    }

    pthread_mutex_lock(&gate_mutex);
    active_threads--;
    pthread_mutex_unlock(&gate_mutex);
    emit_event("finished", 'W', id, "Writer thread finished");
    return NULL;
}

static void add_worker(char role) {
    Worker *w = malloc(sizeof(Worker));
    if (!w) {
        emit_event("error", role, 0, "Memory allocation failed");
        return;
    }
    w->role = role;
    if (role == 'R') w->id = next_reader_id++;
    else w->id = next_writer_id++;

    pthread_t tid;
    pthread_mutex_lock(&gate_mutex);
    active_threads++;
    pthread_mutex_unlock(&gate_mutex);

    int rc = pthread_create(&tid, NULL, role == 'R' ? reader_thread : writer_thread, w);
    if (rc != 0) {
        pthread_mutex_lock(&gate_mutex);
        active_threads--;
        pthread_mutex_unlock(&gate_mutex);
        free(w);
        emit_event("error", role, 0, "Could not create thread");
        return;
    }
    pthread_detach(tid);
}

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    sem_init(&resource, 0, 1);
    emit_event("system", '-', 0, "SyncView 3.0 C engine ready");

    char command[128];
    while (fgets(command, sizeof(command), stdin)) {
        command[strcspn(command, "\r\n")] = 0;

        if (strcmp(command, "ADD_READER") == 0) {
            add_worker('R');
        } else if (strcmp(command, "ADD_WRITER") == 0) {
            add_worker('W');
        } else if (strcmp(command, "PAUSE") == 0) {
            pthread_mutex_lock(&gate_mutex);
            paused = 1;
            pthread_mutex_unlock(&gate_mutex);
            emit_event("paused", '-', 0, "New operations paused; active critical sections finish safely");
        } else if (strcmp(command, "RESUME") == 0) {
            pthread_mutex_lock(&gate_mutex);
            paused = 0;
            pthread_cond_broadcast(&gate_cond);
            pthread_mutex_unlock(&gate_mutex);
            emit_event("resumed", '-', 0, "Simulation resumed");
        } else if (strcmp(command, "STATUS") == 0) {
            emit_event("status", '-', 0, "Status requested");
        } else if (strcmp(command, "SHUTDOWN") == 0) {
            break;
        }
    }

    pthread_mutex_lock(&gate_mutex);
    paused = 0;
    pthread_cond_broadcast(&gate_cond);
    pthread_mutex_unlock(&gate_mutex);

    /* Give detached workers time to finish during a normal shutdown. */
    for (int i = 0; i < 100; i++) {
        pthread_mutex_lock(&gate_mutex);
        int active = active_threads;
        pthread_mutex_unlock(&gate_mutex);
        if (active == 0) break;
        usleep(100000);
    }

    sem_destroy(&resource);
    emit_event("system", '-', 0, "SyncView engine stopped");
    return 0;
}
