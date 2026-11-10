from flask import Flask, render_template, jsonify, request, Response
import subprocess, threading, queue, json, os, signal, time

BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ENGINE = os.path.join(BASE_DIR, "backend", "reader_writer")
app = Flask(__name__)

process = None
process_lock = threading.Lock()
events = queue.Queue(maxsize=1000)
state_lock = threading.Lock()
state = {"running": False, "paused": False, "readers": 0, "value": 0,
         "reader_ops": 0, "writer_ops": 0, "reader_threads": 0,
         "writer_threads": 0, "events": 0, "last_message": "Ready to simulate"}

def publish(event):
    with state_lock:
        state["events"] += 1
        state["last_message"] = event.get("message", "")
        state["readers"] = event.get("readers", state["readers"])
        state["value"] = event.get("value", state["value"])
        typ, role = event.get("type"), event.get("role")
        if typ == "created":
            state["reader_threads" if role == "R" else "writer_threads"] += 1
        if typ == "read_end": state["reader_ops"] += 1
        if typ == "write_end": state["writer_ops"] += 1
    try:
        events.put_nowait(event)
    except queue.Full:
        try: events.get_nowait()
        except queue.Empty: pass
        try: events.put_nowait(event)
        except queue.Full: pass

def read_engine_output(proc):
    for line in iter(proc.stdout.readline, ""):
        try:
            event = json.loads(line.strip())
            publish(event)
        except (json.JSONDecodeError, ValueError):
            continue
    with state_lock:
        state["running"] = False
        state["paused"] = False
    publish({"type":"system", "role":"-", "id":0, "message":"Engine process ended",
             "readers":state["readers"], "value":state["value"]})

def send_command(command):
    global process
    with process_lock:
        if process is None or process.poll() is not None:
            return False
        try:
            process.stdin.write(command + "\n")
            process.stdin.flush()
            return True
        except (BrokenPipeError, OSError):
            return False

@app.route("/")
def index():
    return render_template("index.html")

@app.get("/api/status")
def api_status():
    with state_lock:
        return jsonify(dict(state))

@app.post("/api/start")
def start():
    global process
    with process_lock:
        if process is not None and process.poll() is None:
            return jsonify(ok=True, message="Simulation is already running")
        if not os.path.exists(ENGINE):
            return jsonify(ok=False, message="C engine not found. Compile backend/reader_writer.c first."), 500
        process = subprocess.Popen([ENGINE], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, text=True, bufsize=1)
        with state_lock:
            state.update(running=True, paused=False, readers=0, value=0,
                         reader_ops=0, writer_ops=0, reader_threads=0,
                         writer_threads=0, events=0, last_message="Engine started")
        threading.Thread(target=read_engine_output, args=(process,), daemon=True).start()
    return jsonify(ok=True, message="Simulation engine started")

@app.post("/api/add/<role>")
def add(role):
    command = {"reader":"ADD_READER", "writer":"ADD_WRITER"}.get(role)
    if not command: return jsonify(ok=False, message="Unknown worker type"), 400
    if not send_command(command): return jsonify(ok=False, message="Start the simulation first"), 409
    return jsonify(ok=True, message=f"{role.title()} added")

@app.post("/api/pause")
def pause():
    if not send_command("PAUSE"): return jsonify(ok=False, message="Simulation is not running"), 409
    with state_lock: state["paused"] = True
    return jsonify(ok=True)

@app.post("/api/resume")
def resume():
    if not send_command("RESUME"): return jsonify(ok=False, message="Simulation is not running"), 409
    with state_lock: state["paused"] = False
    return jsonify(ok=True)

@app.post("/api/stop")
def stop():
    global process
    with process_lock:
        if process is None or process.poll() is not None:
            with state_lock: state["running"] = False
            return jsonify(ok=True, message="Simulation already stopped")
        try:
            process.stdin.write("SHUTDOWN\n")
            process.stdin.flush()
            process.wait(timeout=3)
        except Exception:
            process.terminate()
            try: process.wait(timeout=2)
            except subprocess.TimeoutExpired: process.kill()
        with state_lock:
            state["running"] = False
            state["paused"] = False
    return jsonify(ok=True, message="Simulation stopped")

@app.post("/api/reset")
def reset():
    global process
    with process_lock:
        if process is not None and process.poll() is None:
            return jsonify(ok=False, message="Stop the simulation before resetting"), 409
        with state_lock:
            state.update(running=False, paused=False, readers=0, value=0,
                         reader_ops=0, writer_ops=0, reader_threads=0,
                         writer_threads=0, events=0, last_message="Reset complete")
    while True:
        try: events.get_nowait()
        except queue.Empty: break
    return jsonify(ok=True, message="Dashboard counters reset")

@app.get("/api/events")
def stream_events():
    def generate():
        yield "data: " + json.dumps({"type":"connected","message":"Live event stream connected"}) + "\n\n"
        while True:
            try:
                event = events.get(timeout=15)
                yield "data: " + json.dumps(event) + "\n\n"
            except queue.Empty:
                yield ": keep-alive\n\n"
    return Response(generate(), mimetype="text/event-stream",
                    headers={"Cache-Control":"no-cache", "X-Accel-Buffering":"no"})

if __name__ == "__main__":
    app.run(host="127.0.0.1", port=5050, threaded=True, debug=False)
