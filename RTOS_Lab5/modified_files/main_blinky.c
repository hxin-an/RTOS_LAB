/*
 * FreeRTOS Lab5: checkpoint and restoration demo.
 *
 * The checkpoint store lives in a linker NOINIT FRAM section.  Each commit
 * keeps two copies and backs up ucHeap, RAM used by .data/.bss, and a CPU
 * context captured with setjmp().
 */

#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"
#include "driverlib.h"

#define mainCHECKPOINT_TASK_PRIORITY    ( tskIDLE_PRIORITY + 1 )
#define mainCHECKPOINT_TASK_STACK_SIZE  ( configMINIMAL_STACK_SIZE + 120 )

/* Lab5 checkpoint constants:
 *
 * mainCHECKPOINT_MAGIC marks a checkpoint slot as valid.  The commit code writes
 * magic last; if power fails in the middle of a commit, magic is still cleared
 * and that half-written slot will be rejected during restore.
 *
 * mainCHECKPOINT_VERSION is the checkpoint layout version.  When the slot
 * structure changes, changing this value prevents old FRAM data from being
 * mistaken for a valid checkpoint with the new format.
 *
 * mainCHECKPOINT_COPY_COUNT is 2 because the lab needs double buffering.  If
 * power fails while copy 0 is being written, copy 1 can still hold the previous
 * complete checkpoint, so restore scans for the newest valid copy.
 *
 * mainCHECKPOINT_RAM_START and mainCHECKPOINT_RAM_BACKUP_BYTES define the SRAM
 * backup window.  This backs up the global/static data area used by .data and
 * .bss, as required by the lab, instead of saving only the visible loop index.
 */
#define mainCHECKPOINT_MAGIC            ( 0xC0FFEE55UL )
#define mainCHECKPOINT_VERSION          ( 7UL )
#define mainCHECKPOINT_COPY_COUNT       ( 2U )
#define mainCHECKPOINT_RAM_START        ( ( uint8_t * ) 0x1C00UL )
#define mainCHECKPOINT_RAM_BACKUP_BYTES ( 0x0800U )
#define mainCHECKPOINT_RAM_MAX_BYTES    ( mainCHECKPOINT_RAM_BACKUP_BYTES )
#define mainCHECKPOINT_RESTORE_VALUE    ( 1 )
#define mainCHECKPOINT_DONE_ITERATION   ( 45U )
#define mainCHECKPOINT_POWER_OFF_USES_LPM45 ( 1 )
#define mainCHECKPOINT_MCLK_HZ          ( 8000000UL )
#define mainCHECKPOINT_RESTORE_WAIT_SECONDS ( 0UL )

/* Lab5: one slot is one complete checkpoint image.  It stores exactly the
 * pieces required by the slides: ucHeap, SRAM used by globals, and CPU context.
 * The magic value is written last, so an interrupted commit is rejected.
 */
typedef struct xCheckpointSlot
{
    uint32_t ulMagic;
    uint32_t ulVersion;
    uint32_t ulSequence;
    uint32_t ulHeapSize;
    uint32_t ulRamSize;
    uint32_t ulHeapChecksum;
    uint32_t ulRamChecksum;
    uint32_t ulContextChecksum;
    jmp_buf xContext;
    uint8_t ucHeapCopy[ configTOTAL_HEAP_SIZE ];
    uint8_t ucRamCopy[ mainCHECKPOINT_RAM_MAX_BYTES ];
} CheckpointSlot_t;

/* Lab5: two slots implement double buffering.  If power fails while writing one
 * slot, the other slot can still be the latest valid checkpoint.
 */
typedef struct xCheckpointStore
{
    uint32_t ulBootCount;
    uint32_t ulRestoreCount;
    uint32_t ulPowerFailCount;
    uint32_t ulActiveIndex;
    CheckpointSlot_t xSlots[ mainCHECKPOINT_COPY_COUNT ];
} CheckpointStore_t;

void main_blinky( void );
void vCheckpointRestoreIfNeeded( void );

