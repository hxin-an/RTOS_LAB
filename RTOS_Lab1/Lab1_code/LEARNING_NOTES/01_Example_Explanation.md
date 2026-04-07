# 01. 範例 (Test1) 深度解析：從編譯到執行的嵌入式系統完整流程

這份筆記將解析 uC/OS-II 範例專案是如何從原始碼編譯出執行檔，以及程式在啟動後是如何分配工作與執行的。我們將從批次檔 `MAKETEST.BAT` 開始，接著探討編譯規則 `TEST.MAK`，最後進入主程式 `TEST.C` 的架構。

---

## 1. 編譯流程起點：`MAKETEST.BAT`

在命令列環境中，我們透過執行 `MAKETEST.BAT` 來自動化建置過程。這個批次檔的主要功能是建立暫存環境並呼叫編譯工具。

其核心指令如下：

```bat
MD    ..\WORK           :: 建立 WORK 目錄，作為編譯時的暫存工作區
MD    ..\OBJ            :: 建立 OBJ 目錄，用來存放編譯好的 .OBJ 目標檔
MD    ..\LST            :: 建立 LST 目錄，存放編譯與連結過程中的列表檔
CD    ..\WORK           :: 切換到 WORK 工作目錄
COPY  ..\TEST\TEST.MAK   TEST.MAK  :: 複製 MAKEFILE 到工作目錄
C:\BC45\BIN\MAKE -f TEST.MAK       :: 呼叫 Borland C++ 的 MAKE 工具，並使用 TEST.MAK 進行編譯與連結
CD    ..\TEST           :: 編譯完成後返回 TEST 目錄
```

此腳本將環境建構好後，就會將實際的編譯與連結工作交給 `MAKE` 工具與 `TEST.MAK` 來處理。

## 2. 認識嵌入式開發常見的副檔名 (File Extensions)

在你繼續往下看 Makefile 的規則之前，我們先來釐清專案資料夾裡各種不同副檔名的檔案，到底各自代表什麼意義？它們在整個編譯過程裡扮演什麼角色？

| 副檔名 | 類型名稱 | 目的與功能 |
| :--- | :--- | :--- |
| **`.BAT`** | 批次檔 (Batch File) | 本質是自動執行命令列指令的腳本。例如 `MAKETEST.BAT` 就是用來連續執行建立資料夾、複製檔案、呼叫編譯器等一連串枯燥指令的工具。 |
| **`.MAK`** | Make 腳本 (Makefile) | 用來定義專案**編譯規則**的檔案。由於 `TEST.EXE` 包含多個小檔案，`.MAK` 會告訴編譯器「誰依賴誰？誰該先編譯？」，從而達成自動化編譯。 |
| **`.C`** | C 語言原始碼 | 你所寫的神經邏輯與應用程式，例如 `TEST.C` (你的程式) 或是 `uCOS_II.C` (作業系統主程式)，是人類可讀的文字檔。 |
| **`.H`** | 標頭檔 (Header File) | 用來存放變數宣告、常數定義 (`#define`) 或函式原型的檔案。例如 `OS_CFG.H` 定義了系統有幾個任務等組態。`.C` 檔案會透過 `#include` 將其引入。 |
| **`.ASM`** | 組合語言檔 (Assembly) | 比 C 語言更底層的語言，用來直接控制 CPU。以 uC/OS-II 來說，任務切換時必須操作 CPU 內部的暫存器，這件事 C 語言做不到，所以必須寫在 `OS_CPU_A.ASM` 裡。 |
| **`.OBJ`** | 目標檔 (Object File)<br> **關鍵：機器碼的「拼圖碎片」** | 這是 C 語言或組合語言經過「編譯器」翻譯後，所產生的**純機器碼片段**。此時雖然已經是機器看得懂的語言，但它還**缺乏完整的記憶體地址與作業系統資訊**。例如：`TEST.OBJ` 裡面雖然有寫到要呼叫 `OSInit()`，但這階段的 `TEST.OBJ` 根本不知道 `OSInit()` 放在哪個記憶體位址，所以 `.OBJ` 是絕對**無法獨立執行**的。 |
| **`.EXE`** | 執行檔 (Executable)<br> **關鍵：拼湊完成的「最終成品」** | 這是由「連結器 (Linker)」將多塊 `.OBJ` 拼圖 (如 `TEST.OBJ` + `uCOS_II.OBJ` + `PC.OBJ`) 以及底層函式庫組合在一起後產生的最終成品。連結器負責**補齊所有遺失的地址 (補上 `OSInit()` 究竟在哪裡)**，讓各種碎片能彼此相連，並打包成作業系統可以直接載入執行的格式。 |

