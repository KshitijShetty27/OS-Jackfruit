# Multi-Container Runtime with Kernel Memory Monitor

## 1. Team Information

* Name: <Your Name>
* SRN: <Your SRN>

---

## 2. Build, Load, and Run Instructions

### Step 1: Build Project

```bash
make
```

### Step 2: Load Kernel Module

```bash
sudo insmod monitor.ko
```

### Step 3: Verify Device

```bash
ls -l /dev/container_monitor
```

---

### Step 4: Start Supervisor

```bash
sudo ./engine supervisor ./rootfs-base
```

---

### Step 5: Create Container Root Filesystems

```bash
cp -a ./rootfs-base ./rootfs-alpha
cp -a ./rootfs-base ./rootfs-beta
```

---

### Step 6: Start Containers

```bash
sudo ./engine start alpha ./rootfs-alpha /bin/sh
sudo ./engine start beta ./rootfs-beta /bin/sh
```

---

### Step 7: List Running Containers

```bash
sudo ./engine ps
```

---

### Step 8: Run Workloads

Copy workloads:

```bash
cp cpu_hog rootfs-alpha/
cp io_pulse rootfs-beta/
cp memory_hog rootfs-beta/
```

Run:

```bash
sudo ./engine start alpha ./rootfs-alpha /cpu_hog
sudo ./engine start beta ./rootfs-beta /io_pulse
sudo ./engine start gamma ./rootfs-beta /memory_hog
```

---

### Step 9: Stop Containers

```bash
sudo ./engine stop alpha
sudo ./engine stop beta
sudo ./engine stop gamma
```

---

### Step 10: Check Kernel Logs

```bash
sudo dmesg | tail
```

---

### Step 11: Verify Clean Teardown

```bash
ps aux | grep defunct
```

---

### Step 12: Unload Module

```bash
sudo rmmod monitor
```

---

## 3. Demo with Screenshots

### 1. Multi-container supervision

Show two or more containers running under a single supervisor.

---

### 2. Metadata tracking

Command:

```bash
sudo ./engine ps
```

Shows container IDs and PIDs.

---

### 3. Bounded-buffer logging

(If implemented) Show container output captured via logging.

---

### 4. CLI and IPC

Command:

```bash
sudo ./engine start gamma ./rootfs-alpha /bin/sh
```

Shows communication between CLI and supervisor.

---

### 5. Soft-limit warning

Kernel logs showing memory warning event.

---

### 6. Hard-limit enforcement

Run:

```bash
memory_hog
```

Then:

```bash
sudo dmesg | tail
```

Shows process killed due to memory limit.

---

### 7. Scheduling experiment

Run:

```bash
cpu_hog vs io_pulse
```

Observe differences in CPU usage.

---

### 8. Clean teardown

Command:

```bash
ps aux | grep defunct
```

Shows no zombie processes.

---

## 4. Engineering Analysis

### Isolation Mechanisms

The runtime uses Linux namespaces:

* PID namespace isolates process IDs
* UTS namespace isolates hostname
* Mount namespace isolates filesystem

`chroot()` provides filesystem isolation.
Containers still share:

* kernel
* CPU scheduler
* memory subsystem

---

### Supervisor and Process Lifecycle

A long-running supervisor:

* creates containers using `clone()`
* tracks metadata (ID, PID)
* handles lifecycle operations
* uses `waitpid()` to prevent zombie processes

---

### IPC, Threads, and Synchronization

Two IPC mechanisms:

1. UNIX domain sockets (CLI ↔ supervisor)
2. ioctl (user ↔ kernel module)

Supervisor handles requests sequentially, avoiding race conditions.

---

### Memory Management and Enforcement

RSS (Resident Set Size):

* measures physical memory usage
* excludes swapped memory

Soft limit:

* generates warning

Hard limit:

* kills process

Kernel enforcement is necessary because:

* user-space cannot reliably control memory
* kernel has direct access to process memory

---

### Scheduling Behavior

Workloads:

* cpu_hog → CPU-bound
* io_pulse → I/O-bound

Observations:

* CPU-bound tasks consume more CPU time
* I/O-bound tasks yield CPU frequently

Linux scheduler ensures:

* fairness
* responsiveness
* throughput

---

## 5. Design Decisions and Tradeoffs

### Namespace Isolation

* Choice: namespaces + chroot
* Tradeoff: lightweight but less secure than VMs
* Justification: efficiency and simplicity

---

### Supervisor Design

* Choice: single supervisor process
* Tradeoff: limited parallelism
* Justification: easier lifecycle control

---

### IPC Mechanism

* Choice: UNIX socket
* Tradeoff: not highly scalable
* Justification: simple and reliable

---

### Kernel Monitor

* Choice: kernel module
* Tradeoff: complexity
* Justification: accurate enforcement

---

### Scheduling Experiments

* Choice: simple workloads
* Tradeoff: limited metrics
* Justification: clear behavior demonstration

---

## 6. Scheduler Experiment Results

| Workload | Behavior               |
| -------- | ---------------------- |
| cpu_hog  | High CPU usage         |
| io_pulse | Intermittent CPU usage |

Observation:

* CPU-bound dominates CPU
* I/O-bound improves responsiveness

---

## Conclusion

This project demonstrates:

* container isolation using namespaces
* kernel-user communication via ioctl
* process lifecycle management
* kernel-level memory enforcement
* Linux scheduling behavior

---
