# Lab2 實作紀錄

## 概述

在 uC/OS-II 上實作 EDF（Earliest Deadline First）排程器，取代原本的 fixed-priority scheduling。這份 Lab2 延續 Lab1 的週期性任務模型、事件紀錄（`Preempt` / `Complete`）與 deadline miss 偵測，但把「誰先執行」的判斷，從固定 priority 改成「ready task 中 absolute deadline 最小者優先」。

目前程式支援兩組測試案例：

- Set1：`t1(1,3)`, `t2(3,5)`
- Set2：`t1(1,4)`, `t2(2,5)`, `t3(2,10)`

## 修改檔案

### 1. `SOFTWARE/uCOS-II/SOURCE/uCOS_II.H`

**目的：** 在 `OS_TCB` 中加入 EDF 需要的 deadline 欄位。

**修改內容：**
- 在 `struct os_tcb` 新增：
  - `OSTCBDeadline` (`INT32U`)：目前 job 的 absolute deadline

**補充：**
- `OSTCBPeriod`、`OSTCBCompTime`、`OSTCBCompTimeMax`、`OSTCBIsPeriodic` 是 Lab1 已有欄位，Lab2 繼續沿用
- 這樣每個 task 就同時有：
  - 本 period 還剩多少計算量
  - 週期多長
  - 下一次 deadline 在哪個 tick

### 2. `SOFTWARE/uCOS-II/SOURCE/OS_CORE.C`

**目的：** 把核心排程邏輯從 bitmap highest-priority，改成 EDF linear search。

**修改內容：**

#### `OS_EDF_FindHighRdy()`

新增 EDF 搜尋函式：
- 從 `OSTCBList` 頭開始線性掃描所有 TCB
- 篩選條件：
  - `ptcb->OSTCBStat == OS_STAT_RDY`
  - 該 task 也必須真的存在於 ready bitmap 中：
    ```c
    (OSRdyTbl[ptcb->OSTCBY] & ptcb->OSTCBBitX) != 0
    ```
- 從所有 ready task 中找出 `OSTCBDeadline` 最小者
- 將該 task 的 priority 指派給 `OSPrioHighRdy`

**為什麼要同時檢查 `OSTCBStat` 和 ready bitmap：**
- 這份 Lab2 延續 Lab1 的 task model
- periodic task 完成後，會自己從 ready bitmap 移除，再呼叫 `OS_Sched()`
- 但它的 `OSTCBStat` 不一定會改掉
- 如果只看 `OSTCBStat`，EDF 可能誤選一個其實已經不在 ready list 的 task

#### `OS_Sched()`

**修改內容：**
- 把原本用 `OSUnMapTbl` 查最高 priority ready task 的邏輯，換成呼叫：
  ```c
  OS_EDF_FindHighRdy();
  ```
- 若 `OSPrioHighRdy != OSPrioCur`，就記錄一筆：
  - `Complete from to`

**角色：**
- task 自己完成本 period 計算、主動讓出 CPU 時，從這裡做 task-level context switch

#### `OSIntExit()`

**修改內容：**
- 中斷返回前，同樣改成呼叫：
  ```c
  OS_EDF_FindHighRdy();
  ```
- 若有更早 deadline 的 ready task，則觸發搶佔並記錄：
  - `Preempt from to`

**角色：**
- `OSTimeTick()` 負責在週期邊界釋放到期的 periodic task，將其重新加入 ready bitmap
- `OSIntExit()` 負責在中斷返回前重新執行排程判斷，並在必要時觸發搶佔
- 因此，本實作將「任務釋放」與「是否切換執行權」分別放在 `OSTimeTick()` 與 `OSIntExit()` 兩個階段處理

#### `OSStart()`

**修改內容：**
- 系統第一次啟動時，不再選固定 priority 最高者
- 改由 `OS_EDF_FindHighRdy()` 找到第一個要執行的 task

#### `OS_InitTaskIdle()`