了解這些檔案的角色後，我們來看 Makefile 是怎麼把它們組合起來的。

---

## 3. 定義編譯規則與相依性：`TEST.MAK`

`TEST.MAK` (也就是 Makefile) 是整個專案建置的核心。

這個檔案的主要任務是告訴 `MAKE` 工具：**「有哪些檔案需要編譯？」、「編譯時要下什麼參數？」以及「最後要怎麼把所有東西組裝 (連結) 成 `.EXE` 執行檔？」**

### 步驟 A：定義工具路徑與目錄變數

在撰寫 Makefile 時，我們會先把常用的路徑定義成「變數」，後面就可以用 `$(變數名稱)` 來代入，讓程式碼更簡潔。

```makefile
BORLAND=C:\BC45                 # 定義 Borland C++ 編譯器的安裝路徑

CC=$(BORLAND)\BIN\BCC           # 定義 CC 變數為 C 語言編譯器 (BCC)
ASM=$(BORLAND)\BIN\TASM         # 定義 ASM 變數為組合語言編譯器 (TASM)
LINK=$(BORLAND)\BIN\TLINK       # 定義 LINK 變數為連結器 (TLINK)

OBJ=..\OBJ                      # 定義編譯後產生的目標檔 (.OBJ) 存放位置
SOURCE=..\SOURCE                # 定義自己的 C 語言程式碼來源位置
OS=\SOFTWARE\uCOS-II\SOURCE     # 定義 uC/OS-II 作業系統的原始碼位置
```
這裡只是把長長的路徑名稱縮短。當後面需要呼叫編譯器時，只要寫 `$(CC)` 就等於寫了 `C:\BC45\BIN\BCC`。

### 步驟 B：設定編譯參數 (`C_FLAGS`)

我們不能只是呼叫編譯器，還要告訴它「要怎麼編譯」。在 `TEST.MAK` 中定義了很長一串參數：

```makefile
C_FLAGS=-c -ml -1 -G -O -Ogemvlbpi -Z -d -n..\obj -k- -v -vi- -wpro -I$(BORLAND)\INCLUDE -L$(BORLAND)\LIB
```
這串密碼般的參數代表什麼？對新手來說，最需要看懂這幾個關鍵：
*   `-c`：**只編譯不連結**。告訴編譯器先把 `.C` 檔案編譯成 `.OBJ` (目標檔) 就好，先不要急著做成 `.EXE`。因為後面還有其他檔案要一起連結。
*   `-ml`：**使用 Large Memory Model (大記憶體模型)**。早期的 x86 處理器有記憶體分段的限制，編譯作業系統層級的程式時，必須強制使用這個模式，才能妥善管理記憶體。
*   `-I` 與 `-L`：分別用來告訴編譯器去哪裡尋找「標頭檔 (Include files)」與「函式庫 (Libraries)」。

### 步驟 C：定義檔案相依性與編譯規則（生成 `.OBJ`）

這是 Makefile 最精華的部分。它定義了「如果 A 檔案有更新，就必須重新編譯 B 檔案」。以編譯 `TEST.C` (純文字程式) 變成 `TEST.OBJ` (機器碼半成品) 的規則為例：

