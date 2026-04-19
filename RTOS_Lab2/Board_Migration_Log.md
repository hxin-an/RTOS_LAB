# Board Migration Log (Lab2 / Nios II / uC-OSII)

## Goal

Migrate the Lab2 EDF implementation from the DOSBox validation version to the same Nios II board and BSP structure already used in Lab1.

## Migration Basis

- Board: same as Lab1
- BSP structure: same as Lab1
- Migration method: reuse the final Lab1 board version as the base, then apply only the Lab2 EDF differences

This avoids copying the DOSBox source tree directly into the BSP.

## Source Mapping

### Application side

- DOSBox source:
  - `RTOS_Lab2/Lab2_code/SOFTWARE/uCOS-II/Lab2/BC45/SOURCE/TEST.C`
- Board target:
  - `RTOS_Lab2/NIOS2_Board_Migration_Package/to_test/hello_ucosii.c`

### BSP side

- DOSBox source:
  - `RTOS_Lab2/Lab2_code/SOFTWARE/uCOS-II/SOURCE/uCOS_II.H`
- Board target:
  - `RTOS_Lab2/NIOS2_Board_Migration_Package/to_test_bsp/UCOSII/inc/ucos_ii.h`

- DOSBox source:
  - `RTOS_Lab2/Lab2_code/SOFTWARE/uCOS-II/SOURCE/OS_CORE.C`
- Board target:
  - `RTOS_Lab2/NIOS2_Board_Migration_Package/to_test_bsp/UCOSII/src/os_core.c`

- BSP config:
  - `RTOS_Lab2/NIOS2_Board_Migration_Package/to_test_bsp/UCOSII/inc/os_cfg.h`
  - carried forward from the Lab1 board solution

## Lab2 Diffs Applied

### `hello_ucosii.c`

- Switched task parameters to the Lab2 task sets:
  - Set1: `t1(1,3)`, `t2(3,5)`
  - Set2: `t1(1,4)`, `t2(2,5)`, `t3(2,10)`
- Added initial `OSTCBDeadline` setup after each task is created
- Added `deadline += period` when a periodic job completes
- Kept the Lab1 board-side bridge/filter logic so hidden BSP task hops do not pollute the trace
- Changed trace printing to numeric `from/to` values to match the Lab2 DOSBox output style
- Kept `OSSchedLock()` and `OSTimeSet(0u)` so task creation does not trigger premature EDF decisions

### Trace Bridge Fixes After First Board Run

- First board validation showed that the Lab1 bridge logic was too strict for Lab2:
  - `set2` could miss a valid transition such as `20 Complete 12 1`
  - root cause: board execution could produce `tracked -> hidden -> tracked` within the same tick, but the two halves did not always arrive with the same event type
  - fix: when consuming a saved bridge on the `hidden -> tracked` half, merge by `time` and reuse the saved first-half event type

- After that relaxation, `set1` exposed the opposite problem:
  - a false self-transition such as `20 Preempt 1 1` could appear
  - root cause: the relaxed rule could rewrite an already-valid `tracked -> tracked` event using stale bridge state
  - fix: only consume bridge state on the `hidden -> tracked` path, clear bridge state immediately on direct `tracked -> tracked` events, and drop any merged event where `from == to`

- Final board-side trace bridge rules in `hello_ucosii.c` are therefore:
  - direct `tracked -> tracked`: log directly, and clear any pending bridge
  - `tracked -> hidden`: save bridge context and wait
  - `hidden -> tracked`: restore the saved `from` and `event` if the tick matches
  - self-transitions: discard

### `ucos_ii.h`

- Added `INT32U OSTCBDeadline` to `OS_TCB`
- Kept all Lab1 periodic-task fields and shared event logging declarations

### `os_core.c`

- Added `OS_EDF_FindHighRdy()`
- Replaced fixed-priority ready selection with EDF in:
  - `OSStart()`
  - `OS_Sched()`
  - `OSIntExit()`
- Kept the Lab1 board-side `OSTimeTick()` periodic release and deadline-miss framework
- Added `OSTCBDeadline = 0u` in `OS_TCBInit()`
- Set the idle task deadline to `0xFFFFFFFFu` in `OS_InitTaskIdle()`

## Why This Is Diff-Based

Lab1 already solved the board-specific problems that do not exist in the DOSBox environment, especially:

- hidden BSP/background tasks entering the scheduling trace
- board-side task hops that need bridge filtering
- timing alignment around simulation start
- keeping printing in task context instead of ISR context

Lab2 only changes the scheduling policy and deadline bookkeeping. The board-specific infrastructure from Lab1 is still required.

## Known Risks To Verify On Board

1. Hidden BSP tasks may still appear in the trace if the tracked-priority filter is incomplete.
2. The first startup transition should remain visible as `0 -> task`, while transitions back to startup are still filtered.
3. If any unexpected deadline-miss behavior appears on board but not in DOSBox, check:
   - tick timing alignment
   - whether the correct `TASK_SET` is selected
   - whether extra BSP tasks are slipping into the ready-set transitions
4. Idle-task selection should only happen when no periodic task is ready; if idle appears too early, re-check the idle deadline initialization.
5. If a line is missing from the trace, check the bridge rules before suspecting the EDF core. On board, most mismatches came from `LogEvent()` filtering rather than from `OS_EDF_FindHighRdy()`.

## Current Status

- A first Lab2 board migration package has been prepared at:
  - `RTOS_Lab2/NIOS2_Board_Migration_Package`
- This package is ready for copy-based migration into the Eclipse `test` / `test_bsp` workspace layout used in Lab1
- Board-side trace validation has been iterated to remove:
  - missing merged transitions such as `Complete 12 1`
  - false self-transitions such as `Preempt 1 1`
- Current status: both `set1` and `set2` traces are now aligned with the expected behavior after the `LogEvent()` bridge fixes
