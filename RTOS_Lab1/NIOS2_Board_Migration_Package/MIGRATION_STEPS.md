# Nios II Board Migration Package

This folder is prepared for direct migration to a Nios II Eclipse project.

## Folder Structure

- to_test/hello_ucosii.c
- to_test_bsp/UCOSII/inc/ucos_ii.h
- to_test_bsp/UCOSII/inc/os_cfg.h
- to_test_bsp/UCOSII/src/os_core.c
- reference/TEST.C
- reference/INCLUDES.H

## Copy Targets in Eclipse Workspace

1. Application project (test)
   - Copy to_test/hello_ucosii.c to test/hello_ucosii.c

2. BSP project (test_bsp)
   - Copy to_test_bsp/UCOSII/inc/ucos_ii.h to test_bsp/UCOSII/inc/ucos_ii.h
   - Copy to_test_bsp/UCOSII/inc/os_cfg.h to test_bsp/UCOSII/inc/os_cfg.h
   - Copy to_test_bsp/UCOSII/src/os_core.c to test_bsp/UCOSII/src/os_core.c

## Build Order

1. Right click test_bsp -> Nios II -> Generate BSP
2. Build test_bsp
3. Build test
4. Run As -> Nios II Hardware

## Expected Runtime Output

Set1 (TASK_SET 1):
- Ends after 23 event lines.

Set2 (TASK_SET 2):
- Ends at: time:9 task3 exceed deadline

## Notes

- The application file is DOS-free and intended for Nios II console output.
- If your template does not provide includes.h, include ucos_ii.h and needed BSP headers directly.
- Keep printf out of ISR. Logging is done through LogEvent and printed from task context.
- reference/ contains original DOSBox source for cross-checking only.