```makefile
# 規則 1：定義目標與相依性
$(OBJ)\TEST.OBJ:                      \
               $(SOURCE)\TEST.C       \
               $(INCLUDES)

# 規則 2：滿足條件時，要執行的實質指令
               COPY   $(SOURCE)\TEST.C      TEST.C
               $(CC)  $(C_FLAGS)            TEST.C
```
這段語法我們透過行數逐一拆解：
1. **目標檔 (`$(OBJ)\TEST.OBJ:`)**：冒號前面代表我們**想要產生的結果**（存在 OBJ 資料夾裡的 TEST.OBJ）。
2. **依賴檔案 (`$(SOURCE)\TEST.C` 與 `$(INCLUDES)`)**：冒號後面代表要產生這個目標，需要**依賴哪些原料**。這裡代表它依賴 `TEST.C` 本身，以及各種標頭檔 (`INCLUDES.H`, `OS_CFG.H` 等等)。這也是 `MAKE` 最聰明的地方：它會檢查檔案最後修改時間，**只有當這幾項「原料」的修改時間，比「目標結果 (`TEST.OBJ`)」還新時，才會觸發底下的編譯指令**。
3. **搬運檔案 (`COPY ...`)**：將原料 `TEST.C` 複製到當前工作目錄。
4. **執行編譯 (`$(CC) $(C_FLAGS) TEST.C`)**：替換變數後，這行實際上是在命令列下達 `C:\BC45\BIN\BCC -c -ml -1 ... TEST.C`，正式啟動 C 編譯器將文字檔轉為二進位的 `.OBJ` 檔。

對於必須直接控制硬體的組合語言 (Assembly)，作法也非常類似：

```makefile
$(OBJ)\OS_CPU_A.OBJ:                  \
               $(PORT)\OS_CPU_A.ASM   

               COPY   $(PORT)\OS_CPU_A.ASM  OS_CPU_A.ASM
	           $(ASM) $(ASM_FLAGS)  $(PORT)\OS_CPU_A.ASM,$(OBJ)\OS_CPU_A.OBJ
```
這裡依賴的是 `OS_CPU_A.ASM`，並且把指令從 `$(CC)` 換成了負責處理組合語言的 `$(ASM)` (也就是 TASM 組譯器)，最後一樣吐出 `.OBJ`。

### 步驟 D：最後組裝，連結成 `TEST.EXE`

當系統把所有的 `.C` 與 `.ASM` 檔都分別編譯成獨立的 `.OBJ` 目標檔後，這些半成品彼此是互相不認識的（例如 `TEST.C` 呼叫了 `OSInit()`，但在這階段它不知道 `OSInit()` 的實際機器碼位於哪裡），這時候就需要「連結器 (Linker)」把這些零散的單位像拼圖一樣組合起來，補上地址：

```makefile
$(TARGET)\TEST.EXE:                  \
               $(WORK)\INCLUDES.H    \
               $(OBJ)\OS_CPU_A.OBJ   \
               $(OBJ)\OS_CPU_C.OBJ   \
               $(OBJ)\PC.OBJ         \
               $(OBJ)\TEST.OBJ       \
               $(OBJ)\uCOS_II.OBJ    \
               $(SOURCE)\TEST.LNK
               
               COPY    $(SOURCE)\TEST.LNK
               $(LINK) $(LINK_FLAGS)     @TEST.LNK
               COPY    $(OBJ)\TEST.EXE   $(TARGET)\TEST.EXE
```
這段規則指出，終極目標 `TEST.EXE` 依賴了所有的 `.OBJ` 檔與一個設定檔 `TEST.LNK`。
當其中任何一個小元件被更新，就會執行 `$(LINK)` (連結器 TLINK)，並透過 `@TEST.LNK` 將指令外包給設定檔。連結器會把核心 (`uCOS_II.OBJ`)、底層 CPU 程式 (`OS_CPU_A.OBJ`)、硬體溝通 (`PC.OBJ`) 以及我們的這支範例應用程式 (`TEST.OBJ`) 全部縫合在一起，補齊全部的函數地址，結合成可以執行的成品 `TEST.EXE`。

---

## 4. 系統啟動：`main()` 的暫時性

透過上述步驟編譯出 `TEST.EXE` 後，執行該檔案會進入 `TEST.C` 中的 `main()`。
在 RTOS 中，`main()` 的職責僅是進行環境初始化與啟動系統核心，啟動後就會將控制權轉交給 OS。

