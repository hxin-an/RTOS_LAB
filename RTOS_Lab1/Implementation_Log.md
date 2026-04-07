# Lab1 實作紀錄

## 概述

在 uC/OS-II 上實作 RMS（Rate Monotonic Scheduling），包含兩組任務（Set1、Set2）、事件紀錄（Preempt/Complete）與 deadline 偵測。

## 修改檔案

### 1. `SOFTWARE/uCOS-II/SOURCE/uCOS_II.H`

**目的：** 在 OS_TCB 中加入週期性任務所需欄位。

**修改內容：**
- 在 `struct os_tcb` 新增 4 個欄位：
  - `OSTCBCompTime` (INT16U)：目前 period 的剩餘計算時間
  - `OSTCBCompTimeMax` (INT16U)：每個 period 的最大計算時間（c）
  - `OSTCBPeriod` (INT16U)：週期（tick 單位，p）
  - `OSTCBIsPeriodic` (BOOLEAN)：區分週期性任務與系統任務

### 2. `SOFTWARE/uCOS-II/EX1_x86L/BC45/SOURCE/OS_CFG.H`

**目的：** 依 Lab1 需求調整 uC/OS-II 設定。

**修改內容：**
- `OS_LOWEST_PRIO` 從 12 改為 **63**（Idletask 必須使用優先權 63）
- `OS_MAX_TASKS` 從 11 改為 **10**
- `OS_TASK_STAT_EN` 從 1 改為 **0**（停用不需要的 stat task）

### 3. `SOFTWARE/uCOS-II/EX1_x86L/BC45/SOURCE/INCLUDES.H`

**目的：** 提供所有 source file 可共用的事件紀錄介面宣告。

**修改內容：**
- 新增 `EVENT_COMPLETE`、`EVENT_PREEMPT` 常數
- 新增 `EVENT_ENTRY` 結構（time、event、from、to）
- 新增 extern 宣告：`EventBuf[]`、`EventCount`、`SimDone`、`DeadlineFlag`、`DeadlineTime`、`DeadlineTaskPrio`
- 新增 `LogEvent()` 函式 extern 宣告

### 4. `SOFTWARE/uCOS-II/SOURCE/OS_CORE.C`

**目的：** 在核心中加入事件紀錄插樁與週期性任務支援。

**`OSTimeTick()` 修改：**
- 對目前執行中的週期性任務遞減 `OSTCBCur->OSTCBCompTime`
- 檢查所有週期性任務是否到達 period 邊界（`OSTime % period == 0`）
- 到達 period 邊界時重設 compTime，並把任務設為 ready
- 若在 period 邊界時 `compTime > 0`，判定 deadline miss 並設定 `DeadlineFlag`

**`OSIntExit()` 修改：**
- 在中斷結束切換前呼叫 `LogEvent()` 記錄 `EVENT_PREEMPT`
- 記錄完成後檢查 `DeadlineFlag`，若偵測到 deadline miss 則設定 `SimDone`

**`OS_Sched()` 修改：**
- 在 task-level context switch 前呼叫 `LogEvent()` 記錄 `EVENT_COMPLETE`

**`OS_TCBInit()` 修改：**
- 新建任務時，將新增欄位初始化為 0/FALSE

### 5. `SOFTWARE/uCOS-II/EX1_x86L/BC45/SOURCE/TEST.C`

**目的：** 應用層邏輯：週期任務、事件緩衝區、輸出流程。

**設計重點：**
- 透過 `TASK_SET` 巨集選擇 Set1（1）或 Set2（2）
- 啟動任務（prio 0）：建立週期任務、設定 tick ISR、等待模擬結束、輸出結果
- `PeriodicTask()`：當 `compTime > 0` 時忙等；完成後從 ready list 移除自己並呼叫 `OS_Sched`（觸發 Complete 事件）
- `LogEvent()`：事件寫入共享緩衝區；過濾 startup task（prio 0）；Set1 到 `MAX_PRINT_EVENTS` 即停止記錄
- `PrintEvents()`：模擬結束後印出所有事件；若有 deadline flag 則附加 deadline 訊息
- 預先塞入第一筆事件：`0 Complete Idletask(63) task1(1)`（系統啟動）

## Periodic Task 模型

1. 每個 period 起點將 compTime 重設為 c（於 `OSTimeTick` 的絕對邊界判定）
2. 每個 tick 對執行中的任務 compTime 減 1（於 `OSTimeTick`）
3. compTime 降至 0 後，任務自行從 ready list 移除並呼叫 `OS_Sched`（產生 Complete 事件）
4. period 邊界採絕對檢查：`OSTime % period == 0`（避免漂移累積）

## 事件產生點

