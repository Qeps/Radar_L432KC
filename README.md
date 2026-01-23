# Doppler Radar Processing on STM32 (FreeRTOS)

## Overview

This project implements a **real-time Doppler radar signal processing pipeline** on an **STM32L432KC** microcontroller using **FreeRTOS**.

The system continuously acquires analog radar signals using **ADC with DMA**, processes them in a real-time task to detect motion and estimate speed, and outputs results asynchronously over **UART**.  
Execution time and **deadline misses are explicitly monitored and reported**, making real-time behavior observable.

The project focuses on **correct RTOS architecture**, deterministic behavior, and clear separation of responsibilities rather than raw signal-processing complexity.

---

## Architecture

- ADC + DMA (circular buffer)
- DMA ISR (half / full buffer)
- Motion Processing Task
- FreeRTOS Queue
- UART Task
- Serial Output


### Key Design Choices

- Continuous ADC sampling using DMA (no CPU polling)
- Event-driven task activation via DMA interrupts
- Separation of real-time processing and I/O
- Explicit timing model with deadline detection
- No dynamic memory allocation at runtime

---

## FreeRTOS Tasks

### Motion Processing Task

- Triggered by ADC DMA half/full completion
- Processes one buffer half per activation
- Performs motion detection and speed estimation
- Runs periodically with a defined deadline
- Detects and reports deadline misses

### UART Task

- Receives messages via a queue
- Formats and sends output over UART
- Isolated from real-time constraints

---

## Inter-Task Communication

Tasks communicate using a **single FreeRTOS queue** with a tagged message structure:

```c
typedef enum {
    MSG_RESULT,
    MSG_DEADLINE
} uart_msg_type_t;

typedef struct {
    uart_msg_type_t type;
    union {
        motion_result_t result;
        struct {
            uint32_t miss_cnt;
            uint32_t exec_ms;
        } deadline;
    } u;
} uart_msg_t;
```
This approach allows multiple message types without multiple queues and keeps the UART task simple and extensible.

---

## Signal Processing
For each ADC buffer half:
- DC offset removal (mean subtraction)
- Peak-to-peak amplitude calculation
- Motion detection using amplitude threshold
- Speed estimation using zero-crossing based Doppler frequency
The algorithm is intentionally simple and deterministic, prioritizing predictable execution time.

---

## Timing and Real-Time Behavior
- Processing task runs with a fixed period
- Execution time is measured using RTOS ticks
- Deadline misses are detected and reported over UART
Example output:
```bash
amp_pp=689.0 motion=1 speed=0.89 m/s
DEADLINE MISS: cnt=145 exec=128 ms
```
This makes timing behavior explicit and debuggable.

---

## Why This Is an RTOS Project
- Asynchronous data acquisition (ADC + DMA)
- Event-driven task execution
- Clear task priorities and isolation
- Deterministic inter-task communication
- Measured and reported timing constraints