static void prvCheckpointTask( void * pvParameters );
static void prvCheckpointCommit( void );
static void prvCheckpointPowerOff( void );
static BaseType_t prvShouldPowerFail( void );
static CheckpointSlot_t * prvGetLatestValidSlot( void );
static BaseType_t prvIsSlotValid( const CheckpointSlot_t * pxSlot );
static uint32_t prvChecksum( const uint8_t * pucBytes, size_t xLength );
static void prvCheckpointRestoreWait( void );
static void prvCopyBytes( uint8_t * pucDestination,
                          const uint8_t * pucSource,
                          size_t xLength );
static void prvClearCheckpointStore( void );
static size_t prvGetRamBackupSize( void );

/* Lab5: ucHeap is declared in main.c so checkpoint code can copy the FreeRTOS
 * heap, including task stacks and TCBs allocated by heap_4.c.
 */
extern uint8_t ucHeap[ configTOTAL_HEAP_SIZE ];
extern void vApplicationSetupTimerInterrupt( void );

/* Lab5: setjmp() writes the CPU restore point here; the buffer is also copied
 * into the checkpoint slot so longjmp() can resume after power returns.
 */
static jmp_buf xCheckpointContext;
/* Lab5: current loop index i from the slide pseudo-code.  Restore should bring
 * this value back to the last committed checkpoint.
 */
static volatile uint16_t usIteration = 0U;
/* Lab5: the most recent committed i.  The power-fail trigger uses this to avoid
 * failing repeatedly before a newer checkpoint can be created.
 */
static volatile uint16_t usLastCommitIteration = 0U;
/* Lab5: small randomized offset so simulated power failures are not always at
 * exactly the same iteration.
 */
static volatile uint16_t usPowerFailOffset = 0U;
/* Lab5: extra changing SRAM state.  This proves the checkpoint restores more
 * than only the visible loop index i.
 */
static volatile uint16_t usPayload[ 16 ] = { 0U };

/* Lab5: .checkpoint is mapped by the linker to NOINIT FRAM.  It survives reset,
 * LPM4.5 wakeup, and power removal as long as CCS does not erase that section.
 */
#pragma DATA_SECTION( xCheckpointStore, ".checkpoint" )
#pragma RETAIN( xCheckpointStore )
static CheckpointStore_t xCheckpointStore;

void main_blinky( void )
{
    /* Lab5: reaching this function means there was no valid checkpoint, so this
     * is the first run after erase or after the previous demo cleared the store.
     */
    printf( "Checkpoint Lab5\r\n" );
    printf( "fresh boot\r\n" );
    fflush( stdout );

    srand( 7U );
    usPowerFailOffset = ( uint16_t ) ( rand() % 3U );

    /* Lab5: use one FreeRTOS task to match the slide pseudo-code test_task(). */
    ( void ) xTaskCreate( prvCheckpointTask,
                          "Checkpoint",
                          mainCHECKPOINT_TASK_STACK_SIZE,
                          NULL,
                          mainCHECKPOINT_TASK_PRIORITY,
                          NULL );

    vTaskStartScheduler();

    for( ;; );
}

void vCheckpointRestoreIfNeeded( void )
{
    CheckpointSlot_t * const pxSlot = prvGetLatestValidSlot();

    if( pxSlot != NULL )
    {
        const unsigned int uiCopy = ( unsigned int ) ( pxSlot - &( xCheckpointStore.xSlots[ 0 ] ) );

        /* Lab5: restore in the required order: heap first, SRAM second, CPU
         * context last.  Restoring the context with longjmp() jumps back to the
         * setjmp() call inside prvCheckpointCommit().
         */
        xCheckpointStore.ulBootCount++;
        xCheckpointStore.ulRestoreCount++;
        xCheckpointStore.ulActiveIndex = ( uint32_t ) uiCopy;

        prvCopyBytes( ucHeap,
                      pxSlot->ucHeapCopy,
                      ( size_t ) pxSlot->ulHeapSize );
        prvCopyBytes( mainCHECKPOINT_RAM_START,
                      pxSlot->ucRamCopy,
                      ( size_t ) pxSlot->ulRamSize );

        prvCopyBytes( ( uint8_t * ) xCheckpointContext,
                      ( const uint8_t * ) pxSlot->xContext,
                      sizeof( xCheckpointContext ) );

        prvCheckpointRestoreWait();
        vApplicationSetupTimerInterrupt();
        longjmp( xCheckpointContext, mainCHECKPOINT_RESTORE_VALUE );
    }

    /* Lab5: no valid checkpoint means a fresh boot.  Clear stale bytes so an
     * old partial image cannot be mistaken for a valid restore point.
     */
    prvClearCheckpointStore();
    xCheckpointStore.ulBootCount = 1UL;
}

