# Lab5 Implementation Log

Lab5 的目標是在 FreeRTOS 上實作 checkpoint restoration。程式在執行時會定期 commit checkpoint 到 FRAM；遇到 simulated power failure 或實際斷電後，重新啟動時會從 FRAM 找到有效 checkpoint，依序還原 `ucHeap`、SRAM、CPU context，最後回到 checkpoint 當時的位置繼續執行。

Checkpoint commit / restore 的順序都符合投影片要求：

```text
1. ucHeap
2. SRAM (.data / .bss 的 global/static state)
3. CPU context
```

## modified_files/main_blinky.c

`main_blinky.c` 是 Lab5 的核心實作，包含 checkpoint storage、commit、restore、power failure trigger，以及 demo task。

### Checkpoint Storage

程式定義 `CheckpointStore_t xCheckpointStore` 作為整個 checkpoint 儲存區，裡面包含兩份 checkpoint copy：

```c
CheckpointSlot_t xSlots[ mainCHECKPOINT_COPY_COUNT ];
```

其中：

```c
#define mainCHECKPOINT_COPY_COUNT ( 2U )
```

代表使用 double buffering。這樣如果正在寫 copy 0 時斷電，copy 0 可能是壞的，但 copy 1 仍然保留上一個完整 checkpoint。restore 時會掃描兩份 copy，找出 checksum 正確且 sequence 最新的一份。

每個 `CheckpointSlot_t` 會保存：

- `jmp_buf xContext`：CPU context，由 `setjmp()` 產生，restore 時用 `longjmp()` 回到 checkpoint。
- `ucHeapCopy[]`：FreeRTOS heap 備份。
- `ucRamCopy[]`：SRAM 備份。
- magic、version、sequence、size、checksum 等 metadata。

### FRAM Section

`xCheckpointStore` 被指定到 `.checkpoint` section：

```c
#pragma DATA_SECTION( xCheckpointStore, ".checkpoint" )
#pragma RETAIN( xCheckpointStore )
static CheckpointStore_t xCheckpointStore;
```

`DATA_SECTION` 讓 linker 把 `xCheckpointStore` 放到 `.checkpoint`。`RETAIN` 防止 linker 把這個 storage 最佳化移除。

### Commit

`prvCheckpointCommit()` 會建立新的 checkpoint copy。

流程：

1. 找目前最新有效 checkpoint。
2. 決定下一個要寫的 copy index。
3. 先把新 slot 的 `ulMagic` 清成 0，讓它暫時無效。
4. 用 `setjmp( xCheckpointContext )` 保存 CPU context。
5. 關中斷，避免 copy 過程中狀態被改動。
6. 依序備份 `ucHeap`、SRAM、CPU context。
7. 計算 heap / SRAM / context checksum。
8. 最後才寫入 `mainCHECKPOINT_MAGIC`，表示這份 checkpoint 完整有效。
9. 更新 `ulActiveIndex`。

關鍵順序：

```c
prvCopyBytes( pxNextSlot->ucHeapCopy, ucHeap, configTOTAL_HEAP_SIZE );
prvCopyBytes( pxNextSlot->ucRamCopy, mainCHECKPOINT_RAM_START, xRamBytes );
prvCopyBytes( ( uint8_t * ) pxNextSlot->xContext,
              ( const uint8_t * ) xCheckpointContext,
              sizeof( xCheckpointContext ) );
```

`ulMagic` 最後才寫入，是為了避免 commit 中途斷電時，半寫入的 checkpoint 被誤判成有效。

### Restore

`vCheckpointRestoreIfNeeded()` 會在 `main.c` 裡、建立 task 之前被呼叫。

如果找到有效 checkpoint，restore 順序是：

```c
prvCopyBytes( ucHeap, pxSlot->ucHeapCopy, ... );
prvCopyBytes( mainCHECKPOINT_RAM_START, pxSlot->ucRamCopy, ... );
prvCopyBytes( ( uint8_t * ) xCheckpointContext,
              ( const uint8_t * ) pxSlot->xContext,
              sizeof( xCheckpointContext ) );
```

接著重新啟動 FreeRTOS tick timer：

```c
vApplicationSetupTimerInterrupt();
```

最後用：

```c
longjmp( xCheckpointContext, mainCHECKPOINT_RESTORE_VALUE );
```

真正恢復 CPU context，回到當初 `setjmp()` 的位置。`vApplicationSetupTimerInterrupt()` 不是恢復 tick 數值，而是讓 hardware timer / tick interrupt 重新運作；tick count 本身屬於 SRAM/global state，已經在 SRAM restore 時一起還原。

### Slot Validation

`prvIsSlotValid()` 負責檢查 checkpoint slot 是否可用。它會檢查：