- **Complete**：任務完成本 period 計算並主動讓出 CPU 時，在 `OS_Sched()` 記錄
- **Preempt**：tick ISR 造成切換到更高優先權任務時，在 `OSIntExit()` 記錄

## Deadline 偵測

- 在 `OSTimeTick()` 中，若任務到達自身 period 邊界時 `compTime > 0`，代表該任務 deadline miss
- 透過全域 `DeadlineFlag` 記錄時間與任務資訊
- 在該 tick 的 preempt 事件記錄完成後，設定 `SimDone` 停止後續記錄

## 遇到的問題與修正

### 問題 1：輸出後程式立刻結束

**症狀：** `TEST.EXE` 印完結果後立刻回 DOS，畫面來不及閱讀。

**原因：** `TaskStart` 在 `PrintEvents()` 後返回 `main()`，導致 `PC_DOSReturn()` 直接被呼叫。

**修正：** 在 `PrintEvents()` 後加入 ESC 等待迴圈：
```c
INT16S key;
for (;;) {
    if (PC_GetKey(&key) == TRUE) {
        if (key == 0x1B) break;
    }
    OSTimeDly(1);
}
PC_DOSReturn();
```
另外避免印 `printf("Press ESC to exit...")`，因為額外那行會把重要輸出擠出螢幕。

---

### 問題 2：Set2 缺少 task3 相關事件（task2→task3、task3→task1）

**症狀：** Set2 只印到 `6 Preempt task3(3) task1(1)`，接著立刻出現 `time:6 task3 exceed deadline`，缺少 task3 計算與被 task1 搶占的完整鏈。

**原因：** `PC_SetTickRate()` 會在所有週期任務建立完成前就啟動硬體 timer 中斷，造成任務建立期間出現一次多餘 tick（消耗 OSTime 的 tick 1），使 period 邊界整體偏移 +1。結果是 task2 的 compTime 在 tick 6（其邊界）恰好變成 0，但 `OSTimeTick` 先把 compTime 重設回 3，`OS_Sched` 還沒機會執行，導致 task2 沒有主動讓出 CPU，排程鏈被打斷。

**修正：** 在所有任務建立後加上 `OSTimeSet(0)`，重設時鐘消除偏移：
```c
/* Reset clock to 0 now that all tasks are created and ready */
OSTimeSet(0);
```

---

### 問題 3：Set1 出現不該有的 `time:33 task1 exceed deadline`

**症狀：** Set1 印完預期 23 行事件後，還多出 `time:33 task1 exceed deadline`。

**根因（兩部分）：**

1. **舊物件檔未更新：** `TEST.MAK` 沒有把 `os_core.c`、`os_time.c` 列為 `uCOS_II.OBJ` 依賴。核心檔修改後，MAKE 沒重編，導致 deadline 判斷中的 `!SimDone` 防護其實沒有進到執行檔。

2. **模擬結束後的假性 deadline：** `SimDone=1` 後停止事件記錄，但 `TaskStart`（prio 0）從 `OSTimeDly(SIM_TICKS)` 醒來後會搶占週期任務。週期任務在下一個邊界前無法完成，於是在 tick 33（task1 第 11 個週期）觸發 deadline 判定，這是模擬視窗之外的假陽性。

**修正方式：**
1. 在 `TEST.MAK` 的 `$(OBJ)\uCOS_II.OBJ` 依賴清單加入 `$(OS)\os_core.c`、`$(OS)\os_time.c`，強制核心更新時重編。
2. 在 `OSTimeTick` 的 deadline 判斷加上 `!SimDone` 防護：
   ```c
   if (!SimDone && ptcb->OSTCBCompTime > 0) { /* Deadline miss */ ... }
   ```
3. 在 `PrintEvents()` 的 deadline 輸出處加 `#if TASK_SET == 2`，確保 Set1 不會印 deadline 訊息。

**修正後需 clean rebuild：**
```
del ..\OBJ\*.OBJ
MAKETEST.BAT
TEST.EXE
```


---

## DOSBox 編譯與驗證步驟

1. 在 DOSBox 將 lab1_code 掛載為 C 槽：
   ```
   mount c c:\NYCU\RTOS_Lab1\lab1_code
   c:
   ```

2. 編譯：
   ```
   cd \SOFTWARE\uCOS-II\EX1_x86L\BC45\TEST
   MAKETEST.BAT
   ```

3. 執行 Set1（`TEST.C` 預設 `TASK_SET=1`）：
   ```
   cd \SOFTWARE\uCOS-II\EX1_x86L\BC45\TEST
   TEST.EXE
   ```

4. 驗證 Set2：將 `TEST.C` 的 `#define TASK_SET 1` 改為 `#define TASK_SET 2`，重新編譯後再執行。

