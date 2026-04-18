# Lab 2 Requirements - EDF Scheduler in uC/OS-II

This document outlines the core requirements extracted from the `SPEC.md` for the Lab 2 assignment.

## 1. Core Objective
Replace the default fixed-priority scheduling in uC/OS-II with **Earliest Deadline First (EDF) scheduling**.

## 2. Modification Requirements

### 2.1 Task Control Block (`OS_TCB` in `ucos_ii.h`)
- Must add a field for the deadline: `INT32U OSTCBDeadline;`
- Initial values (deadline and period) should be passed via the `pdata` argument in `OSTaskCreate()`.

### 2.2 Re-scheduling Points (`os_core.c`)
- Modify the following kernel functions to schedule the task with the minimum `OSTCBDeadline` instead of the highest priority (bitmap):
  1. `OS_Sched()` - Triggered by voluntary CPU yield (e.g., `OSTimeDly`).
  2. `OSIntExit()` - Triggered upon returning from an interrupt (e.g., system tick).
  3. `OSStart()` - Triggered upon initial system startup.
- **Search Method**: Linear search over `OSTCBList` is acceptable. The selected task must have `OSTCBStat == OS_STAT_RDY`.
- **Tie-breaking**: When deadlines are equal, the implementation defines behavior (choosing the smaller Priority number is acceptable).
- **Idle Task Caution**: The linear search must correctly handle the Idle Task (`priority=63`), usually by giving it the maximum possible deadline (`0xFFFFFFFF`).

### 2.3 Application Layer Update / Periodic Task Pattern
Each task must advance its absolute deadline by its period before yielding for the next period:
```c
OSTCBCur->OSTCBDeadline += OSTCBCur->OSTCBPeriod;
OSTimeDly(...);
```

## 3. Output Format
Output should log events similarly to Lab 1:
`Time Event From To`
- `Time`: Current OS tick.
- `Event`: `Preempt` or `Complete`.
- `From`: ID of the suspended task.
- `To`: ID of the newly scheduled task.

## 4. Test Cases

### 4.1 Taskset 1
**Set Details:**
- $t1$: $C=1, T=3, D=3$
- $t2$: $C=3, T=5, D=5$

**Expected Key Points:**
- $t=0$: $t1$ preempts Idle.
- $t=6$: $t1$ ($D=9$) preempts $t2$ ($D=10$).

**Reference Trace from `image3.png`:**
```text
0  Complete 0 1
1  Complete 1 2
4  Complete 2 1
5  Complete 1 2
6  Preempt  2 1
7  Complete 1 2
9  Complete 2 1
10 Complete 1 2
13 Complete 2 1
14 Complete 1 12
15 Preempt  12 1
16 Complete 1 2
19 Complete 2 1
20 Complete 1 2
21 Preempt  2 1
22 Complete 1 2
24 Complete 2 1
25 Complete 1 2
28 Complete 2 1
29 Complete 1 12
30 Preempt  12 1
31 Complete 1 2
34 Complete 2 1
```

### 4.2 Taskset 2
**Set Details:**
- $t1$: $C=1, T=4, D=4$
- $t2$: $C=2, T=5, D=5$
- $t3$: $C=2, T=10, D=10$

**Expected Key Points:**
- $t=4$: $t1$ ($D=8$) preempts $t3$ ($D=10$).
- $t=9$: Idle task runs (no ready tasks).
- $t=10$: $t2$ ($D=15$) preempts Idle.

**Reference Trace from `image3.png`:**
```text
0  Complete 0 1
1  Complete 1 2
3  Complete 2 3
4  Preempt  3 1
5  Complete 1 3
6  Complete 3 2
8  Complete 2 1
9  Complete 1 12
10 Preempt  12 2
12 Complete 2 1
13 Complete 1 3
15 Complete 3 2
17 Complete 2 1
18 Complete 1 12
20 Preempt  12 1
21 Complete 1 2
23 Complete 2 3
24 Preempt  3 1
25 Complete 1 3
26 Complete 3 2
28 Complete 2 1
29 Complete 1 12
30 Preempt  12 2
```

**Notes from the image annotation:**
- `5 C 1 2`
- `7 C 2 3`
- `8 C 3 1`

These green annotations appear to highlight the expected event transitions around the first hyperperiod.

## 5. Grading / Completion Criteria
- [ ] Taskset 1 logs trace correctly.
- [ ] Taskset 2 logs trace correctly.
- [ ] Correct log format.
- [ ] `OSTimeTick()` timing logic is not modified (only scheduling logic).
- [ ] Deadline is advanced explicitly before delay.
- [ ] Idle task is handled.
- [ ] `OSIntExit()` uses EDF logic.
