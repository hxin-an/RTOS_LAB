/*
 * FreeRTOS Lab4: EDF scheduler test.
 *
 * This file replaces the original blinky demo with the two task sets from the
 * lab slides.  Select the task set with mainLAB4_TASK_SET below, rebuild, and
 * start the debugger again.
 */

#include <stdio.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"

/* Lab4 EDF demo settings.
 *
 * mainLAB4_TASK_SET selects the slide task set:
 *   set 1: T1(c=1,p=3), T2(c=3,p=5)
 *   set 2: T1(c=1,p=4), T2(c=2,p=5), T3(c=2,p=10)
 *
 * The lab uses implicit deadlines, so each task's relative deadline equals
 * its period.
 *
 * All workload tasks intentionally use the same FreeRTOS priority.  The lab
 * change is in tasks.c: among same-priority ready tasks, the scheduler chooses
 * the task with the earliest absolute deadline.
 */
#define mainLAB4_TASK_SET             1
#define mainLAB4_TASK_PRIORITY        ( tskIDLE_PRIORITY + 1 )
/* Lab4: the extra stack words are a small safety margin for printf and hooks. */
#define mainLAB4_TASK_STACK_SIZE      ( configMINIMAL_STACK_SIZE + 40 )
#define mainLAB4_REPORTER_STACK_SIZE  ( configMINIMAL_STACK_SIZE + 80 )
/* Lab4: fixed-size arrays keep the demo simple and avoid dynamic allocation. */
#define mainLAB4_MAX_TASKS            3
#define mainLAB4_MAX_EVENTS           24

#if ( mainLAB4_TASK_SET == 1 )
    #define mainLAB4_TASK_COUNT       2
    #define mainLAB4_STOP_TICK        20
#else
    #define mainLAB4_TASK_COUNT       3
    #define mainLAB4_STOP_TICK        20
#endif

typedef struct xLab4TaskConfig
{
    /* Lab4: one row is one periodic task from the slide table. */
    UBaseType_t uxTaskNumber;
    TickType_t xExecutionTime;
    TickType_t xPeriod;
} Lab4TaskConfig_t;

typedef struct xLab4Event
{
    /* Lab4: events are buffered in RAM first so scheduler hooks do not print
     * from inside the kernel context-switch path.
     */
    TickType_t xTime;
    char cEvent;
    UBaseType_t uxFromTask;
    UBaseType_t uxToTask;
} Lab4Event_t;

void main_blinky( void );
void vApplicationEDFPreemptHook( UBaseType_t uxFromTask,
                                 UBaseType_t uxToTask,
                                 TickType_t xTime );
void vApplicationEDFCompleteHook( UBaseType_t uxFromTask,
                                  UBaseType_t uxToTask,
                                  TickType_t xTime );

static void prvEDFTask( void * pvParameters );
static void prvReporterTask( void * pvParameters );
static void prvRecordEvent( TickType_t xTime,
                            char cEvent,
                            UBaseType_t uxFromTask,
                            UBaseType_t uxToTask );
static void prvPrintEvent( TickType_t xTime,
                           char cEvent,
                           UBaseType_t uxFromTask,
                           UBaseType_t uxToTask );
static void prvPrintTaskName( UBaseType_t uxTaskNumber );
static void prvBurnExecutionTick( void );

#if ( mainLAB4_TASK_SET == 1 )
    static const Lab4TaskConfig_t xTaskConfigs[ mainLAB4_TASK_COUNT ] =
    {
        { 1, 1, 3 },
        { 2, 3, 5 }
    };
#else
    static const Lab4TaskConfig_t xTaskConfigs[ mainLAB4_TASK_COUNT ] =
    {
        { 1, 1, 4 },
        { 2, 2, 5 },
        { 3, 2, 10 }
    };
#endif

/* Lab4: handles let the demo attach EDF metadata after xTaskCreate() returns. */
static TaskHandle_t xTaskHandles[ mainLAB4_MAX_TASKS ] = { NULL };
static volatile BaseType_t xStopRequested = pdFALSE;
static volatile UBaseType_t uxEventCount = 0;
/* Lab4: the expected output is a short trace, so a bounded log is enough. */
static volatile Lab4Event_t xEventLog[ mainLAB4_MAX_EVENTS ];
static TickType_t xLabStartTime = 0;

void main_blinky( void )
{
    UBaseType_t uxIndex;

    /* Lab4: print the task set header before the scheduler starts so the
     * console output matches the lab's expected answer exactly.
     */
    for( uxIndex = 0; uxIndex < mainLAB4_TASK_COUNT; uxIndex++ )
    {
        printf( "T%d c:%d p:%d\r\n",
                ( int ) xTaskConfigs[ uxIndex ].uxTaskNumber,
                ( int ) xTaskConfigs[ uxIndex ].xExecutionTime,
                ( int ) xTaskConfigs[ uxIndex ].xPeriod );
    }

    /* Lab4: create the periodic workload.  xTaskCreate() creates a normal FreeRTOS
     * task first; vTaskSetEDFParameters() then attaches the Lab4 EDF metadata
     * that the modified scheduler reads from the task's TCB.
     */
    for( uxIndex = mainLAB4_TASK_COUNT; uxIndex > 0U; uxIndex-- )
    {
        const UBaseType_t uxConfigIndex = uxIndex - 1U;

        /* Lab4: create tasks in a deterministic order so equal-time startup
         * behavior is repeatable across demo runs.
         */
        ( void ) xTaskCreate( prvEDFTask,
                              "EDF",
                              mainLAB4_TASK_STACK_SIZE,
                              ( void * ) &( xTaskConfigs[ uxConfigIndex ] ),
                              mainLAB4_TASK_PRIORITY,
                              &( xTaskHandles[ uxConfigIndex ] ) );

        vTaskSetEDFParameters( xTaskHandles[ uxConfigIndex ],
                               xTaskConfigs[ uxConfigIndex ].uxTaskNumber,
                               xTaskConfigs[ uxConfigIndex ].xPeriod,
                               xTaskConfigs[ uxConfigIndex ].xPeriod );
    }

    /* Lab4: Reporter is not an EDF task.  It wakes after the test window and prints
     * the events recorded by the kernel hooks.
     */
    ( void ) xTaskCreate( prvReporterTask,
                          "Reporter",
                          mainLAB4_REPORTER_STACK_SIZE,
                          NULL,
                          mainLAB4_TASK_PRIORITY + 1U,
                          NULL );

    vTaskStartScheduler();

    for( ;; );
}

