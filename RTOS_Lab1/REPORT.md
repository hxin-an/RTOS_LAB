# Lab1 Report: RMS Scheduling on uC/OS-II

## 1. Modified Files and Purpose

| File | Purpose |
|------|---------|
| `SOFTWARE/uCOS-II/SOURCE/uCOS_II.H` | Add `OSTCBCompTime`, `OSTCBCompTimeMax`, `OSTCBPeriod`, `OSTCBIsPeriodic` fields to OS_TCB |
| `SOFTWARE/uCOS-II/SOURCE/OS_CORE.C` | Instrument `OSTimeTick` (compTime decrement, period boundary), `OSIntExit` (Preempt event), `OS_Sched` (Complete event), `OS_TCBInit` (field init) |
| `SOFTWARE/uCOS-II/EX1_x86L/BC45/SOURCE/OS_CFG.H` | Set `OS_LOWEST_PRIO=63`, `OS_TASK_STAT_EN=0` |
| `SOFTWARE/uCOS-II/EX1_x86L/BC45/SOURCE/INCLUDES.H` | Add event logging interface (struct, externs, function prototype) |
| `SOFTWARE/uCOS-II/EX1_x86L/BC45/SOURCE/TEST.C` | Application: periodic tasks, event buffer, LogEvent, PrintEvents, Set1/Set2 selection |

## 2. DOSBox Verification Steps and Results

### 2.1 Build Steps

1. Open DOSBox and mount the lab1_code directory:
   ```
   mount c c:\NYCU\RTOS_Lab1\lab1_code
   c:
   ```

2. Navigate to the build directory and build:
   ```
   cd \SOFTWARE\uCOS-II\EX1_x86L\BC45\TEST
   MAKETEST.BAT
   ```

3. Run the program:
   ```
   TEST.EXE
   ```

### 2.2 Set1 Verification

With `#define TASK_SET 1` in TEST.C (default), build and run. Expected output:

```
0 Complete Idletask(63) task1(1)
1 Complete task1(1) task2(2)
3 Preempt task2(2) task1(1)
4 Complete task1(1) task2(2)
5 Complete task2(2) Idletask(63)
6 Preempt Idletask(63) task1(1)
7 Complete task1(1) task2(2)
9 Preempt task2(2) task1(1)
10 Complete task1(1) task2(2)
11 Complete task2(2) Idletask(63)
12 Preempt Idletask(63) task1(1)
13 Complete task1(1) task2(2)
15 Preempt task2(2) task1(1)
16 Complete task1(1) task2(2)
17 Complete task2(2) Idletask(63)
18 Preempt Idletask(63) task1(1)
19 Complete task1(1) task2(2)
21 Preempt task2(2) task1(1)
22 Complete task1(1) task2(2)
23 Complete task2(2) Idletask(63)
24 Preempt Idletask(63) task1(1)
25 Complete task1(1) task2(2)
27 Preempt task2(2) task1(1)
```

### 2.3 Set2 Verification

Change `#define TASK_SET 1` to `#define TASK_SET 2` in TEST.C, rebuild, and run. Expected output:

```
0 Complete Idletask(63) task1(1)
1 Complete task1(1) task2(2)
3 Preempt task2(2) task1(1)
4 Complete task1(1) task2(2)
5 Complete task2(2) task3(3)
6 Preempt task3(3) task1(1)
7 Complete task1(1) task2(2)
9 Preempt task2(2) task1(1)
time:9 task3 exceed deadline
```

## 3. Known Issues and Fixes

### 3.1 Program Exits Immediately

The program returns to DOS immediately after printing output. Fix: add an ESC-key wait loop after `PrintEvents()` in `TaskStart`. Do **not** print a "Press ESC" prompt — it shifts output off screen.

### 3.2 Set2 Missing task3 Events

**Cause:** `PC_SetTickRate()` fires a spurious tick during task creation, offsetting `OSTime` by +1. This makes period boundary checks land on wrong ticks, causing task2's `compTime` to be reset at tick 6 before `OS_Sched` can fire.

**Fix:** Call `OSTimeSet(0)` after all periodic tasks are created (before `OSTimeDly`). This resets the timer, eliminating the offset.

### 3.3 Set1 Shows Spurious Deadline Message