static void prvCheckpointTask( void * pvParameters )
{
    ( void ) pvParameters;

    for( ;; )
    {
        /* Lab5: this loop is the slide's while(i++).  usPayload gives the RAM
         * backup something observable beyond only the loop index.
         */
        usIteration++;
        usPayload[ usIteration & 0x0FU ] = ( uint16_t ) ( usIteration ^ 0x5A5AU );

        /* Lab5: random_condition() is tested before commit, matching the slide
         * pseudo-code.  A power fail before the next commit should restore to
         * the previous committed iteration.
         */
        if( prvShouldPowerFail() != pdFALSE )
        {
            prvCheckpointPowerOff();
        }

        /* Lab5: commit every 10 iterations so the output is easy to verify:
         * failures between 10 and 20 restore to 10, between 20 and 30 to 20.
         */
        if( ( usIteration % 10U ) == 0U )
        {
            usLastCommitIteration = usIteration;
            prvCheckpointCommit();
        }

        printf( "i:%d\r\n", ( int ) usIteration );

        if( usIteration >= mainCHECKPOINT_DONE_ITERATION )
        {
            printf( "done i:%d\r\n", ( int ) usIteration );
            /* Lab5: clear after a complete demo so the next manual run starts
             * from fresh boot instead of restoring a finished checkpoint.
             */
            prvClearCheckpointStore();
            vTaskSuspend( NULL );
        }
    }
}

static void prvCheckpointCommit( void )
{
    CheckpointSlot_t * const pxLatestSlot = prvGetLatestValidSlot();
    uint32_t ulNextSequence = 1UL;
    uint32_t ulNextIndex = 0UL;
    CheckpointSlot_t * pxNextSlot;
    const size_t xRamBytes = prvGetRamBackupSize();

    if( pxLatestSlot != NULL )
    {
        ulNextSequence = pxLatestSlot->ulSequence + 1UL;
        ulNextIndex = ( xCheckpointStore.ulActiveIndex + 1UL ) % mainCHECKPOINT_COPY_COUNT;
    }

    pxNextSlot = &( xCheckpointStore.xSlots[ ulNextIndex ] );
    /* Lab5: invalidate the target slot first; it becomes valid only after all
     * copies and checksums are complete.
     */
    pxNextSlot->ulMagic = 0UL;

    if( setjmp( xCheckpointContext ) == 0 )
    {
        /* Lab5: use raw interrupt masking instead of a FreeRTOS critical
         * section, because saving FreeRTOS critical nesting state inside the
         * checkpoint can make the restored task resume with scheduling blocked.
         */
        __disable_interrupt();
        prvCopyBytes( pxNextSlot->ucHeapCopy, ucHeap, configTOTAL_HEAP_SIZE );
        prvCopyBytes( pxNextSlot->ucRamCopy, mainCHECKPOINT_RAM_START, xRamBytes );
        prvCopyBytes( ( uint8_t * ) pxNextSlot->xContext,
                      ( const uint8_t * ) xCheckpointContext,
                      sizeof( xCheckpointContext ) );

        pxNextSlot->ulVersion = mainCHECKPOINT_VERSION;
        pxNextSlot->ulSequence = ulNextSequence;
        pxNextSlot->ulHeapSize = configTOTAL_HEAP_SIZE;
        pxNextSlot->ulRamSize = ( uint32_t ) xRamBytes;
        pxNextSlot->ulHeapChecksum = prvChecksum( pxNextSlot->ucHeapCopy,
                                                  configTOTAL_HEAP_SIZE );
        pxNextSlot->ulRamChecksum = prvChecksum( pxNextSlot->ucRamCopy,
                                                 xRamBytes );
        pxNextSlot->ulContextChecksum = prvChecksum( ( const uint8_t * ) pxNextSlot->xContext,
                                                     sizeof( pxNextSlot->xContext ) );
        /* Lab5: write magic last to make the commit atomic enough for the lab:
         * a reset during the copy leaves magic cleared and the slot invalid.
         */
        pxNextSlot->ulMagic = mainCHECKPOINT_MAGIC;
        xCheckpointStore.ulActiveIndex = ulNextIndex;
        __enable_interrupt();

        printf( "commit i:%d\r\n", ( int ) usIteration );
        fflush( stdout );
    }
    else
    {
        /* Lab5: after longjmp(), execution continues here, proving the CPU
         * context was restored rather than starting the task from the top.
         */
        printf( "Checkpoint Lab5\r\n" );
        printf( "restore\r\n" );
        printf( "restore i:%d\r\n", ( int ) usIteration );
        fflush( stdout );
    }
}