**修改內容：**
- idle task 建立完成後，手動設定：
  ```c
  OSTCBPrioTbl[OS_IDLE_PRIO]->OSTCBDeadline = 0xFFFFFFFF;
  ```

**原因：**
- idle task 永遠 ready
- 若不把它的 deadline 設成最大值，linear search 可能在沒有工作時以外的情況誤選到它

#### `OS_TCBInit()`

**修改內容：**
- 新建 task 時，將：
  ```c
  ptcb->OSTCBDeadline = 0;
  ```
  作為初始化值

### 3. `SOFTWARE/uCOS-II/Lab2/BC45/SOURCE/OS_CFG.H`

**目的：** 對齊助教參考環境設定。

**修改內容：**
- `OS_LOWEST_PRIO = 12`
- `OS_MAX_TASKS = 10`
- `OS_TASK_STAT_EN = 0`
- `OS_TIME_GET_SET_EN = 1`
- `OS_SCHED_LOCK_EN = 1`

**說明：**
- 這份 Lab2 不是沿用 Lab1 的 `idle=63` 設定，而是回到助教參考環境常見的 `idle=12`
- 因此輸出裡 idle task 會顯示成 `12`

### 4. `SOFTWARE/uCOS-II/Lab2/BC45/SOURCE/INCLUDES.H`

**說明：** `INCLUDES.H` 未於 Lab2 中新增 EDF 專屬欄位或介面，本實作直接沿用 Lab1 建立的事件紀錄宣告，供 `OS_CORE.C` 與 `TEST.C` 共用。

**沿用內容：**
- `EVENT_COMPLETE`
- `EVENT_PREEMPT`
- `EVENT_ENTRY`
- `EventBuf[]`
- `EventCount`
- `SimDone`
- `DeadlineFlag`
- `DeadlineTime`
- `DeadlineTaskPrio`
- `LogEvent()`

### 5. `SOFTWARE/uCOS-II/Lab2/BC45/SOURCE/TEST.C`

**說明：** `TEST.C` 大致沿用 Lab1 的測試框架，包含 task 建立流程、事件輸出與模擬控制；Lab2 在此檔案中的主要新增內容，集中在 deadline 初始化與 job 完成後的 deadline 推進。

**修改內容：**

#### Taskset 與模擬長度

- `TASK_SET` 目前預設為 `2`
- `SIM_TICKS`：
  - Set1：`35`
  - Set2：`31`

這部分屬於測試與輸出配置，主要用來讓模擬長度與參考 trace 較容易對齊。

#### `LogEvent()`

**說明：**
- `LogEvent()` 本身沿用 Lab1 的事件紀錄機制，Lab2 繼續使用它來輸出 EDF 排程下的 `Complete` / `Preempt` trace
- 本版本保留事件寫入緩衝區、記錄上限控制與 `SimDone` 停止條件
- 另外加入 `to == TASK_START_PRIO` 的過濾，避免將切換回 startup task 的事件印入輸出；因此輸出中仍可能保留 `from = 0` 的第一筆切換紀錄

#### `PrintEvents()`

**說明：**
- `PrintEvents()` 負責將事件緩衝區內容依序輸出，格式維持 Lab1 的 trace 形式：
  - `time Preempt from to`
  - `time Complete from to`
- 目前程式中仍保留 deadline miss 訊息輸出邏輯，作為沿用 Lab1 模擬框架的相容設計；但 Lab2 的核心驗證重點仍是 EDF 排程事件順序與搶佔行為

#### `TaskStart()`

**Lab2 相關內容：**
- 延續 Lab1 的 task 建立流程，並在 task 建立完成後補上各自的初始 `OSTCBDeadline`
- `OSTCBPeriod` 與 `OSTCBDeadline` 一起決定 EDF 比較所需的初始時間參數

**關鍵做法：**
- 用 `OSSchedLock()` / `OSSchedUnlock()` 把整段 task 建立包起來

