# Board Migration Log (Nios II / uC-OSII)

## 文件目的

這份文件專門記錄「從 DOSBox 版本移植到 Nios II 板子」過程中的困難、根因、修正方式與驗證結果。

---

## 環境與前提

- Workspace: `C:\NYCU\RTOS_Lab1`
- 目標專案型態: `test` (application) + `test_bsp` (BSP)
- 核心來源: DOSBox 可運作版本 (`Lab1_code`)
- 板端工具鏈: Nios II IDE (Eclipse) + BSP auto-generated from `.sopcinfo`

---

## 目前移植策略

1. 以 Nios II 原生 BSP 結構為主。
2. 僅移植 Lab1 必要差異（TCB 欄位、OS 核心插樁、應用層任務模型）。
3. 避免整包覆蓋 DOSBox 檔案到 BSP（會破壞 Nios 相容性）。
原因 : DOSBox 版本的核心檔與 Nios BSP 的核心檔在結構與設定上有差異，直接覆蓋會導致編譯錯誤與功能衝突。

---

## 檔案修正摘要

以下整理以「板子上原始程式碼」對照「完整改好的程式碼」為主，並直接標示修正加在什麼位置。

- `NIOS2_Board_Migration_Package/to_test_bsp/UCOSII/inc/ucos_ii.h`
  - 原始程式碼僅包含標準 uC/OS-II 的 TCB 與系統介面定義，尚未納入 Lab1 所需的週期任務模型。
  - 完整改好的程式碼新增下列欄位與介面：
    - `OSTCBCompTime`
    - `OSTCBCompTimeMax`
    - `OSTCBPeriod`
    - `OSTCBIsPeriodic`
    - `EVENT_COMPLETE`
    - `EVENT_PREEMPT`
    - `EVENT_ENTRY`
    - `EventBuf`
    - `EventCount`
    - `SimDone`
    - `DeadlineFlag`
    - `DeadlineTime`
    - `DeadlineTaskPrio`
    - `LogEvent()`
  - 加入位置：`OS_TCB` 結構區段，以及檔案後段的 Lab1 event logging interface 區塊。
  - 修改目的：擴充 TCB 資訊以支援週期任務控制，並建立核心與應用層共用的事件記錄介面。

- `NIOS2_Board_Migration_Package/to_test_bsp/UCOSII/src/os_core.c`
  - 原始程式碼角色：主要執行標準 uC/OS-II 核心流程（tick 更新、排程、ISR 結束切換），不包含 Lab1 的 RMS 週期控制與事件輸出規格。
  - DOS 版本本來就有的 Lab1 邏輯（概念上沿用）：
    - 在核心路徑中追蹤每個週期任務的剩餘 computation time。
    - 在週期邊界檢查 deadline miss，並輸出 miss 訊息。
    - 在排程關鍵點記錄 `Complete` / `Preempt` 事件。
  - 板子移植時因實際執行問題而補強或調整的部分：
    - 在 `OSTimeTick()` 加入週期任務條件檢查（以 `OSTCBIsPeriodic` 區分 Lab task 與背景 task），避免把 BSP 系統任務誤套用 RMS 計算。
    - 在 `OSIntExit()` 加入 `SimDone` 的結束保護，deadline 發生後停止後續事件污染，避免板端持續中斷造成額外輸出。
    - 在 `OS_Sched()` 只是調用 `LogEvent()` 記錄 Complete 事件；實際過濾邏輯（白名單、bridge 合併）在應用層的 `LogEvent()` 負責。
    - 在 `OS_TCBInit()` 明確初始化 Lab1 擴充欄位，避免板端動態建立 TCB 時保留未定值。
  - 具體加入位置：
    - `OSTimeTick()` 內 `ptcb = OSTCBList` 走訪迴圈附近，插入週期欄位遞減、period boundary 重設與 miss 判定區塊。
    - `OSIntExit()` 內 `if (OSIntNesting == 0)` 的可切換區段前後，插入 `Preempt` 記錄與 `SimDone` 終止條件。
    - `OS_Sched()` 內由 `OSPrioCur != OSPrioHighRdy` 觸發切換的分支，插入 `Complete` 記錄呼叫。
    - `OS_TCBInit()` 完成 TCB 基本欄位設定後，插入 Lab1 欄位初值。
  - 修改目的：在不破壞 BSP 核心架構的前提下，將 DOS 可用的 RMS 規則安全移植到板端，並修正板端特有的背景任務與持續中斷干擾。

- `NIOS2_Board_Migration_Package/to_test/hello_ucosii.c`
  - 原始程式碼角色：BSP 預設示範 application，重點是驗證系統可跑，未內建 Lab1 指定的 task set、事件格式與模擬結束條件。
  - DOS 版本本來就有的 Lab1 應用層內容（直接移植）：
    - 建立 `task1/task2/task3` 與對應 stack，形成實驗指定 task set。
    - 設定每個 task 的 computation time (`c`) 與 period (`p`)。
    - 建立事件緩衝區 `EventBuf`，並以 `LogEvent()` 收集、`PrintEvents()` 輸出。
    - 啟動前重設全域狀態（`SimDone`、`DeadlineFlag`、`EventCount` 等）。
  - 板子移植時因實際執行問題而補強或調整的部分：
    - 新增 `OSTCBIsPeriodic` 標記流程，只對 Lab task 啟用週期邏輯，隔離 BSP 系統任務。
    - 在 `LogEvent()` 加入追蹤優先權白名單（Set1: 1/2/63，Set2: 1/2/3/63），避免板端背景任務事件污染輸出。
    - 加入 bridge 合併機制，處理 `tracked -> untracked -> tracked` 的板端實際切換鏈，還原成作業要求的一筆事件。
    - 加入對 `OSTimeSet(0)` 的啟動時序校正，避免板端 timer 提前 tick 導致週期基準偏移。
  - 具體加入位置：
    - 全域變數區：加入 `EventBuf`、deadline 狀態、bridge 暫存狀態與 Lab task 參數。
    - `LogEvent()`：加入白名單過濾、bridge 合併與容量保護。
    - `PrintEvents()`：加入事件列印格式與 task 名稱對映。
    - `main()`：加入 task 建立參數、系統時間重設與模擬主流程。
    - `TaskStart()`：加入板端啟動後的狀態初始化與統一入口。
  - 修改目的：把 BSP 範例程式轉成可重現 Lab1 測資的板端測試主程式，並補上 DOS 環境不需要、但板端實機必須處理的事件過濾與時序修正。

