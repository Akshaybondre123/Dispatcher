A C++ implementation of a dispatcher system that manages worker processes through pipes.

## How to Run

1. Save all files in one folder
2. Open PowerShell (as Administrator) or Ubuntu terminal
3. Navigate to your folder path
4. Run these commands:

```bash
g++ task.cpp -o task
./task


Input Instructions
First enter Worker Configuration (first 5 lines):
<worker_type> <worker_count>
worker_type: Number between 1-5
worker_count: Number between 1-10

Then enter Job Input:
<job_type> <job_duration>
job_type: Number between 1-5
job_duration: Number between 1-10 (seconds)

Example:
akshay@LAPTOP-HNF1475G:/mnt/c/Users/aksha/Dropbox/C++$ g++ task.cpp -o task
akshay@LAPTOP-HNF1475G:/mnt/c/Users/aksha/Dropbox/C++$ ./task
[Thu Apr 03 15:12:17 2025] Starting job dispatcher system...
[Thu Apr 03 15:12:17 2025] Reading worker configuration...
1 1
[Thu Apr 03 15:12:25 2025] Creating 1 workers of type 1...
2 3
[Thu Apr 03 15:12:29 2025] Creating 3 workers of type 2...
3 1
[Thu Apr 03 15:12:36 2025] Creating 1 workers of type 3...
4 1
[Thu Apr 03 15:12:38 2025] Creating 1 workers of type 4...
5 1
[Thu Apr 03 15:12:39 2025] Creating 1 workers of type 5...
[Thu Apr 03 15:12:39 2025] Worker configuration complete. Ready to accept jobs.
1 3
[Thu Apr 03 15:12:49 2025] Dispatched job type 1 (duration: 3) to worker 1-0
[Thu Apr 03 15:12:49 2025] Worker 1-0: Starting job (duration: 3 seconds)
[Thu Apr 03 15:12:52 2025] Worker 1-0: Job completed