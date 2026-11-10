# SyncView

SyncView is an interactive reader-writer synchronization simulator. It combines a Flask web dashboard with a C engine that runs reader and writer threads using POSIX threads and semaphores.

Repository: [jagadeesh07-tech/sync_view](https://github.com/jagadeesh07-tech/sync_view)

## Features

- Start and stop the simulation engine.
- Add reader and writer threads while the simulation is running.
- Pause and resume workers at safe checkpoints.
- View live events, operation counts, active readers, and the shared value.
- Observe concurrent reader access and exclusive writer access.

## Requirements

- Ubuntu or WSL with GCC and POSIX threads
- Python 3
- pip

## Run locally

Clone the repository and change into the project directory:

```bash
git clone https://github.com/jagadeesh07-tech/sync_view.git
cd sync_view
```

Compile the C engine:

```bash
gcc -Wall -Wextra -pthread backend/reader_writer.c -o backend/reader_writer
```

Create and activate a Python virtual environment, then install the dependencies:

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

Start the web application:

```bash
python3 frontend/app.py
```

Open [http://127.0.0.1:5050](http://127.0.0.1:5050) in your browser.

## Using the simulator

- **Start simulation** launches the engine and adds an initial reader and writer.
- **Add Reader / Add Writer** starts additional worker threads. Each worker performs three operations.
- **Pause / Resume** prevents workers from starting new operations until resumed; operations already in their critical section finish safely.
- **Stop** shuts down the engine.
- **Reset** clears dashboard counters after the engine has stopped.

The synchronization implementation allows multiple readers to access the resource concurrently while ensuring writers have exclusive access. It uses reader preference, so a continuous stream of readers could theoretically delay a writer.
