# NYCU 嵌入式即時系統課程作業整理

這個倉庫用來整理 NYCU 嵌入式即時系統課程（張立平老師）整學期實作。
內容涵蓋 uC/OS-II 與 FreeRTOS 兩個階段，從模擬環境驗證到板子遷移。

## 各 Lab 詳細簡介

1. RTOS_Lab1（uC/OS-II）
	主題：RMS 排程實作、事件紀錄（Preempt/Complete）、deadline 偵測。
	內容：修改核心與應用層，於 DOSBox 驗證 Set1/Set2 事件序列，並整理到板子遷移版本。
	目前狀態：已完成。
	產出：可正常執行的 DOSBox 版本程式，以及成功遷移到板子的程式版本和實作紀錄。

2. RTOS_Lab2（uC/OS-II）
	目前狀態：僅確認使用板子，實作內容尚未確定。
	備註：待課程進度與作業說明公布後補齊主題與產出。

3. RTOS_Lab3（uC/OS-II）
	目前狀態：僅確認使用板子，實作內容尚未確定。
	備註：待課程進度與作業說明公布後補齊主題與產出。

4. RTOS_Lab4（FreeRTOS）
	目前狀態：僅確認使用板子，實作內容尚未確定。
	備註：待課程進度與作業說明公布後補齊主題與產出。

5. RTOS_Lab5（FreeRTOS）
	目前狀態：僅確認使用板子，實作內容尚未確定。
	備註：待課程進度與作業說明公布後補齊主題與產出。

## 倉庫結構

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
│  │  ├─ SOFTWARE/
│  │  └─ bc45/
│  └─ NIOS2_Board_Migration_Package/
│     ├─ MIGRATION_STEPS.md
│     ├─ reference/
│     ├─ to_test/
│     └─ to_test_bsp/
├─ RTOS_Lab2/
│  └─ README.md
├─ RTOS_Lab3/
│  └─ README.md
├─ RTOS_Lab4/
│  └─ README.md
└─ RTOS_Lab5/
   └─ README.md
```

1. RTOS_Lab1（已完成）
   - Lab1_Requirements.md：作業需求與驗收條件。
   - Implementation_Log.md：實作過程、問題與修正紀錄。
   - REPORT.md：最終流程、結果與分析。
   - Board_Migration_Log.md：DOSBox 到板端移植差異與修正說明。
   - Lab1_code/SOFTWARE：原始實驗程式與 uC/OS-II 相關內容。
   - Lab1_code/bc45：Borland C 4.5 工具鏈相關資源。
   - NIOS2_Board_Migration_Package：板端移植程式、BSP、參考檔與步驟文件。

2. RTOS_Lab2 ~ RTOS_Lab5（規劃中）
   - 目前各資料夾先保留 README.md。
   - 已確認使用板子，詳細題目與實作內容待課程公布後補上。

## 根目錄檔案用途

1. .gitignore：版本控制忽略規則。
2. README.md：整學期倉庫導覽與狀態說明（本文件）。