```c
void main (void) {
    OSInit();                  // 1. 初始化 uC/OS-II 內部變數與資料結構 (TCB 表、Event Queue)
    PC_DOSSaveReturn();        // 2. 備份原有 DOS 狀態 (設定定時中斷前，務必先保存)
    PC_VectSet(uCOS, OSCtxSw); // 3. 設定軟體中斷向量 (OSCtxSw)，這是處理任務切換的核心機制
    
    // 4. 建立系統的第一個應用層任務：TaskStart (優先等級設定為 0，具有最高權限)
    OSTaskCreate(TaskStart, (void *)0, &TaskStartStk[TASK_STK_SIZE - 1], 0);
    
    OSStart();                 // 5. 正式啟動多工排程。此函數不會返回，後續完全由 uC/OS-II 接管
}
```

---

## 5. 任務 (Task) 的運作機制

進入多工作業模式後，系統是由多個**任務 (Tasks)** 所組成。
嵌入式系統的任務具有以下技術特質：
*   **無窮迴圈**：任務多半設計為 `for(;;)`，這代表它會持續等待事件發生並進行處理。
*   **私有的堆疊 (Stack)**：上述 `OSTaskCreate` 定義了 `TaskStartStk`。每個任務都有自己獨立的 Stack，用來儲存暫存器狀態與區域變數。
*   **優先權搶佔 (Preemptive)**：uC/OS-II 是優先權搶佔式內核。優先權數字越低，權力越大。一旦高優先權任務變為 Ready 狀態，它會立刻中斷當前較低優先權任務的執行。

---

## 6. 核心運作 3 要素

### A. 系統時鐘滴答 (Clock Tick)
在 `TaskStart` 中可以找到：
```c
PC_VectSet(0x08, OSTickISR);      // 攔截硬體定時器中斷 (Timer 0)
PC_SetTickRate(OS_TICKS_PER_SEC); // 設定 Timer 產生中斷的頻率
```
這是多工系統的計時基礎。系統藉由定期的硬體中斷 (Tick)，不斷檢查是否有處於「延遲」或「等待」狀態的任務已經可以進入 Ready 狀態。

### B. 任務狀態阻塞 (`OSTimeDly`)
```c
OSTimeDly(1); // 延遲當前任務 1 個 Clock Tick
```
呼叫此函數會立刻觸發任務切換 (Context Switch)。這代表當前任務暫時進入 Block 狀態，主動將 CPU 控制權讓出給下一個優先權最高的 Ready 任務。

### C. 資源互斥與同步 (Semaphore)
為了避免多個任務同時存取相同的硬體資源 (例如同時操作螢幕寫入)，程式使用了信號量 (Semaphore)：
```c
OSSemPend(RandomSem, 0, &err); // 請求取得信號量，若無法取得則進入 Block 狀態等待
// ... 執行共同資源存取 (取得亂數並寫入螢幕位置) ...
OSSemPost(RandomSem);          // 釋放信號量，喚醒可能在等待此信號量的任務
```
透過信號量鎖死機制，可保證這段程式碼 (Critical Section) 在同一時間內只有一個任務可以進入。

---

## 7. 功能組態設定：`OS_CFG.H`

在建置作業系統前，會透過修改 `OS_CFG.H` 來決定核心要編譯進哪些功能。
*   **`OS_MAX_TASKS`**: 定義系統最大任務數量，直接影響 TCB 表格所佔用的記憶體大小。
*   **`OS_TICKS_PER_SEC`**: 定義每秒的系統中斷次數 (解析度)。
*   **條件編譯開關**: 例如 `OS_SEM_EN`。如果設定為 0，有關 Semaphore 的程式碼就不會被編譯，藉此將 OS 的體積縮到最小，這件事能達成也正是因為前面提到的 `C_FLAGS` 結合了條件編譯的功能。

透過這些從 `MAKETEST.BAT` 到 `TEST.MAK` 再到 C 程式的分析，你就能了解一個嵌入式系統專案是如何從一堆文字檔案，編譯、連結並成功在硬體上實現多工作業的。