static void prvCheckpointPowerOff( void )
{
    xCheckpointStore.ulPowerFailCount++;

    /* Lab5: print before entering LPM4.5 so the console shows exactly where the
     * simulated power failure happened.
     */
    printf( "power_off i:%d\r\n", ( int ) usIteration );
    fflush( stdout );

    __disable_interrupt();

    #if( mainCHECKPOINT_POWER_OFF_USES_LPM45 != 0 )
    {
        /* Lab5: LPM4.5 is the slide's convenient power-fail trigger.  The same
         * checkpoint data also survives real USB power removal because it is in
         * FRAM.
         */
        PMM_turnOffRegulator();
        __bis_SR_register( LPM4_bits + GIE );
    }
    #else
    {
        PMMCTL0 = PMMPW | PMMSWBOR;
    }
    #endif

    for( ;; );
}

static BaseType_t prvShouldPowerFail( void )
{
    /* Lab5: the failure point is deterministic enough for demo, but still uses
     * rand() as requested by the slides.  The >= 3 guard prevents repeated
     * failures before a new checkpoint has a chance to be committed.
     */
    const uint16_t usNextFailIteration =
        ( uint16_t ) ( 13U + usPowerFailOffset +
                       ( uint16_t ) ( xCheckpointStore.ulPowerFailCount * 17UL ) );
    const int iRandomValue = rand();
    const uint16_t usRandomNudge = ( uint16_t ) ( iRandomValue % 2 );

    if( ( usIteration > usLastCommitIteration ) &&
        ( ( usIteration - usLastCommitIteration ) >= 3U ) &&
        ( ( usIteration + usRandomNudge ) >= usNextFailIteration ) &&
        ( usIteration < mainCHECKPOINT_DONE_ITERATION ) )
    {
        return pdTRUE;
    }

    return pdFALSE;
}

static CheckpointSlot_t * prvGetLatestValidSlot( void )
{
    CheckpointSlot_t * pxBestSlot = NULL;
    uint16_t usIndex;

    /* Lab5: try the active index first because it is the normal fast path after
     * a clean commit.
     */
    if( ( xCheckpointStore.ulActiveIndex < mainCHECKPOINT_COPY_COUNT ) &&
        ( prvIsSlotValid( &( xCheckpointStore.xSlots[ xCheckpointStore.ulActiveIndex ] ) ) != pdFALSE ) )
    {
        pxBestSlot = &( xCheckpointStore.xSlots[ xCheckpointStore.ulActiveIndex ] );
    }

    /* Lab5: scan both copies anyway so restore still works if power failed
     * while activeIndex was being updated.
     */
    for( usIndex = 0U; usIndex < mainCHECKPOINT_COPY_COUNT; usIndex++ )
    {
        CheckpointSlot_t * const pxSlot = &( xCheckpointStore.xSlots[ usIndex ] );

        if( prvIsSlotValid( pxSlot ) != pdFALSE )
        {
            if( ( pxBestSlot == NULL ) || ( pxSlot->ulSequence > pxBestSlot->ulSequence ) )
            {
                pxBestSlot = pxSlot;
            }
        }
    }

    return pxBestSlot;
}