static void prvEDFTask( void * pvParameters )
{
    const Lab4TaskConfig_t * const pxConfig = ( const Lab4TaskConfig_t * ) pvParameters;
    TickType_t xLastWakeTime = xLabStartTime;
    TickType_t xWorkDone;

    for( ;; )
    {
        /* Lab4: one loop iteration models one released periodic job. */
        vTaskStartEDFJob( NULL );

        /* Lab4: burn exactly c ticks of CPU time for this job. */
        for( xWorkDone = 0; xWorkDone < pxConfig->xExecutionTime; xWorkDone++ )
        {
            prvBurnExecutionTick();

            if( ( xTaskGetTickCount() - xLabStartTime ) >= mainLAB4_STOP_TICK )
            {
                xStopRequested = pdTRUE;
            }
        }

        /* Lab4: the job completed, so its next absolute deadline moves forward by
         * one period.  The scheduler uses this updated value on the next
         * context switch.
         */
        vTaskAdvanceEDFDeadline( NULL );

        if( xStopRequested != pdFALSE )
        {
            vTaskSuspend( NULL );
        }

        /* Lab4: block until the next period release.  While blocked, this task is no
         * longer in the ready list, so EDF will not choose it.
         */
        vTaskDelayUntil( &xLastWakeTime, pxConfig->xPeriod );
    }
}

/* Lab4: these hooks are called from the modified kernel scheduler whenever a context
 * switch should be shown as "preempt" or "complete" in the lab console output.
 */
void vApplicationEDFPreemptHook( UBaseType_t uxFromTask,
                                 UBaseType_t uxToTask,
                                 TickType_t xTime )
{
    prvRecordEvent( xTime - xLabStartTime, 'p', uxFromTask, uxToTask );
}

void vApplicationEDFCompleteHook( UBaseType_t uxFromTask,
                                  UBaseType_t uxToTask,
                                  TickType_t xTime )
{
    prvRecordEvent( xTime - xLabStartTime, 'c', uxFromTask, uxToTask );
}

static void prvReporterTask( void * pvParameters )
{
    UBaseType_t uxIndex;
    UBaseType_t uxCount;

    ( void ) pvParameters;

    vTaskDelay( mainLAB4_STOP_TICK + 5U );
    xStopRequested = pdTRUE;

    uxCount = uxEventCount;

    if( uxCount > mainLAB4_MAX_EVENTS )
    {
        uxCount = mainLAB4_MAX_EVENTS;
    }

    for( uxIndex = 0; uxIndex < uxCount; uxIndex++ )
    {
        prvPrintEvent( xEventLog[ uxIndex ].xTime,
                       xEventLog[ uxIndex ].cEvent,
                       xEventLog[ uxIndex ].uxFromTask,
                       xEventLog[ uxIndex ].uxToTask );
    }

    /* CCS CIO may buffer output; flush before the reporter task suspends. */
    fflush( NULL );

    vTaskSuspend( NULL );
}

static void prvRecordEvent( TickType_t xTime,
                            char cEvent,
                            UBaseType_t uxFromTask,
                            UBaseType_t uxToTask )
{
    UBaseType_t uxIndex;

    if( ( xTime <= mainLAB4_STOP_TICK ) && ( uxEventCount < mainLAB4_MAX_EVENTS ) )
    {
        uxIndex = uxEventCount;
        xEventLog[ uxIndex ].xTime = xTime;
        xEventLog[ uxIndex ].cEvent = cEvent;
        xEventLog[ uxIndex ].uxFromTask = uxFromTask;
        xEventLog[ uxIndex ].uxToTask = uxToTask;
        uxEventCount++;
    }
}

static void prvPrintEvent( TickType_t xTime,
                           char cEvent,
                           UBaseType_t uxFromTask,
                           UBaseType_t uxToTask )
{
    if( xTime <= mainLAB4_STOP_TICK )
    {
        if( cEvent == 'p' )
        {
            printf( "%d preempt  ", ( int ) xTime );
        }
        else
        {
            printf( "%d complete ", ( int ) xTime );
        }

        prvPrintTaskName( uxFromTask );
        printf( " " );
        prvPrintTaskName( uxToTask );
        printf( "\r\n" );
    }
}

static void prvPrintTaskName( UBaseType_t uxTaskNumber )
{
    if( uxTaskNumber == ( UBaseType_t ) 0U )
    {
        printf( "Idle" );
    }
    else
    {
        printf( "T%d", ( int ) uxTaskNumber );
    }
}

static void prvBurnExecutionTick( void )
{
    const TickType_t xStartTick = xTaskGetTickCount();

    while( xTaskGetTickCount() == xStartTick )
    {
        /* Burn CPU time until the next scheduler tick. */
    }
}
