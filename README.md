# Multi-Container Runtime with Kernel Memory Monitor

---

# 1. Team Information

* Name: Mallikarjuna Rao R.V, SRN: PES1UG24AM155
* Name: Kshitij Satish Shetty, SRN: PES1UG24AM143

---

# 2. Project Overview

This project implements a lightweight multi-container runtime in C along with a Linux kernel module for memory monitoring.

The system supports:

* Multiple containers managed by a single supervisor
* Process isolation using Linux namespaces and chroot
* CLI-based container control using UNIX domain sockets
* Kernel-level memory monitoring via ioctl interface
* Logging system using pipes (producer-consumer model)
* Scheduling experiments using CPU-bound and I/O-bound workloads
* Clean container lifecycle management without zombie processes

---

# 3. Build, Load, and Run Instructions

## Step 1: Build

```bash
make
```

---

## Step 2: Load Kernel Module

```bash
sudo insmod monitor.ko
ls -l /dev/container_monitor
```

---

## Step 3: Start Supervisor

```bash
sudo ./engine supervisor ./rootfs-base
```

---

## Step 4: Create Writable Root Filesystems

```bash
cp -a ./rootfs-base ./rootfs-alpha
cp -a ./rootfs-base ./rootfs-beta
```

---

## Step 5: Start Containers

```bash
sudo ./engine start alpha ./rootfs-alpha /bin/sh
sudo ./engine start beta ./rootfs-beta /bin/sh
```

---

## Step 6: List Containers

```bash
sudo ./engine ps
```

---

## Step 7: Run Workloads

```bash
cp cpu_hog rootfs-alpha/
cp io_pulse rootfs-beta/
cp memory_hog rootfs-beta/

sudo ./engine start alpha ./rootfs-alpha /cpu_hog
sudo ./engine start beta ./rootfs-beta /io_pulse
sudo ./engine start gamma ./rootfs-beta /memory_hog
```

---

## Step 8: View Logs (Logging Pipeline)

```bash
sudo ./engine logs alpha
```

---

## Step 9: Stop Containers

```bash
sudo ./engine stop alpha
sudo ./engine stop beta
sudo ./engine stop gamma
```

---

## Step 10: Inspect Kernel Logs

```bash
dmesg | tail
```

---

## Step 11: Verify Clean Teardown

```bash
ps aux | grep defunct
```

---

## Step 12: Unload Module

```bash
sudo rmmod monitor
```

---

# 4. Demo with Screenshots

### 1. Multi-container supervision

![Start](screenshots/start.png)
*Two containers running under supervisor*

---

### 2. Metadata tracking

![PS](screenshots/ps.png)
*Container IDs and PIDs displayed*

---

### 3. CLI and IPC

![CLI](screenshots/cli.png)
*CLI command interacting with supervisor*

---

### 4. Logging system (Bounded-buffer logging)

![Logging](screenshots/logging.png)
*Container output captured via pipe and retrieved using logs command*

---

### 5. Soft-limit warning

![Soft Limit](screenshots/soft_hard_limit.png)
*Kernel logs showing soft memory threshold warning*

---

### 6. Hard-limit enforcement

![Hard Limit](screenshots/soft_hard_limit.png)
*Kernel logs showing container killed after exceeding memory limit*

---

### 7. Scheduling experiment

![Scheduling](screenshots/scheduling.png)
*CPU-bound vs I/O-bound workload behavior*

---

### 8. Clean teardown

![Cleanup](screenshots/cleanup.png)
*No zombie processes after stopping containers*

---

# 5. Test Cases

## Test Case 1: Multi-container execution

```bash
sudo ./engine start alpha ./rootfs-alpha /bin/sh
sudo ./engine start beta ./rootfs-beta /bin/sh
```

Result: Multiple containers run successfully.

---

## Test Case 2: Metadata tracking

```bash
sudo ./engine ps
```

Result: Correct container metadata displayed.

---

## Test Case 3: CLI communication

```bash
sudo ./engine start gamma ./rootfs-alpha /bin/sh
```

Result: Supervisor responds with OK.

---

## Test Case 4: Logging system

```bash
sudo ./engine logs alpha
```

Result: Container output is displayed via logging pipeline.

---

## Test Case 5: Kernel integration

```bash
dmesg | tail
```