- magic 是否正確。
- version 是否符合目前 checkpoint format。
- heap / RAM size 是否合理。
- heap / RAM / CPU context checksum 是否正確。

checksum 的用途是 restore 前的完整性檢查，避免使用半寫入、舊格式或損壞的 checkpoint。

### Power Failure Trigger

`prvCheckpointTask()` 是投影片中的 test task。它會：

- 每輪 `usIteration++`。
- 更新 `usPayload[]`，證明 checkpoint 備份的不只是 visible index。
- 呼叫 `prvShouldPowerFail()` 判斷是否觸發 power failure。
- 每 10 次 iteration commit 一次 checkpoint。
- 到 `done i:45` 後清除 checkpoint store，讓下一次 demo 從 fresh boot 開始。

`prvCheckpointPowerOff()` 使用 LPM4.5 模擬 power failure：

```c
PMM_turnOffRegulator();
__bis_SR_register( LPM4_bits + GIE );
```

這是投影片建議的方便測試方式。實際拔 USB 電源時，checkpoint 仍可保留，因為 storage 在 FRAM `.checkpoint` section。

## modified_files/lnk_msp430fr5994.cmd

此檔案是 linker command file，用來描述 MSP430FR5994 的 memory layout，以及各 section 要放到哪個 memory region。

Lab5 主要使用這行：

```cmd
.checkpoint : type = NOINIT {}
```

`.checkpoint` 被放在 FRAM 的 read-write memory group 裡，並設定 `NOINIT`。

`NOINIT` 表示 C startup 不會把這段 section 清成 0 或重新初始化。這很重要，因為 reset / LPM4.5 wakeup / power recovery 後，程式進入 `main()` 前不能把 checkpoint 資料清掉。

## RTOSDemo_Lab5.map

此檔案是從 VM build output 複製出來的 linker map，作為 memory mapping 驗證紀錄。

map 中可確認：

```text
.checkpoint
*          0    00006000    00005098     UNINITIALIZED
                  00006000    00005098     main_blinky.obj (.checkpoint:retain)
```

這代表：

- `.checkpoint` 起始位址是 `0x6000`。
- 大小是 `0x5098`。
- 屬性是 `UNINITIALIZED`，對應 `.cmd` 裡的 `NOINIT`。
- 來源是 `main_blinky.obj (.checkpoint:retain)`，對應 `xCheckpointStore`。

map 中也可確認 `ucHeap`：

```text
00004000 00002000 main.obj (.TI.persistent:ucHeap)
00004000 ucHeap
```

代表 `ucHeap` 位於 `0x4000`，大小 `0x2000`，也就是 8 KB。

## modified_files/FreeRTOSConfig.h

Lab5 需要實際備份 FreeRTOS heap，因此設定：

```c
#define configAPPLICATION_ALLOCATED_HEAP 1
```

這表示 FreeRTOS heap 的底層 array 由 application 提供，而不是由 `heap_4.c` 內部自己宣告。`heap_4.c` 仍然負責 heap allocation / free 的演算法，只是它使用外部定義的 `ucHeap`。

## modified_files/main.c

`main.c` 負責 Lab5 啟動順序。

重要順序：

```c
prvSetupHardware();
vCheckpointRestoreIfNeeded();
main_blinky();
```

`prvSetupHardware()` 先重新初始化硬體。接著在建立任何 task 之前呼叫 `vCheckpointRestoreIfNeeded()`，避免 fresh boot 的 task creation 覆蓋 checkpoint 中的 heap / SRAM state。如果沒有有效 checkpoint，才進入 `main_blinky()` 建立新的 test task。

`main.c` 也宣告 FreeRTOS heap：

```c
#pragma PERSISTENT( ucHeap )
uint8_t ucHeap[ configTOTAL_HEAP_SIZE ] = { 0 };
```

配合 `configAPPLICATION_ALLOCATED_HEAP = 1`，讓 checkpoint code 可以直接 copy / restore `ucHeap`。

## modified_files/printf-stdarg.c

此檔案提供 lightweight `printf()`。Lab5 透過 `putchar()` 將輸出送到 CCS CIO console：

```c
extern int putchar(int c);
(void)putchar(c);
```

這讓 demo 可以在 CCS Console 看到：

```text
Checkpoint Lab5
fresh boot
i:1
...
commit i:10
...
power_off i:14
Checkpoint Lab5
restore
restore i:10
...
done i:45
```

## Modified File Summary

Lab5 實際需要提交 / 說明的 modified files：

```text
modified_files/main_blinky.c
modified_files/lnk_msp430fr5994.cmd
modified_files/FreeRTOSConfig.h
modified_files/main.c
modified_files/printf-stdarg.c
RTOSDemo_Lab5.map
```

Lab5 沒有修改 FreeRTOS scheduler，因此不需要提交或說明 `task.h` / `tasks.c`。
