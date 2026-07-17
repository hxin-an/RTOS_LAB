# Lab5 Runbook

This runbook reproduces the stable Lab5 demonstration through the Code Composer Studio (CCS) Debug Console.

## Configuration

- Project: `RTOSDemo_Lab5`
- Active configuration: `Large_Data`
- Target: `MSP430FR5994`
- Target configuration: `/home/rtos/workspace/RTOSDemo_Lab5/targetConfigs/MSP430FR5994.ccxml`
- Console: `RTOSDemo_Lab5:CIO`

## Restore the Source Snapshot

Copy the files from this repository into the matching locations in the course VM project:

```text
modified_files/main.c
  -> /home/rtos/workspace/RTOSDemo_Lab5/main.c

modified_files/main_blinky.c
  -> /home/rtos/workspace/RTOSDemo_Lab5/Blinky_Demo/main_blinky.c

modified_files/printf-stdarg.c
  -> /home/rtos/workspace/RTOSDemo_Lab5/printf-stdarg.c

modified_files/FreeRTOSConfig.h
  -> the FreeRTOSConfig.h used by RTOSDemo_Lab5

modified_files/lnk_msp430fr5994.cmd
  -> /home/rtos/workspace/RTOSDemo_Lab5/lnk_msp430fr5994.cmd
```

## Build

In CCS:

1. Open `RTOSDemo_Lab5`.
2. Select the `Large_Data` configuration.
3. Choose **Project > Build Project**.
4. Confirm the build finishes without errors.

Expected output image:

```text
/home/rtos/workspace/RTOSDemo_Lab5/Large_Data/RTOSDemo_Lab5.out
```

## Fresh-Boot Demo

1. Connect the Texas Instruments MSP debugger to the VM.
2. Select **Debug As > Code Composer Debug Session**.
3. Confirm CCS uses `MSP430FR5994.ccxml`.
4. Open the Console view and select `RTOSDemo_Lab5:CIO`.
5. Resume execution.

Expected output:

```text
Checkpoint Lab5
fresh boot
i:1
i:2
...
commit i:10
i:10
i:11
...
power_off i:14
```

After `power_off`, the application enters LPM4.5 and stops producing output.

## Restore Demo

1. Wait until the console prints `power_off i:...`.
2. Reset the board.
3. If CCS shows the CPU as suspended, resume execution.
4. Observe `RTOSDemo_Lab5:CIO`.

Expected output:

```text
Checkpoint Lab5
restore
restore i:10
i:10
i:11
...
```

The example resumes at `i:10` because that was the last committed checkpoint. Iterations after it had not yet been committed when the simulated power failure occurred.

## Starting From a Clean State

To force a fresh boot, either clear the `.checkpoint` FRAM section or change `mainCHECKPOINT_VERSION` so the existing slots fail validation.

Do this before recording a fresh-boot demonstration. Keep the checkpoint section intact when recording the restore demonstration.

## Troubleshooting

- If CCS reports that an MSP430FR5969 target does not match the MSP430FR5994 board, cancel the session and recreate the debug configuration with `MSP430FR5994.ccxml`.
- Confirm `.ccsproject` uses `deviceVariant=MSP430FR5994` and the `lnk_msp430fr5994` linker command file.
- Do not continue after accepting a mismatched target configuration.
- If the console is blank after reset, check whether the CPU is suspended and resume it.
- Select the CIO console rather than the CDT Build Console for application output.
