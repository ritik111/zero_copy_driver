# High-Performance Zero-Copy Ring Buffer Driver

[![License: GPL](https://img.shields.io/badge/License-GPL-blue.svg)](https://opensource.org/licenses/GPL-3.0)
[![Linux Kernel](https://img.shields.io/badge/Linux-Kernel%206.x-orange.svg)](https://kernel.org)
[![Language](https://img.shields.io/badge/Language-C-green.svg)](https://en.cppreference.com/w/c)

A Linux kernel character device driver that implements a Zero-Copy Data Plane using memory mapping (mmap) and a simulated DMA engine via kernel timers. This project demonstrates high-efficiency communication between Kernel-space and User-space, suitable for high-throughput applications like FPGA data acquisition or software-defined radio.
 Key Features

---

##  Key Features



* **Zero-Copy Memory Mapping:** Utilizes remap_pfn_range to map kernel-allocated pages directly into user-space, bypassing the overhead of copy_to_user syscalls.
* **Asynchronous Notification:** Implements the poll subsystem with wait-queues to allow user-space applications to sleep efficiently while waiting for data.
* **Simulated DMA Engine:** A kernel timer_list simulates hardware interrupts, producing data bursts at configurable intervals (default: 50ms).
* **Atomic Telemetry:** Uses atomic primitives (__sync_fetch_and_add) to track produced, consumed, and overflowed bytes across CPU cores without race conditions.
* **Flow Control** Robust handling of buffer overflow (backpressure) scenarios with a dedicated overflow counter.
---

## Steps for execution
 cd zero_copy_driver (after clonning the repository)
 
 make
 
 chmod +x ./scripts/load.sh ./scripts/unload.sh

**run driver using :**   ./scripts/load.sh

**run user program  :**   ./app/consumer_app  ( press ctrl+c to break the execution )

**close the driver :**  ./scripts/unload.sh

make clean

---

## 🔐 Technical Implementation

### Ring Buffer Metadata
The kernel and user-space share a control structure located at the start of the mapped memory region:

```c
struct rb_metadata {
    unsigned int head;               // Producer Index (Kernel)
    unsigned int tail;               // Consumer Index (User)
    unsigned long total_bytes_produced;
    unsigned long overflow_count;    // Dropped data telemetry
    unsigned long total_bytes_consumed;
    char data[];                     // Flexible Array Member
};
