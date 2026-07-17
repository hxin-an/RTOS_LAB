# RTOS Lab5 - FreeRTOS Checkpoint / Restore

Status: implemented and demonstrated on an MSP430FR5994.

## Summary

Lab5 adds checkpoint and restoration to a FreeRTOS demo. The program periodically commits execution state to FRAM. After a simulated power failure and reset, it selects the newest valid checkpoint and resumes from the last committed iteration.

## Design

Each checkpoint stores the required state in this order:

```text
1. FreeRTOS heap (ucHeap)
2. SRAM state (.data and .bss)
3. CPU context captured with setjmp()
```

Restoration applies the same order and resumes execution with `longjmp()`.

Two FRAM checkpoint slots provide failure tolerance. Each slot contains a magic value, format version, sequence number, size fields, and checksums. A slot becomes valid only after its payload and checksums have been written, so an interrupted commit does not replace the previous valid checkpoint.

## Modified Files

| File | Responsibility |
| --- | --- |
| [`modified_files/main.c`](modified_files/main.c) | Hardware startup, persistent `ucHeap`, and restore hook before task creation |
| [`modified_files/main_blinky.c`](modified_files/main_blinky.c) | Checkpoint storage, commit/restore flow, validation, demo task, and LPM4.5 trigger |
| [`modified_files/lnk_msp430fr5994.cmd`](modified_files/lnk_msp430fr5994.cmd) | Persistent `.checkpoint` `NOINIT` section |
| [`modified_files/FreeRTOSConfig.h`](modified_files/FreeRTOSConfig.h) | Application-allocated FreeRTOS heap |
| [`modified_files/printf-stdarg.c`](modified_files/printf-stdarg.c) | Lightweight output through the CCS CIO console |

Lab5 does not modify the FreeRTOS scheduler. The EDF-specific `task.h` and `tasks.c` changes belong to Lab4 only.

## Memory Placement

The recorded build map placed the persistent regions as follows:

```text
ucHeap       : 0x4000, size 0x2000, persistent FRAM
.checkpoint  : 0x6000, NOINIT FRAM
.data/.bss   : SRAM
.stack       : SRAM
```

The `NOINIT` attribute prevents C startup code from clearing the checkpoint region after reset.

## Demo Output

A fresh run has this shape:

```text
Checkpoint Lab5
fresh boot
i:1
...
commit i:10
...
power_off i:14
```

After reset, a valid checkpoint restores the last committed state:

```text
Checkpoint Lab5
restore
restore i:10
i:10
...
```

The stable demo uses the CCS Debug Console (`RTOSDemo_Lab5:CIO`) as its output path.

## Documentation

- [Implementation log](Implementation_Log.md): checkpoint format, commit ordering, restore logic, and file responsibilities.
- [Runbook](RUNBOOK.md): build, debug, reset, and troubleshooting steps.