- `NIOS2_Board_Migration_Package/to_test_bsp/UCOSII/inc/os_cfg.h`
  - 結論：此檔案在目前版本視為「無 Lab1 手動改動」。
  - 說明：沿用 BSP 產生的預設設定，不覆蓋 DOS 版 `OS_CFG.H`。
  - 補充：Lab1 功能改動集中在 `os_core.c`、`ucos_ii.h`、`hello_ucosii.c`。

---

## 問題記錄

### [BM-001] 直接覆蓋 BSP 核心檔導致編譯大量錯誤

- 現象:
  - `alt_exit.c` 出現 `OS_FALSE undeclared`
  - `os_cfg.h` 大量 `redefined` 警告
  - make 連鎖失敗（`make: *** ... Error 1`）
- 根因:
  - 把 DOSBox 版本的 `uCOS_II.H / OS_CFG.H / OS_CORE.C` 直接覆蓋到 Nios BSP。
  - Nios BSP 與 DOSBox 版設定/巨集來源不同，導致 HAL 與 OS 設定衝突。
- 修正:
  - 改成「Nios 原生檔為底 + 精準移植 Lab1 差異」：
    - 在 `ucos_ii.h` 增加 Lab1 週期欄位與 LogEvent 介面。
    - 在 `os_core.c` 只插入 `OSTimeTick / OSIntExit / OS_Sched / OS_TCBInit` 的 Lab1 邏輯。
    - 保留 Nios 版 `os_cfg.h` 結構（避免重新定義衝突）。

### [BM-002] 執行結果提早 deadline（`time:6 task1 exceed deadline`）

- 現象:
  - 輸出只到：
    - `0 Complete Idletask(63) task1(1)`
    - `1 Complete task1(1) task2(2)`
    - `3 Preempt task2(2) task1(1)`
    - `time:6 task1 exceed deadline`
  - 與目標 Set2 不符（應先出現 `4 Complete ...`、`5 Complete ...`、`6 Preempt ...`）。
- 根因:
  - `PeriodicTask` 用 busy-wait 直接讀 `OSTCBCur->OSTCBCompTime`。
  - 在 Nios GCC 最佳化下，該讀值可能被暫存/重排，造成 task completion 時機錯亂。
- 修正:
  - 建 task 時以 `pdata` 帶入自身 prio（1/2/3）。
  - `PeriodicTask` 內改抓自己的 TCB (`me = OSTCBPrioTbl[prio]`)。
  - 迴圈判斷改為 `volatile` 讀取 `me->OSTCBCompTime`。
  - 自行從 ready list 移除也改用 `me`，避免依賴 `OSTCBCur`。

### [BM-003] Set2 輸出序列與預期不一致風險

- 現象:
  - 目前曾出現提早 deadline，顯示週期邊界與完成事件先後可能被破壞。
- 可能根因:
  - task context 與 tick context 對 compTime 的讀寫競態。
  - 初始化時序造成起始 tick 偏移。
- 已採措施:
  - `OSTimeSet(0)` 於任務建立完後重設系統時間。
  - deadline 判斷受 `SimDone` 保護，避免結束後假陽性。

### [BM-004] Set1 在 time=5 多一筆事件（顯示 task(?)）

- 現象:
  - Set1 輸出在 `5 Complete task2(2) Idletask(63)` 前後出現額外事件：
    - `5 Complete task2(2) task(?)`
    - `5 Complete task(?) Idletask(63)`
- 根因:
  - Nios BSP 啟用了系統背景任務（常見為統計 task，priority 62）。
  - Lab1 事件記錄原本未過濾背景任務，導致背景任務切換也被記錄。
- 修正:
  - 在 `hello_ucosii.c` 的 `LogEvent()` 增加 priority 白名單過濾：
    - Set1 只接受 `1, 2, 63`
    - Set2 只接受 `1, 2, 3, 63`
  - 新增 `IsTrackedPrio()`，過濾所有非 Lab 任務切換事件。
- 驗證結果:
  - 額外 `task(?)` 事件可被濾除，但先前版本會連 `5 Complete task2(2) Idletask(63)` 一起濾掉。

### [BM-005] Set1 的 time=5 事件被誤濾除

- 現象:
  - Set1 輸出中 `5 Complete task2(2) Idletask(63)` 消失。
- 根因:
  - 過濾邏輯僅接受 from/to 皆為追蹤任務。
  - 當實際切換是 `task2 -> 系統任務 -> idle` 時，兩筆都被丟棄。
- 修正:
  - 在 `LogEvent()` 加入「橋接模式」：
    - 若 `tracked -> untracked`，暫存 from/time/event。
    - 若同時間同事件出現 `untracked -> tracked`，合併成 `tracked -> tracked`。
  - 這樣可隱藏系統任務，同時保留規格要求的 Lab 事件。
- 驗證結果:
  - 待重新 Build/Run 確認 `time=5` 恢復，且無 `task(?)`。


---

