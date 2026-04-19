# Nios II Board Migration Package

This package is the Lab2 board-side migration set for the same Nios II board/BSP layout used in Lab1.

## Folder Structure

- `to_test/hello_ucosii.c`
- `to_test_bsp/UCOSII/inc/ucos_ii.h`
- `to_test_bsp/UCOSII/inc/os_cfg.h`
- `to_test_bsp/UCOSII/src/os_core.c`

## Migration Strategy

Use the Lab1 board version as the base, then apply only the Lab2 EDF differences:

- `ucos_ii.h`: add `OSTCBDeadline` to `OS_TCB`
- `os_core.c`: replace fixed-priority ready selection with `OS_EDF_FindHighRdy()` in:
  - `OSStart()`
  - `OS_Sched()`
  - `OSIntExit()`
- `os_core.c`: keep the Lab1 board-side `OSTimeTick()` periodic model
- `os_core.c`: initialize `OSTCBDeadline` in `OS_TCBInit()` and set idle deadline to max in `OS_InitTaskIdle()`
- `hello_ucosii.c`: initialize each task's first deadline and advance `deadline += period` after each completed job

## Copy Targets in Eclipse Workspace

1. Application project (`test`)
   - Copy `to_test/hello_ucosii.c` to `test/hello_ucosii.c`

2. BSP project (`test_bsp`)
   - Copy `to_test_bsp/UCOSII/inc/ucos_ii.h` to `test_bsp/UCOSII/inc/ucos_ii.h`
   - Copy `to_test_bsp/UCOSII/inc/os_cfg.h` to `test_bsp/UCOSII/inc/os_cfg.h`
   - Copy `to_test_bsp/UCOSII/src/os_core.c` to `test_bsp/UCOSII/src/os_core.c`

## Build Order

1. Right click `test_bsp` -> `Nios II` -> `Generate BSP`
2. Build `test_bsp`
3. Build `test`
4. Run As -> `Nios II Hardware`

## Runtime Notes

- `to_test/hello_ucosii.c` defaults to `TASK_SET 1`
- Switch between task sets by editing `TASK_SET` in `hello_ucosii.c`
- Output format is numeric to stay aligned with the Lab2 DOSBox trace:
  - `time Complete from to`
  - `time Preempt from to`
- The Lab1 board-side bridge/filter is still kept to suppress hidden BSP/background task hops

## Expected Behavior

- Set1 should follow the EDF trace pattern from the Lab2 DOSBox version
- Set2 should also follow EDF ordering; any unexpected extra lines usually indicate board-side background tasks were not filtered correctly

## Important Reminder

This package is intended for diff-based migration from the Lab1 board solution. Do not overwrite a fresh BSP with the DOSBox source tree directly.
