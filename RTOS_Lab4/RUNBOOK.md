# Lab4 Runbook

這份文件記錄如何把 `modified_files/` 裡的 Lab4 程式放回課程 VM，並在 Code Composer Studio 中重新 build、flash、執行。

## 複製修改檔案到 VM 專案

將本 repo 的 verified source files 複製到 VM 內對應位置：

```text
modified_files/main.c
  -> /home/rtos/workspace/RTOSDemo/main.c

modified_files/main_blinky.c
  -> /home/rtos/workspace/RTOSDemo/Blinky_Demo/main_blinky.c

modified_files/printf-stdarg.c
  -> /home/rtos/workspace/RTOSDemo/printf-stdarg.c

modified_files/task.h
  -> /home/rtos/Downloads/FreeRTOS/FreeRTOS/Source/include/task.h

modified_files/tasks.c
  -> /home/rtos/Downloads/FreeRTOS/FreeRTOS/Source/tasks.c
```

## Build

在 Code Composer Studio 中：

1. 開啟 `RTOSDemo` project。
2. 確認 active configuration 是 `Large_Data`。
3. 選擇 `Project -> Build Project`。
4. 確認 build console 沒有 error。

也可以在 VM terminal 使用：

```sh
cd /home/rtos/workspace/RTOSDemo/Large_Data
/home/rtos/ti/ccs1010/ccs/utils/bin/gmake clean all
```

## 在 CCS 執行

1. 將 MSP430 board/debugger 接到 VM。
2. 在 VMware 確認 `Texas Instruments MSP Tools Driver` 已連到 guest。
3. 在 CCS 按 `Debug`。
4. 等 CCS 停在 `main()` 附近的 hardware breakpoint。
5. 開啟 `Console` view，並用 console selector 選 `RTOSDemo:CIO`。
6. 按 `Resume` / `Continue` / `F8` 開始執行。

不要用 CDT Build Console 看 Lab 輸出。Lab 的 `printf()` 結果會出現在 `RTOSDemo:CIO` console。

## 切換 Task Set

打開 VM 內：

```text
/home/rtos/workspace/RTOSDemo/Blinky_Demo/main_blinky.c
```

修改：

```c
#define mainLAB4_TASK_SET             1
```

設定值：

```text
1 = task set 1
2 = task set 2
```

修改後需要重新 build，並重新 debug/flash 到板子。只按 Resume 不會把新的 C code 燒進 MSP430。

## 預期輸出開頭

Task set 1 開頭：

```text
T1 c:1 p:3
T2 c:3 p:5
1 complete T1 T2
4 complete T2 T1
```

Task set 2 開頭：

```text
T1 c:1 p:4
T2 c:2 p:5
T3 c:2 p:10
1 complete T1 T2
3 complete T2 T3
```

完整實作說明見：

```text
Implementation_Log.md
```
