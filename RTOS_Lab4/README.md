# RTOS Lab4 - FreeRTOS EDF Scheduler

Status: implemented, build-passed, and console-verified.

Platform:

- FreeRTOS demo project
- MSP430FR5994
- Code Composer Studio

## Summary

This lab extends the FreeRTOS demo project with an EDF scheduling path.

FreeRTOS normally selects the highest-priority ready task.  In this lab,
EDF-enabled tasks are selected by earliest absolute deadline instead.

The verified source files are kept in:

```text
modified_files/
```

These files can be copied back into the VM project when rebuilding the lab.

## Modified Files

```text
modified_files/main.c
modified_files/main_blinky.c
modified_files/printf-stdarg.c
modified_files/task.h
modified_files/tasks.c
```

What each file does:

- `main.c`: selects the simple demo path, `main_blinky()`.
- `main_blinky.c`: defines the Lab4 task sets, creates EDF tasks, records
  scheduler events, and prints the CIO result.
- `task.h`: exposes small EDF helper APIs to the application.
- `tasks.c`: stores EDF metadata in each TCB and selects the ready task with
  the earliest absolute deadline.
- `printf-stdarg.c`: fixes the lightweight printf path used by CCS CIO.

## VM Paths

In the course VM, the corresponding project paths are:

```text
/home/rtos/workspace/RTOSDemo/main.c
/home/rtos/workspace/RTOSDemo/Blinky_Demo/main_blinky.c
/home/rtos/workspace/RTOSDemo/printf-stdarg.c
/home/rtos/Downloads/FreeRTOS/FreeRTOS/Source/include/task.h
/home/rtos/Downloads/FreeRTOS/FreeRTOS/Source/tasks.c
```

Build folder:

```text
/home/rtos/workspace/RTOSDemo/Large_Data
```

## Task Sets

Task set 1:

```text
T1: C = 1, P = D = 3
T2: C = 3, P = D = 5
```

Task set 2:

```text
T1: C = 1, P = D = 4
T2: C = 2, P = D = 5
T3: C = 2, P = D = 10
```

Switch task sets in `main_blinky.c`:

```c
#define mainLAB4_TASK_SET             1
```

Use `1` for task set 1 and `2` for task set 2.

## Verification

Both task sets were rebuilt and run on the MSP430FR5994 board through CCS/DSS
CIO capture.

The observed CIO output was checked line-by-line against the expected lab
output:

```text
Task set 1: exact match, 15 lines
Task set 2: exact match, 17 lines
```

For the implementation explanation, read:

```text
Implementation_Log.md
```

For the manual CCS run steps, read:

```text
RUNBOOK.md
```