**原因：**
- 若 task 一建立就被排程出去，可能在欄位尚未填完整前就先執行
- 尤其 `OSTCBDeadline` 預設是 `0`，如果不鎖排程，EDF 會把剛建立好的 task 當成超急任務立刻切走

**建立完成後額外做兩件事：**
- `OSTimeSet(0)`：把時間重設為 0，消除建立 task 期間可能發生的 spurious tick
- `OSTimeDly(SIM_TICKS)`：讓 startup task 睡眠，正式開始模擬

#### `PeriodicTask()`

**Lab2 相關內容：**
- 延續 Lab1 的 busy-wait periodic task 模型：
  ```c
  while (OSTCBCur->OSTCBCompTime > 0) { }
  ```
- 當本 period 算完後，新增：
  1. `OSTCBDeadline += OSTCBPeriod`
  2. 把自己從 ready bitmap 移除
  3. 呼叫 `OS_Sched()` 讓出 CPU

**這裡是 Lab2 在應用層最核心的改動：**
- 每完成一個 job，就把 absolute deadline 往下一個 period 推進，使下一次 EDF 比較對應到新的 job deadline

## EDF 排程模型

這份程式的實際執行模型可以整理成下面 4 步：

1. `TaskStart()` 建立所有週期性任務，並為每個 task 設定第一個 absolute deadline
2. task 執行時，`OSTimeTick()` 每個 tick 遞減 running task 的 `CompTime`
3. task 的 `CompTime` 歸零後，自己先做：
   - `deadline += period`
   - 從 ready bitmap 移除
   - 呼叫 `OS_Sched()`
4. 下一次 period 到達時，`OSTimeTick()` 把 task 放回 ready bitmap；中斷返回時 `OSIntExit()` 再用 EDF 決定是否搶佔目前 task

## 事件產生點

- **Complete**
  - 位置：`OS_Sched()`
  - 時機：task 完成本 period 計算後主動讓出 CPU

- **Preempt**
  - 位置：`OSIntExit()`
  - 時機：tick 中斷後發現有更早 deadline 的 ready task，於 ISR 返回前切換

## Deadline 偵測

- 沿用 Lab1 的 deadline miss 偵測方式，位置在 `OSTimeTick()`
- 若 task 到達自己的 period boundary 時，`OSTCBCompTime > 0`
  - 表示上一個 job 還沒完成
  - 設定：
    - `DeadlineFlag`
    - `DeadlineTime`
    - `DeadlineTaskPrio`

## 補充說明

- `OSTimeTick()` 未於 Lab2 中新增修改；本實作直接沿用 Lab1 已建立的時間推進、job release 與 deadline miss 偵測機制
- Lab2 的主要變更集中在 `OSStart()`、`OS_Sched()`、`OSIntExit()` 與 `OS_EDF_FindHighRdy()` 所構成的 EDF 排程決策流程
- 因此，`OSTimeTick()` 的作用是將到期 task 重新置為 ready，而非直接執行 EDF 任務選擇

## 遇到的問題與修正

### 問題 1：Idle task 會被 EDF 誤選

**症狀：**
- 系統可能在不該 idle 的時候切到 idle task
- 或 linear search 沒有正確把 idle 當成最後順位

**原因：**
- idle task 永遠 ready
- 若不特別處理 deadline，它也會參與 EDF 比較

**修正：**
- 在 `OS_InitTaskIdle()` 把 idle task deadline 設成 `0xFFFFFFFF`

### 問題 2：只看 `OSTCBStat` 會誤選 fake-ready task

**症狀：**
- task 明明已經完成本輪、也從 ready bitmap 移除了，EDF 還可能選到它

**原因：**
- 這份程式的 periodic task 完成後，是直接把自己從 ready bitmap 清掉
- 但 `OSTCBStat` 仍可能維持 `OS_STAT_RDY`

**修正：**
- `OS_EDF_FindHighRdy()` 同時檢查：
  - `OSTCBStat == OS_STAT_RDY`
  - ready bitmap 也有對應 bit