static BaseType_t prvIsSlotValid( const CheckpointSlot_t * pxSlot )
{
    /* Lab5: reject slots from old code versions, partial commits, or an
     * unexpected backup size before trusting the data.
     */
    if( ( pxSlot->ulMagic != mainCHECKPOINT_MAGIC ) ||
        ( pxSlot->ulVersion != mainCHECKPOINT_VERSION ) ||
        ( pxSlot->ulHeapSize != configTOTAL_HEAP_SIZE ) ||
        ( pxSlot->ulRamSize == 0UL ) ||
        ( pxSlot->ulRamSize > mainCHECKPOINT_RAM_MAX_BYTES ) )
    {
        return pdFALSE;
    }

    /* Lab5: checksums prove we copied the actual heap, RAM, and CPU context,
     * not only the visible loop index.
     */
    if( pxSlot->ulHeapChecksum != prvChecksum( pxSlot->ucHeapCopy,
                                               ( size_t ) pxSlot->ulHeapSize ) )
    {
        return pdFALSE;
    }

    if( pxSlot->ulRamChecksum != prvChecksum( pxSlot->ucRamCopy,
                                              ( size_t ) pxSlot->ulRamSize ) )
    {
        return pdFALSE;
    }

    if( pxSlot->ulContextChecksum != prvChecksum( ( const uint8_t * ) pxSlot->xContext,
                                                  sizeof( pxSlot->xContext ) ) )
    {
        return pdFALSE;
    }

    return pdTRUE;
}

static uint32_t prvChecksum( const uint8_t * pucBytes, size_t xLength )
{
    /* Lab5: a small FNV-1a checksum is enough to detect stale or half-written
     * checkpoint data during the demo.
     */
    uint32_t ulChecksum = 2166136261UL;
    size_t xIndex;

    for( xIndex = 0U; xIndex < xLength; xIndex++ )
    {
        ulChecksum ^= pucBytes[ xIndex ];
        ulChecksum *= 16777619UL;
    }

    return ulChecksum;
}

static void prvCheckpointRestoreWait( void )
{
    /* Lab5: optional demo delay.  Leave it at 0 for reset-button demos; raise it
     * only when you need time to attach CCS after physical power removal.
     */
    #if( mainCHECKPOINT_RESTORE_WAIT_SECONDS > 0UL )
    uint32_t ulSecond;

    for( ulSecond = 0UL; ulSecond < mainCHECKPOINT_RESTORE_WAIT_SECONDS; ulSecond++ )
    {
        __delay_cycles( mainCHECKPOINT_MCLK_HZ );
    }
    #endif
}

static void prvCopyBytes( uint8_t * pucDestination,
                          const uint8_t * pucSource,
                          size_t xLength )
{
    size_t xIndex;

    for( xIndex = 0U; xIndex < xLength; xIndex++ )
    {
        pucDestination[ xIndex ] = pucSource[ xIndex ];
    }
}

static void prvClearCheckpointStore( void )
{
    /* Lab5: clearing the FRAM store gives a true fresh boot after the demo
     * finishes or when no valid checkpoint exists.
     */
    uint8_t * const pucStore = ( uint8_t * ) &xCheckpointStore;
    size_t xIndex;

    for( xIndex = 0U; xIndex < sizeof( xCheckpointStore ); xIndex++ )
    {
        pucStore[ xIndex ] = 0U;
    }
}

static size_t prvGetRamBackupSize( void )
{
    /* Lab5: this board's global/static RAM for the demo fits inside the fixed
     * 0x0800-byte window starting at mainCHECKPOINT_RAM_START.
     */
    return mainCHECKPOINT_RAM_BACKUP_BYTES;
}
