# NYCU Embedded Real-Time OS Labs

這個 repo 用來整理 NYCU 嵌入式即時作業系統課程的 lab 實作、驗證紀錄、board migration 筆記與報告。

目前內容以 `uC/OS-II` 與 `FreeRTOS` 為主，並保留 DOSBox 驗證版本與 Nios II board migration 版本，方便比對與後續整理。

## Labs Overview

### `RTOS_Lab1`

- 主題：uC/OS-II 上的 RMS scheduling
- 內容：
  - periodic task scheduling
  - `Preempt` / `Complete` trace logging
  - deadline-related behavior verification
- 主要文件：
  - [Lab1_Requirements.md](RTOS_Lab1/Lab1_Requirements.md)
  - [Implementation_Log.md](RTOS_Lab1/Implementation_Log.md)
  - [REPORT.md](RTOS_Lab1/REPORT.md)
  - [Board_Migration_Log.md](RTOS_Lab1/Board_Migration_Log.md)
- 相關資料夾：
  - `Lab1_code/`
  - `NIOS2_Board_Migration_Package/`

### `RTOS_Lab2`

- 主題：uC/OS-II 上的 EDF scheduling
- 內容：
  - EDF ready-task selection
  - absolute deadline bookkeeping
  - DOSBox trace verification
  - Nios II board migration
  - board-side trace bridge/filter debugging
- 主要文件：
  - [Lab2_Requirements.md](RTOS_Lab2/Lab2_Requirements.md)
  - [Implementation_Log.md](RTOS_Lab2/Implementation_Log.md)
  - [REPORT.md](RTOS_Lab2/REPORT.md)
  - [Board_Migration_Log.md](RTOS_Lab2/Board_Migration_Log.md)
- 相關資料夾：
  - `Lab2_code/`
  - `dosbox/`
  - [NIOS2_Board_Migration_Package/](RTOS_Lab2/NIOS2_Board_Migration_Package/)

### `RTOS_Lab3`

- 狀態：待整理

### `RTOS_Lab4`

- 狀態：待整理

### `RTOS_Lab5`

- 狀態：待整理

## Repository Structure

```text
RTOS_LAB/
├─ README.md
├─ .gitignore
├─ RTOS_Lab1/
│  ├─ Lab1_Requirements.md
│  ├─ Implementation_Log.md
│  ├─ REPORT.md
│  ├─ Board_Migration_Log.md
│  ├─ Lab1_code/
│  └─ NIOS2_Board_Migration_Package/
├─ RTOS_Lab2/
│  ├─ Lab2_Requirements.md
│  ├─ Implementation_Log.md
│  ├─ REPORT.md
│  ├─ Board_Migration_Log.md
│  ├─ dosbox/
│  ├─ Lab2_code/
│  └─ NIOS2_Board_Migration_Package/
├─ RTOS_Lab3/
├─ RTOS_Lab4/
└─ RTOS_Lab5/
```

## Lab2 Board Migration Package

`RTOS_Lab2/NIOS2_Board_Migration_Package/` 現在只保留真正要移植到 Eclipse `test/` 與 `test_bsp/` 的檔案：

- `to_test/hello_ucosii.c`
- `to_test_bsp/UCOSII/inc/os_cfg.h`
- `to_test_bsp/UCOSII/inc/ucos_ii.h`
- `to_test_bsp/UCOSII/src/os_core.c`
- `MIGRATION_STEPS.md`

Board migration 的除錯與 trace 問題整理在：

- [RTOS_Lab2/Board_Migration_Log.md](RTOS_Lab2/Board_Migration_Log.md)

完整報告整理在：

- [RTOS_Lab2/REPORT.md](RTOS_Lab2/REPORT.md)

## Notes

- `README.md` 提供整體導覽
- 每個 lab 的需求、實作紀錄、報告與 board migration 筆記都盡量分開保存
- 若後續補齊 `RTOS_Lab3` 到 `RTOS_Lab5`，可沿用同樣的文件結構
