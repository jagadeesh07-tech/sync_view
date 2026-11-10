SYNCVIEW 3.0 — READER–WRITER SYNCHRONIZATION SIMULATOR
=====================================================

IMPORTANT
---------
This upgrade replaces:
  backend/reader_writer.c
  frontend/app.py
  frontend/templates/index.html
  frontend/static/style.css
  frontend/static/app.js

Keep your existing project folder and copy these files into the matching folders.
Do not delete your .venv folder. The requirements.txt file is at project root.

RUN IN UBUNTU / WSL TERMINAL
---------------------------
1. Go to the project:
   cd ~/SyncView

2. Compile the C engine using Ubuntu GCC (not Windows MinGW):
   gcc -Wall -Wextra -pthread backend/reader_writer.c -o backend/reader_writer

3. Activate your existing virtual environment:
   source .venv/bin/activate

4. Install Flask if needed:
   pip install -r requirements.txt

5. Start the web application:
   python3 frontend/app.py

6. Open Chrome or Edge in Windows and visit:
   http://127.0.0.1:5050

HOW TO USE
----------
- Start simulation starts the C engine and automatically adds one reader and one writer.
- Add Reader / Add Writer creates real POSIX threads in the C process.
- Each worker performs three operations and then exits.
- Pause prevents workers from starting their next operation. An operation already
  inside the critical section is allowed to finish safely.
- Resume allows waiting workers to continue.
- Stop shuts down the C engine.
- Reset clears dashboard counters after the engine has stopped.

TECHNICAL NOTES
---------------
- Synchronization is implemented in C using pthreads, a binary semaphore,
  a reader-count mutex, and a condition variable for pause/resume.
- The current implementation is reader-preference: multiple readers may read
  concurrently, while a writer gets exclusive access. Under a continuous stream
  of readers, writer starvation is theoretically possible.
- The web dashboard receives JSON-line events emitted by the C program.
  The event log and operation counters are driven by those real events.
- Pause is a safe checkpoint pause, not an OS-level suspension of threads.
