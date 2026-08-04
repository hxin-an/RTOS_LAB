# Lab4 實作紀錄：EDF Scheduler

這份文件整理 Lab4 每個修改檔案的責任分工。

Lab4 的主軸是：保留 FreeRTOS 原本的 task、ready list、context switch 架構，但在 scheduler 選下一個 task 時，改成用 EDF，也就是選 absolute deadline 最早的 ready task。

## 整體流程

```text
main.c
  選擇進入 main_blinky()

main_blinky.c
  建立 Lab4 workload，並記錄/輸出 console 事件

task.h
  對 application 公開 Lab4 EDF helper APIs

tasks.c
  在 TCB 裡儲存 EDF metadata，並用 deadline 選下一個 task

printf-stdarg.c
  支援 CCS CIO console 的 printf 輸出
```

## modified_files/main.c

檔案角色：

`main.c` 是 application 的進入點。它負責初始化 MSP430 硬體，並選擇要執行哪一個 FreeRTOS demo 程式。

修改內容：

- 將 `mainCREATE_SIMPLE_BLINKY_DEMO_ONLY` 設為 `1`。
- 因此程式會呼叫 `main_blinky()`，而不是 `main_full()`。

為什麼需要：

原本 FreeRTOS demo 有兩條 application path：

```text
main_blinky()  simple demo
main_full()    full demo
```

Lab4 使用 simple demo 這條路，因為它比較小，也比較適合替換成 EDF workload。`main.c` 本身不實作 EDF，它只負責把程式導向 `main_blinky()`。

## modified_files/main_blinky.c

檔案角色：

`main_blinky.c` 是 Lab4 的 application layer。它負責定義 task set、建立 FreeRTOS tasks、模擬每個 periodic job、記錄 scheduler events，最後印出 console output。

修改內容：

- 將原本 blinky demo 的行為替換成 Lab4 EDF 測試程式。
- 新增 `mainLAB4_TASK_SET`，用來切換投影片中的兩組 task set。
- 新增 `Lab4TaskConfig_t`，用來儲存每個 task 的 task number、execution time、period。
- 新增 `Lab4Event_t` 和 `xEventLog[]`，用來儲存 `complete` 和 `preempt` 事件。
- 使用 `xTaskCreate()` 建立 T1/T2/T3。
- 每建立一個 task 後，呼叫 `vTaskSetEDFParameters()`，讓 kernel 把 EDF metadata 存進該 task 的 TCB。
- 新增 `prvEDFTask()`，作為 T1/T2/T3 共用的 task body。
- 使用 `vTaskDelayUntil()` 實作 periodic release，避免使用相對 delay 造成 period drift。
- 新增 `vApplicationEDFPreemptHook()` 和 `vApplicationEDFCompleteHook()`，讓 `tasks.c` 可以把 scheduler event 回報給 application。
- 新增 `prvReporterTask()`，在 measurement window 結束後印出 event log。
- 新增 `fflush(NULL)`，確保 reporter task suspend 前，CCS CIO console 會把 buffered output 印出來。

重要行為：

每個 EDF task 會重複執行以下流程：

```text
開始一個 job
消耗 C ticks 的 CPU time
標記 job complete
將 absolute deadline 往後推一個 period
delay until 下一次 period release
```

對應的程式呼叫是：

```c
vTaskStartEDFJob( NULL );
prvBurnExecutionTick();
vTaskAdvanceEDFDeadline( NULL );
vTaskDelayUntil( &xLastWakeTime, pxConfig->xPeriod );
```

其中 `NULL` 表示目前正在執行的 task。

為什麼需要：

Scheduler 需要真的 task 來排程。`main_blinky.c` 提供 Lab4 workload：

```text
execution time C  -> 用 busy-wait C ticks 模擬
period P          -> 用 vTaskDelayUntil() 控制
deadline          -> 透過 EDF APIs 初始化與更新
```

這個檔案不負責決定下一個 task 是誰。它負責建立 workload，並記錄最後結果。

## modified_files/task.h

檔案角色：

`task.h` 是 FreeRTOS task API 的公開 header。Application 如果要呼叫 task 相關的 kernel function，就會 include 這個檔案。

修改內容：

新增三個 Lab4 EDF API 宣告：