### 問題 3：task 建立期間發生提早切換

**症狀：**
- 在 startup task 還沒把 `CompTime`、`Period`、`Deadline` 填完整前，新 task 就先被排程出去
- 造成 t=0 出現一串異常切換，甚至 compTime 還沒設好就先完成

**原因：**
- `OSTaskCreate()` 後 task 會先進 ready list
- 而 `OSTCBDeadline` 預設值是 `0`
- 對 EDF 而言，deadline=0 代表非常急

**修正：**
- 在 `TaskStart()` 建立 tasks 的區段前後加上：
  - `OSSchedLock()`
  - `OSSchedUnlock()`

### 問題 4：task 建立期間可能先吃到多餘 tick

**症狀：**
- 排程邊界整體偏移
- 導致事件時間點與推導不一致

**原因：**
- `PC_SetTickRate()` 啟動 timer 後，可能在 task 初始化完成前先發生 tick

**修正：**
- 所有 periodic task 建好後，呼叫：
  ```c
  OSTimeSet(0);
  ```
  把時鐘重設回 0

### 問題 5：tie-breaking 與參考輸出不完全一致

**現象：**
- 若兩個 task deadline 相同，實際誰先被選到，會受 `OSTCBList` 掃描順序影響

**這份實作的行為：**
- `OS_EDF_FindHighRdy()` 只有在 `< min_deadline` 時才更新最佳任務
- 因此相同 deadline 時採用 first-found wins

**說法：**
- 這符合 `SPEC.md` 中「tie-breaking 取決於 linear search 順序」的描述

## 驗證重點

### Set1

- t=0：task1 先執行（deadline 3 比 task2 的 5 小）
- t=6：task1 新 job 到達，`d=9`，會搶佔 task2 的 `d=10`

### Set2

- t=4：task1 的 `d=8`，會搶佔 task3 的 `d=10`
- t=9：沒有 ready task，回到 idle
- t=10：task2 的 `d=15` 最早，從 idle 被切回來

## DOSBox 編譯與驗證步驟

1. 掛載 Lab2 code：
   ```bat
   mount c c:\NYCU\embedded_real_time\RTOS_LAB\RTOS_Lab2\Lab2_code
   c:
   ```

2. 進入測試目錄並編譯：
   ```bat
   cd \SOFTWARE\uCOS-II\Lab2\BC45\TEST
   del ..\OBJ\*.OBJ
   MAKETEST.BAT
   ```

3. 執行目前預設的 Set2：
   ```bat
   TEST.EXE
   ```

4. 若要驗證 Set1，將 `TEST.C` 內：
   ```c
   #define TASK_SET 2
   ```
   改成：
   ```c
   #define TASK_SET 1
   ```
   然後重新編譯執行。

## DOSBox 腳本使用方式

- `RTOS_Lab2/dosbox/` 內提供多個 DOSBox 設定檔，可用來自動掛載專案目錄並執行測試流程
- 這些 `.conf` 檔的用途是將 `[autoexec]` 中的 DOS 指令自動化，例如：
  - `mount c ...`
  - `c:`
  - `cd \SOFTWARE\uCOS-II\Lab2\BC45\TEST`
  - `CALL MAKETEST.BAT`
  - `TEST.EXE`
- 可直接在 Windows PowerShell 中使用：
  ```powershell
  & "C:\Program Files (x86)\DOSBox-0.74-3\DOSBox.exe" -conf "C:\NYCU\embedded_real_time\RTOS_LAB\RTOS_Lab2\dosbox\dosbox_lab2_test.conf"
  ```
- 若要執行其他腳本，只需替換最後的 `.conf` 路徑，例如：
  - `dosbox_auto_set1.conf`
  - `dosbox_auto_set2.conf`
- `dosbox_auto_set1.conf` / `dosbox_auto_set2.conf` 只負責自動進入測試目錄、重編並執行；實際執行的 task set 仍取決於 `TEST.C` 中目前的 `TASK_SET` 定義


