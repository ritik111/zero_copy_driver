High-Performance Zero-Copy Ring Buffer Driver

A Linux kernel character device driver that implements a Zero-Copy Data Plane using memory mapping (mmap) and a simulated DMA engine via kernel timers. This project demonstrates high-efficiency communication between Kernel-space and User-space, suitable for high-throughput applications like FPGA data acquisition or software-defined radio.
 Key Features

    Zero-Copy Memory Mapping: Utilizes remap_pfn_range to map kernel-allocated pages directly into user-space, bypassing the overhead of copy_to_user syscalls.

    Asynchronous Notification: Implements the poll subsystem with wait-queues to allow user-space applications to sleep efficiently while waiting for data.

    Simulated DMA Engine: A kernel timer_list simulates hardware interrupts, producing data bursts at configurable intervals (default: 50ms).

    Atomic Telemetry: Uses atomic primitives (__sync_fetch_and_add) to track produced, consumed, and overflowed bytes across CPU cores without race conditions.

    Flow Control: Robust handling of buffer overflow (backpressure) scenarios with a dedicated overflow counter.

 Directory Structure

    /driver: Kernel module source code (my_device.c).

    /app: User-space consumer application.

    /include: Shared header files defining the Ring Buffer metadata structure.

    /scripts: Utility scripts for loading/unloading the module.

    /Makefile: Comprehensive build system for both kernel and user components.

Technical Implementation Details
Ring Buffer Metadata

The kernel and user-space share a common understanding of the buffer via the rb_metadata structure:
C

struct rb_metadata {
    unsigned int head;               // Managed by Kernel (Producer)
    unsigned int tail;               // Managed by User (Consumer)
    unsigned long total_bytes_produced;
    unsigned long overflow_count;
    unsigned long total_bytes_consumed;
    char data[];                     // Flexible Array Member for data payload
};

Synchronization

    Kernel: Uses smp_wmb() (Store Memory Barrier) to ensure data is committed to RAM before the head pointer is updated and the user is woken up.

    User-space: Uses __sync_synchronize() (Full Memory Barrier) to ensure it sees the most recent data written by the DMA simulation.

Performance & Results

During testing with a 50ms production interval and 512-byte bursts:

    CPU Usage: Negligible due to poll() based sleeping.

    Reliability: 100% data integrity where Total Produced == Total Consumed in non-overflow conditions.

    Backpressure: Correctly identifies and logs dropped bytes when the consumer is artificially slowed.

Building and Running

    Compile both driver and app:
    Bash

    make

    Load the module:
    Bash

    sudo insmod driver/my_device.ko

    Check device node permissions:
    Bash

    sudo chmod 666 /dev/my_device

    Run the consumer:
    Bash

    ./app/consumer

    Stop the simulation:
    Press Ctrl+C to trigger a graceful shutdown and view telemetry stats.