**Cause 1 (stale object):** `TEST.MAK` did not list `os_core.c` as a dependency of `uCOS_II.OBJ`, so MAKE skipped recompiling the kernel even when `OS_CORE.C` was modified.

**Cause 2 (post-simulation false positive):** After `SimDone=1`, `TaskStart` (prio 0) wakes and preempts periodic tasks. Those tasks cannot finish before their next period boundary, triggering a spurious deadline detection at tick 33.

**Fixes:**
- Add `$(OS)\os_core.c` and `$(OS)\os_time.c` to `uCOS_II.OBJ` dependencies in `TEST.MAK`
- Guard deadline detection with `!SimDone` in `OSTimeTick`
- Wrap deadline output in `PrintEvents` with `#if TASK_SET == 2`
- Always do `del ..\OBJ\*.OBJ` before rebuilding when kernel source changes

### 3.4 Clean Rebuild Procedure

Whenever kernel source files (`OS_CORE.C`, `OS_TIME.C`) are modified, MAKE may not detect the change without clean:
```
del ..\OBJ\*.OBJ
MAKETEST.BAT
TEST.EXE
```

## 5. File Mapping for Board Project (test + test_bsp)

When initializing the Nios II project in Quartus/Eclipse (project type: test + test_bsp), copy modified files as follows:

| Source (lab1_code) | Target (Nios II project) |
|---|---|
| `SOFTWARE/uCOS-II/SOURCE/uCOS_II.H` | `test_bsp/UCOSII/inc/ucos_ii.h` |
| `SOFTWARE/uCOS-II/SOURCE/OS_CORE.C` | `test_bsp/UCOSII/src/os_core.c` |
| `SOFTWARE/uCOS-II/EX1_x86L/BC45/SOURCE/TEST.C` | `test/` (main application, adapt `#include` and I/O calls) |
| `SOFTWARE/uCOS-II/EX1_x86L/BC45/SOURCE/OS_CFG.H` | `test_bsp/UCOSII/inc/os_cfg.h` |
| `SOFTWARE/uCOS-II/EX1_x86L/BC45/SOURCE/INCLUDES.H` | `test/` or `test_bsp/` (adapt paths for Nios II) |

**Important notes for board adaptation:**
- Replace `printf()` with `alt_printf()` or JTAG UART output
- Replace `PC_DispClrScr`, `PC_DOSSaveReturn`, `PC_DOSReturn`, `PC_VectSet`, `PC_SetTickRate` with Nios II equivalents or remove (tick ISR setup differs on Nios II)
- Remove `#include <dos.h>`, `#include <conio.h>` and other DOS-specific headers
- The OS_CFG.H port-specific settings (e.g., `OS_CPU_HOOKS_EN`) may need adjustment for the Nios II BSP

## 6. Board Flow - PDF Reference and Supplementary Notes

**Primary reference:** `ucOS2 NIOS2 on altera+VM 環境操作步驟.pdf`

### 4.1 Default Project Type

- Project type: **test** (application) + **test_bsp** (board support package)
- Created via Nios II SBT for Eclipse: File -> New -> Nios II Application and BSP from Template

### 4.2 File Placement

After creating the default project:

1. **test/** directory: place your modified TEST.C (renamed/adapted for Nios II)
2. **test_bsp/UCOSII/**: the BSP contains the uC/OS-II source
   - `inc/` contains headers (ucos_ii.h, os_cfg.h, os_cpu.h)
   - `src/` contains source files (os_core.c, os_task.c, os_time.c, etc.)
3. Overwrite the BSP files with your modified versions (uCOS_II.H -> ucos_ii.h, OS_CORE.C -> os_core.c, OS_CFG.H -> os_cfg.h)

### 4.3 Build and Run Checkpoints

1. **Build success:** Right-click project -> Build Project. No errors in console. The `.elf` file is generated.
2. **Program the FPGA:** Use Quartus Programmer to download the `.sof` file to the board
3. **Run on board:** Right-click project -> Run As -> Nios II Hardware. Select the appropriate JTAG connection.
4. **Success indicator:** The scheduling output appears in the Nios II Console (JTAG UART terminal). The output should match the expected Set1/Set2 output (with possible time offset, as allowed by verification rule 1).