Result: Container registration messages visible.

---

## Test Case 6: Soft-limit warning

Result: Kernel logs show soft limit warning before termination.

---

## Test Case 7: Hard-limit enforcement

Result: Memory-hog process is killed when exceeding limit.

---

## Test Case 8: Clean teardown

```bash
ps aux | grep defunct
```

Result: No zombie processes.

---

# 6. Engineering Analysis

### 1. Isolation Mechanisms

The runtime calls `clone(2)` with three namespace flags:

**`CLONE_NEWPID` (PID namespace):** The kernel maintains a separate PID numbering space for the container. The container's first process becomes PID 1 inside its namespace while retaining a host PID visible from the supervisor. Tools like `ps` running inside the container see only that container's processes. If PID 1 inside the container exits, the kernel sends `SIGHUP` to all other processes in that namespace, enforcing lifecycle coupling.

**`CLONE_NEWUTS` (UTS namespace):** The container gets its own hostname and domain name fields, backed by a separate `struct uts_namespace` in the kernel. Calling `sethostname(2)` inside the container updates only that namespace's copy.

**`CLONE_NEWNS` (mount namespace):** Each container gets a private copy of the mount table. We can call `chroot(2)` to make the container's assigned rootfs directory appear as `/`, and mount `/proc` privately. The host's mount table is unaffected.

`chroot(2)` works by changing `task_struct->fs->root` for the process, so all `open(2)` path resolutions start from the container rootfs. `pivot_root(2)` is more thorough — it physically changes the root of the mount namespace, preventing escape via `..` traversal, and is used in production runtimes like Docker's runc.

**What the host kernel still shares with all containers:** the scheduler, device drivers, system call table, network stack (unless `CLONE_NEWNET` is used), and the kernel's physical memory allocator. Containers are not VMs — they share the same kernel binary.

---

### 2. Supervisor and Process Lifecycle

A long-running parent supervisor is essential for three reasons:

1. **Zombie prevention:** POSIX requires that a child's `task_struct` remain in the process table until its parent calls `wait(2)`. Without a living parent, children become zombies indefinitely, consuming process table slots. The supervisor's `SIGCHLD` handler calls `waitpid(-1, &status, WNOHANG)` in a loop to reap all ready children in one handler invocation.

2. **Metadata continuity:** Container metadata (state, exit code, log path, limits) persists across the lifetime of many containers. A short-lived parent would lose this state on exit.

3. **IPC anchor:** The UNIX domain socket at `/tmp/mini_runtime.sock` exists for the supervisor's lifetime, giving all CLI clients a stable rendezvous address.

**Signal delivery:** `SIGCHLD` is configured with `SA_NOCLDSTOP` to suppress spurious signals from `SIGSTOP`/`SIGCONT`. `SIGTERM` and `SIGINT` to the supervisor trigger orderly shutdown. Inside the child, `SIGTERM` is the first signal for graceful stop; `SIGKILL` follows 500ms later if the child is still alive.

**Attribution rule:** The `stop_requested` flag is set in `container_record_t` before any signal is sent. In `SIGCHLD` handler: if `stop_requested` is set → state = `stopped`; else if the signal was `SIGKILL` → state = `hard_limit_killed`; else → state = `killed`.

---

### 3. IPC, Threads, and Synchronization

**Two distinct IPC mechanisms:**

| Mechanism | Used for | Justification |
|---|---|---|
| Anonymous `pipe(2)` | Container stdout/stderr → supervisor (Path A) | Unidirectional, efficient, no rendezvous needed; child inherits write end via `fork`/`clone` |
| UNIX domain socket (`AF_UNIX SOCK_STREAM`) | CLI client ↔ supervisor (Path B) | Bidirectional, reliable, ordered; supports request/response protocol; accessible by path |

**Bounded buffer synchronization:**

| Shared data | Protection | Without it |
|---|---|---|
| `head`, `tail`, `count` | `mutex` | Two producers write same slot → data loss or corruption |
| `not_full` condition | `pthread_cond_t` | Producers spin-wait when full, wasting CPU; or miss wakeup |
| `not_empty` condition | `pthread_cond_t` | Consumer misses data when buffer transitions from empty to non-empty |
| `shutting_down` flag | Read under `mutex` | Consumer sees stale value, sleeps forever after shutdown broadcast |

