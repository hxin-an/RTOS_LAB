# Lab1 Requirements (簡化版)

## 1. Lab 目標

在 uC/OS-II 上完成 RMS 排程實作，並做到：

1. 可在 DOSBox 正常執行與驗證。
2. 能輸出正確事件序列（Preempt/Complete）。
3. 能偵測 Set2 的 deadline miss。
4. 後續可遷移到 Nios II 板子版本。

## 2. Task Set 定義

### Set1

1. task1(1): c=1, p=3
2. task2(2): c=3, p=6
3. Idletask(63)
4. task3(3) 關閉

### Set2

1. task1(1): c=1, p=3
2. task2(2): c=3, p=6
3. task3(3): c=4, p=9
4. Idletask(63)

## 3. 功能需求

1. 程式啟動後可正常進入排程，不可卡死。
2. 每個 period 起點重設 compTime=c。
3. 每個 tick，若 task 正在執行則 compTime 減 1。
4. compTime 歸零才視為本 period 完成。
5. 週期邊界必須用絕對時間判定（避免累積漂移）。
6. Event 只允許 `Preempt` 與 `Complete`。

## 4. 輸出格式

### 事件行

`<time> <Event> <from> <to>`

### deadline 行

`time:<t> task<n> exceed deadline`

### Task 名稱/優先權

1. task1(1)
2. task2(2)
3. task3(3)
4. Idletask(63)

## 5. 停止條件

1. Set1: 印完指定事件序列最後一行後停止。
2. Set2: 印出 deadline 行後立即停止，不可再印其他事件。

## 6. 目標輸出

### Set1

1. 0 Complete Idletask(63) task1(1)
2. 1 Complete task1(1) task2(2)
3. 3 Preempt task2(2) task1(1)
4. 4 Complete task1(1) task2(2)
5. 5 Complete task2(2) Idletask(63)
6. 6 Preempt Idletask(63) task1(1)
7. 7 Complete task1(1) task2(2)
8. 9 Preempt task2(2) task1(1)
9. 10 Complete task1(1) task2(2)
10. 11 Complete task2(2) Idletask(63)
11. 12 Preempt Idletask(63) task1(1)
12. 13 Complete task1(1) task2(2)
13. 15 Preempt task2(2) task1(1)
14. 16 Complete task1(1) task2(2)
15. 17 Complete task2(2) Idletask(63)
16. 18 Preempt Idletask(63) task1(1)
17. 19 Complete task1(1) task2(2)
18. 21 Preempt task2(2) task1(1)
19. 22 Complete task1(1) task2(2)
20. 23 Complete task2(2) Idletask(63)
21. 24 Preempt Idletask(63) task1(1)
22. 25 Complete task1(1) task2(2)
23. 27 Preempt task2(2) task1(1)

### Set2

1. 0 Complete Idletask(63) task1(1)
2. 1 Complete task1(1) task2(2)
3. 3 Preempt task2(2) task1(1)
4. 4 Complete task1(1) task2(2)
5. 5 Complete task2(2) task3(3)
6. 6 Preempt task3(3) task1(1)
7. 7 Complete task1(1) task2(2)
8. 9 Preempt task2(2) task1(1)
9. time:9 task3 exceed deadline

