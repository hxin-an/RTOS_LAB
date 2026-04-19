# Lab2 Report: EDF Scheduling on uC/OS-II

## 1. Modified Files and Purpose

| File | Purpose |
|------|---------|
| `SOFTWARE/uCOS-II/SOURCE/uCOS_II.H` | Add `OSTCBDeadline` to `OS_TCB` while keeping the Lab1 periodic-task fields |
| `SOFTWARE/uCOS-II/SOURCE/OS_CORE.C` | Replace fixed-priority ready selection with EDF, keep periodic release logic, and keep Complete/Preempt logging |
| `SOFTWARE/uCOS-II/Lab2/BC45/SOURCE/INCLUDES.H` | Keep the shared event logging interface |
| `SOFTWARE/uCOS-II/Lab2/BC45/SOURCE/TEST.C` | Application: create Lab2 task sets, initialize deadlines, advance `deadline += period`, and print the EDF trace |
| `NIOS2_Board_Migration_Package/to_test/hello_ucosii.c` | Nios II board application version with board-side trace bridge/filter |
| `NIOS2_Board_Migration_Package/to_test_bsp/UCOSII/inc/ucos_ii.h` | Nios II BSP header carrying the Lab2 `OSTCBDeadline` addition |
| `NIOS2_Board_Migration_Package/to_test_bsp/UCOSII/src/os_core.c` | Nios II BSP kernel source with EDF scheduler integration |

## 2. DOSBox Verification Steps and Results

### 2.1 Build Steps

1. Open DOSBox and mount the Lab2 code directory:
   ```
   mount c c:\NYCU\embedded_real_time\RTOS_LAB\RTOS_Lab2\Lab2_code
   c:
   ```

2. Navigate to the build directory and build:
   ```
   cd \SOFTWARE\uCOS-II\Lab2\BC45\TEST
   MAKETEST.BAT
   ```

3. Run the program:
   ```
   TEST.EXE
   ```

### 2.2 Set1 Verification

With `#define TASK_SET 1` in `TEST.C`, the EDF trace follows the expected two-task pattern:

```
0 Complete 0 1
1 Complete 1 2
3 Preempt 2 1
4 Complete 1 2
5 Complete 2 12
6 Preempt 12 1
...
```

This confirms:
- EDF chooses `task1` first because deadline 3 is earlier than deadline 5
- `task2` is preempted whenever a new `task1` job arrives with an earlier deadline
- idle only runs when neither periodic task is ready

### 2.3 Set2 Verification

Change `#define TASK_SET 1` to `#define TASK_SET 2` in `TEST.C`, rebuild, and run. The EDF trace follows the expected three-task pattern:

```
0 Complete 0 1
1 Complete 1 2
3 Complete 2 3
4 Preempt 3 1
5 Complete 1 2
7 Complete 2 12
...
```

This confirms:
- EDF picks by absolute deadline rather than fixed priority
- `task3` runs when its deadline is earliest among ready jobs
- preemption happens at the correct arrival ticks when a new earlier-deadline job is released

## 3. Board Migration Summary

The Nios II migration reused the final Lab1 board solution as the base and only carried over the Lab2 EDF-specific changes.

### Files to copy into Eclipse projects

| Package file | Eclipse target |
|---|---|
| `NIOS2_Board_Migration_Package/to_test/hello_ucosii.c` | `test/hello_ucosii.c` |
| `NIOS2_Board_Migration_Package/to_test_bsp/UCOSII/inc/ucos_ii.h` | `test_bsp/UCOSII/inc/ucos_ii.h` |
| `NIOS2_Board_Migration_Package/to_test_bsp/UCOSII/inc/os_cfg.h` | `test_bsp/UCOSII/inc/os_cfg.h` |
| `NIOS2_Board_Migration_Package/to_test_bsp/UCOSII/src/os_core.c` | `test_bsp/UCOSII/src/os_core.c` |

### Board build flow

1. Replace the target files in `test/` and `test_bsp/`
2. Right click `test_bsp` -> `Nios II` -> `Generate BSP`
3. Build `test_bsp`
4. Build `test`
5. Run As -> `Nios II Hardware`

## 4. Trace Issues Found on Board and Fixes

### 4.1 Missing `Complete 12 1` in Set2

Board execution can insert hidden BSP/background tasks between two tracked tasks, so one logical transition may appear as:

```
tracked -> hidden -> tracked
```

The original Lab1 bridge logic only merged the two halves when both the tick and the event type matched. On Lab2 board runs, the two halves could happen in the same tick but carry different event types, which caused valid transitions such as `20 Complete 12 1` to disappear.

Fix:
- merge bridge transitions by matching tick
- when restoring the merged event, reuse the saved first-half event type

### 4.2 False `Preempt 1 1` in Set1

After relaxing the bridge rule, the board trace exposed the opposite bug: a stale saved bridge could rewrite an already-valid direct tracked transition, producing a false self-transition such as `20 Preempt 1 1`.

Fix:
- do not consume bridge state on direct `tracked -> tracked` transitions
- clear bridge state immediately when a direct tracked transition arrives
- discard any merged event where `from == to`

These fixes were applied in:
- `NIOS2_Board_Migration_Package/to_test/hello_ucosii.c`

## 5. Final Result

- DOSBox Lab2 EDF behavior was validated for both task sets
- The Nios II board migration package was reduced to only the files that actually need to be copied
- Board-side trace mismatches were resolved by tightening the `LogEvent()` bridge/filter logic
- Final result: both `set1` and `set2` traces align with the expected Lab2 EDF behavior
