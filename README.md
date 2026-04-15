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

The system uses Linux namespaces (PID, UTS, mount) to isolate containers while sharing the same kernel.
Filesystem isolation is achieved using chroot.

The supervisor process manages container lifecycle using clone() and tracks metadata in a linked list.
Zombie processes are avoided using waitpid().

Two IPC mechanisms are used:

* UNIX domain sockets for CLI communication
* ioctl interface for communication with the kernel module

The kernel module monitors memory usage using RSS.
A periodic timer checks memory usage and enforces limits:

* Soft limit → logs warning
* Hard limit → kills process

Scheduling behavior is analyzed using CPU-bound and I/O-bound workloads.

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
