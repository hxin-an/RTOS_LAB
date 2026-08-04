# Embedded Real-Time Systems Portfolio

Coursework and implementation notes from the NYCU Embedded Real-Time Operating Systems course. The repository focuses on scheduling algorithms, RTOS kernel work, board migration, and persistent execution on embedded hardware.

## Portfolio Scope

The work is documented in **Lab1, Lab2, Lab4, and Lab5**.

> **Authorship note:** Lab3 was completed by another person. It is not presented or claimed as part of this portfolio.

## Project Highlights

| Lab | System | Focus | Result |
| --- | --- | --- | --- |
| [Lab1](RTOS_Lab1/) | uC/OS-II | Rate-Monotonic Scheduling (RMS) | Requirements, implementation record, report, and Nios II migration notes |
| [Lab2](RTOS_Lab2/) | uC/OS-II | Earliest-Deadline-First (EDF) scheduling | EDF implementation, report, DOSBox setup, and Nios II migration package |
| [Lab4](RTOS_Lab4/) | FreeRTOS on MSP430FR5994 | Kernel-level EDF scheduler | Added deadline metadata and ready-task selection; both task sets built and matched the expected traces |
| [Lab5](RTOS_Lab5/) | FreeRTOS on MSP430FR5994 | FRAM checkpoint and restore | Implemented double-buffered checkpoints, integrity checks, and recovery after simulated power loss |

## Technical Highlights

- Implemented and analyzed fixed-priority RMS and dynamic-priority EDF scheduling.
- Extended the FreeRTOS task control block and context-switch path for EDF scheduling.
- Used `vTaskDelayUntil()` to model periodic releases without cumulative timing drift.
- Stored FreeRTOS heap, SRAM state, and CPU context in MSP430FR5994 FRAM.
- Designed two-copy checkpoint storage with magic values, versions, sequence numbers, and checksums.
- Documented reproducible build, flash, and demo procedures for Code Composer Studio.
- Recorded migration considerations from legacy x86/DOS tooling to an Altera Nios II target.

## Repository Guide

```text
RTOS_LAB/
├── RTOS_Lab1/   # uC/OS-II RMS scheduling
├── RTOS_Lab2/   # uC/OS-II EDF scheduling
├── RTOS_Lab3/   # excluded from portfolio authorship
├── RTOS_Lab4/   # FreeRTOS EDF scheduler
└── RTOS_Lab5/   # FreeRTOS checkpoint / restore
```

Each completed lab contains some combination of:

- a requirements or report document;
- an implementation log explaining design decisions;
- a runbook for reproducing the demo;
- source snapshots needed to apply the changes to the course project.

## Suggested Reading Order

1. Start with the [Lab4 overview](RTOS_Lab4/README.md) for the FreeRTOS EDF kernel changes.
2. Read the [Lab4 implementation log](RTOS_Lab4/Implementation_Log.md) for file-by-file design details.
3. Continue with the [Lab5 overview](RTOS_Lab5/README.md) for persistent checkpoint architecture.
4. Use the [Lab5 implementation log](RTOS_Lab5/Implementation_Log.md) for the commit and restore sequence.

## Environment

- FreeRTOS and uC/OS-II
- C and MSP430 assembly/linker configuration
- Texas Instruments MSP430FR5994
- Code Composer Studio
- DOSBox / Borland C++ 4.5 for the legacy lab environment
- Altera Nios II migration experiments

## Notes

The repository contains course-framework and RTOS baseline files where they are necessary to reproduce or explain the modifications. Ownership is claimed only for the implementation, analysis, and documentation described above.