**Why mutex + condition variables over semaphores:** CVs allow checking a predicate (`count == 0`) atomically with the wait, preventing the TOCTOU race `if count==0 → [preempted] → signal fired → wait` that a semaphore requires extra bookkeeping to avoid. The same mutex that protects `count` also protects `shutting_down`, making shutdown broadcast race-free.

**Why separate `metadata_lock` from the buffer lock:** Container metadata is read/written by SIGCHLD handler, the supervisor event loop, and CLI request handlers. The log buffer is written by producer threads and read by the consumer. Combining them into one lock would create unnecessary contention and potential priority inversion.

---

### 4. Memory Management and Enforcement

**What RSS measures:** Resident Set Size is the count of physical RAM pages currently mapped and present in a process's page tables, multiplied by `PAGE_SIZE`. It measures actual physical memory consumption right now.

**What RSS does not measure:**
- Pages that have been paged out to swap (present bit = 0)
- Pages mapped via `mmap` but not yet faulted in (demand paging)
- Shared library pages (each sharing process contributes to RSS; the kernel uses copy-on-write)
- File-backed mapped pages that can be evicted and re-read from disk

**Soft vs. hard limit policies:**
- **Soft limit:** an advisory ceiling. The kernel module logs a warning when RSS first exceeds the soft threshold, but the process continues running. The operator can investigate; the workload may self-limit. This is intentionally non-disruptive.
- **Hard limit:** a mandatory ceiling. When RSS exceeds the hard threshold, the module sends `SIGKILL`. The process has no opportunity to catch or ignore `SIGKILL`.

**Why enforcement belongs in kernel space:**
1. **Privilege:** `SIGKILL` requires the sender to have appropriate credentials. The kernel module runs with full privilege.
2. **Accuracy:** The kernel module reads RSS from `mm_struct` directly — the ground truth. User-space would have to parse `/proc/<pid>/status`, which introduces file-open overhead and is subject to TOCTOU races.
3. **Timing:** The kernel timer fires every second unconditionally, without being subject to scheduling pressure from the process it monitors. A user-space watchdog could be starved by the very process it tries to kill.
4. **Tamper resistance:** A container cannot disable kernel-space monitoring by catching signals or killing the monitoring thread.

---

### 5. Scheduling Behavior

Linux's **Completely Fair Scheduler (CFS)** tracks `vruntime` (virtual runtime) per runnable task. CFS always picks the task with the smallest `vruntime` to run next, targeting perfectly equal CPU time across equal-priority tasks.

**Nice values and CFS weights:** Each nice level maps to a weight. The weight ratio between nice -5 and nice 15 is approximately 335/15 ≈ **22:1**. CFS allocates time proportional to weight, so a container at nice -5 receives ~22× the CPU time of a container at nice 15 on a single core.

**I/O-bound vs CPU-bound behavior:** An I/O-bound container (e.g., `io_pulse`) voluntarily yields the CPU on each `fsync + usleep`. CFS boosts its `vruntime` as if it had been running, but it is usually sleeping. When it wakes, CFS gives it a fresh time slice because its `vruntime` is behind the CPU-bound container's. This is CFS's built-in responsiveness mechanism — sleepers get "catch-up" scheduling.


---

# 7. Design Decisions and Tradeoffs

| Component      | Design Choice       | Tradeoff                       |
| -------------- | ------------------- | ------------------------------ |
| Isolation      | namespaces + chroot | lightweight but less secure    |
| Supervisor     | single process      | simple but limited scalability |
| IPC            | UNIX sockets        | simple but less flexible       |
| Logging        | pipe-based          | limited buffer size            |
| Kernel monitor | LKM with timer      | added complexity               |

---

# 8. Scheduler Experiment Results

| Workload | Behavior                               |
| -------- | -------------------------------------- |
| cpu_hog  | Consumes high CPU continuously         |
| io_pulse | Sleeps intermittently, lower CPU usage |

Conclusion:
CPU-bound processes dominate scheduling, while I/O-bound processes allow better responsiveness.

---

# 9. Conclusion

This project demonstrates:

* container runtime design
* kernel-user communication
* memory monitoring and enforcement
* logging pipeline implementation
* scheduling behavior in Linux

---