```c
void vTaskSetEDFParameters( TaskHandle_t xTask,
                            UBaseType_t uxTaskNumber,
                            TickType_t xPeriod,
                            TickType_t xRelativeDeadline );

void vTaskStartEDFJob( TaskHandle_t xTask );

void vTaskAdvanceEDFDeadline( TaskHandle_t xTask );
```

為什麼需要：

`main_blinky.c` 不應該直接碰 kernel 內部的 private TCB fields。比較好的方式是透過 `task.h` 宣告公開 helper functions，再由 `tasks.c` 實作。

三個 API 的意義是：

- `vTaskSetEDFParameters()`：啟用某個 task 的 EDF metadata，並設定 task number、period、relative deadline。
- `vTaskStartEDFJob()`：標記目前 job 還沒完成。
- `vTaskAdvanceEDFDeadline()`：標記目前 job 已完成，並把 absolute deadline 往下一個 period 推進。

## modified_files/tasks.c

檔案角色：

`tasks.c` 是 FreeRTOS task kernel 的實作檔。它負責 TCB structure、ready lists、task creation internals，以及 context switch 時的 task selection。

修改內容：

在每個 TCB 裡新增 EDF metadata：

```c
BaseType_t xEDFEnabled;
UBaseType_t uxEDFTaskNumber;
TickType_t xEDFPeriod;
TickType_t xEDFRelativeDeadline;
TickType_t xEDFAbsoluteDeadline;
BaseType_t xEDFJobCompleted;
```

在 task 建立時初始化這些欄位：

```text
新建 task 預設不是 EDF task
vTaskSetEDFParameters() 之後才會讓 T1/T2/T3 啟用 EDF
```

實作 `task.h` 宣告的三個 EDF APIs：

```text
vTaskSetEDFParameters()
vTaskStartEDFJob()
vTaskAdvanceEDFDeadline()
```

新增 `prvSelectEarliestDeadlineTask()`：

```text
掃描所有 ready lists
忽略 non-EDF tasks
比較 xEDFAbsoluteDeadline
選出 absolute deadline 最早的 ready EDF task
把結果寫進 pxCurrentTCB
```

修改 `vTaskSwitchContext()`：

- 呼叫 `prvSelectEarliestDeadlineTask()`，不只依賴原本 FreeRTOS 的 priority selector。
- 檢查 outgoing task 是否已完成目前 EDF job。
- 呼叫 `vApplicationEDFCompleteHook()` 或 `vApplicationEDFPreemptHook()`，讓 application 可以記錄 console trace。

為什麼需要：

原本 FreeRTOS 是 priority-based scheduler。它通常會選 highest-priority ready task。Lab4 要實作 EDF，所以 scheduler 必須改成比較 deadline。

核心概念是：

```text
FreeRTOS 仍然使用 ready lists。
Lab4 改的是下一個 pxCurrentTCB 的選擇方式。
```

Ready lists 仍然告訴 kernel 哪些 task 是 ready。EDF selector 則掃描這些 ready tasks，並選出 absolute deadline 最小的 task。

## modified_files/printf-stdarg.c

檔案角色：

`printf-stdarg.c` 是 embedded demo 使用的小型 printf implementation。它提供 `printf()`、`sprintf()` 等格式化輸出功能，避免依賴完整 desktop C library。

修改內容：

- 保留 local lightweight printf implementation。
- 保持假的 `putchar` macro 被註解：

```c
/* #define putchar(c) c */
```

- 使用真正的 `putchar()` path，讓字元可以送到 CCS CIO console。
- 讓 `main_blinky.c` 和 reporter task 可以印出 Lab 需要的 exact console output。

為什麼需要：

Lab demo 主要看 console output。如果 `printf()` 沒有正確把字元送到 CCS CIO，即使 scheduler 本身是對的，助教也看不到 expected trace。

這個檔案不是 EDF scheduling 的一部分，只負責輸出。

## 責任分工總結

```text
main.c
  選擇 Lab4 demo entry

main_blinky.c
  建立 workload、模擬 periodic jobs、記錄並印出結果

task.h
  宣告 application 需要呼叫的 EDF APIs

tasks.c
  實作 EDF metadata、deadline-based task selection、event hooks

printf-stdarg.c
  支援 CCS CIO 的 printf output
```
