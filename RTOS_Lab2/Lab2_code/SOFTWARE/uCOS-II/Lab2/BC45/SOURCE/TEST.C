/*
*********************************************************************************************************
*                                                uC/OS-II
*                                          The Real-Time Kernel
*
*                                       Lab2: EDF Scheduling Demo
*********************************************************************************************************
*/

#include "includes.h"

/*
*********************************************************************************************************
*                                            CONFIGURATION
*
*  Set TASK_SET to 1 for Set1, or 2 for Set2.
*********************************************************************************************************
*/

#define  TASK_SET                     1       /* 1 = Set1, 2 = Set2                                    */

/*
*********************************************************************************************************
*                                               CONSTANTS
*********************************************************************************************************
*/

#define  TASK_STK_SIZE              512
#define  TASK_START_PRIO              0       /* Startup task priority (highest)                        */

#if TASK_SET == 1
#define  MAX_PRINT_EVENTS            50       /* Stop limit                              */
#define  SIM_TICKS                   35       /* Run simulation for 35 ticks (2+ LCM)    */
#else
#define  MAX_PRINT_EVENTS            50       /* Stop limit                              */
#define  SIM_TICKS                   31       /* Run simulation for 31 ticks (1+ LCM)    */
#endif

/*
*********************************************************************************************************
*                                               VARIABLES
*********************************************************************************************************
*/

OS_STK  TaskStartStk[TASK_STK_SIZE];
OS_STK  Task1Stk[TASK_STK_SIZE];
OS_STK  Task2Stk[TASK_STK_SIZE];
OS_STK  Task3Stk[TASK_STK_SIZE];

/*--- Event logging buffer (defined here, extern'd in INCLUDES.H) ---*/
EVENT_ENTRY  EventBuf[MAX_EVENTS];
INT8U        EventCount;
INT8U        SimDone;
INT8U        DeadlineFlag;
INT32U       DeadlineTime;
INT8U        DeadlineTaskPrio;

/*
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*/

void  TaskStart(void *pdata);
void  PeriodicTask(void *pdata);

/*
*********************************************************************************************************
*                                           LogEvent
*
* Description: Logs a scheduling event to the shared buffer.
*              Called from OS_Sched (Complete) and OSIntExit (Preempt) in OS_CORE.C.
*              Protected by being called inside OS critical sections.
*********************************************************************************************************
*/

void  LogEvent (INT32U time, INT8U event, INT8U from, INT8U to)
{
    if (SimDone) {
        return;
    }
    if (EventCount >= MAX_EVENTS) {
        return;
    }
    /* Filter out context switches TO TaskStart (it only runs at end to print results) */
    if (to == TASK_START_PRIO) {
        return;
    }
    EventBuf[EventCount].time  = time;
    EventBuf[EventCount].event = event;
    EventBuf[EventCount].from  = from;
    EventBuf[EventCount].to    = to;
    EventCount++;
#if TASK_SET == 1
    if (EventCount >= MAX_PRINT_EVENTS) {
        SimDone = 1;
    }
#endif
}

/*
*********************************************************************************************************
*                                      GetTaskName
*
* Description: Returns task name string for a given priority.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                       PrintEvents
*
* Description: Prints all collected events from the buffer using bare priority numbers.
*              Called by the startup task after the simulation ends.
*********************************************************************************************************
*/

static void  PrintEvents (void)
{
    INT8U i;
    for (i = 0; i < EventCount; i++) {
        if (EventBuf[i].event == EVENT_PREEMPT) {
            printf("%lu Preempt %u %u\n",
                   EventBuf[i].time,
                   (unsigned)EventBuf[i].from,
                   (unsigned)EventBuf[i].to);
        } else {
            printf("%lu Complete %u %u\n",
                   EventBuf[i].time,
                   (unsigned)EventBuf[i].from,
                   (unsigned)EventBuf[i].to);
        }
    }
}

/*$PAGE*/
/*
*********************************************************************************************************
*                                                MAIN
*********************************************************************************************************
*/

void  main (void)
{
    PC_DispClrScr(DISP_FGND_WHITE + DISP_BGND_BLACK);

    OSInit();

    PC_DOSSaveReturn();
    PC_VectSet(uCOS, OSCtxSw);

    EventCount       = 0;
    SimDone          = 0;
    DeadlineFlag     = 0;
    DeadlineTime     = 0;
    DeadlineTaskPrio = 0;

    OSTaskCreate(TaskStart, (void *)0, &TaskStartStk[TASK_STK_SIZE - 1], TASK_START_PRIO);
    OSStart();
}

/*$PAGE*/
/*
*********************************************************************************************************
*                                           STARTUP TASK
*
* Description: Creates periodic tasks, installs tick ISR, waits for simulation
*              to complete, then prints results and returns to DOS.
*********************************************************************************************************
*/

void  TaskStart (void *pdata)
{
#if OS_CRITICAL_METHOD == 3
    OS_CPU_SR  cpu_sr;
#endif
    OS_TCB    *ptcb;

    pdata = pdata;

    OS_ENTER_CRITICAL();
    PC_VectSet(0x08, OSTickISR);
    PC_SetTickRate(OS_TICKS_PER_SEC);
    OS_EXIT_CRITICAL();

    /* Lock scheduler so task creation does not trigger premature context switches */
    OSSchedLock();

    /* Create task1: priority 1 */
    OSTaskCreate(PeriodicTask, (void *)0, &Task1Stk[TASK_STK_SIZE - 1], 1);
    ptcb = OSTCBPrioTbl[1];
#if TASK_SET == 1
    ptcb->OSTCBCompTime    = 1;
    ptcb->OSTCBCompTimeMax = 1;
    ptcb->OSTCBPeriod      = 3;
    /*----- Lab 2: EDF Scheduler Modification -----*/
    ptcb->OSTCBDeadline    = 3;
    /*---------------------------------------------*/
    ptcb->OSTCBIsPeriodic  = TRUE;
#else
    ptcb->OSTCBCompTime    = 1;
    ptcb->OSTCBCompTimeMax = 1;
    ptcb->OSTCBPeriod      = 4;
    /*----- Lab 2: EDF Scheduler Modification -----*/
    ptcb->OSTCBDeadline    = 4;
    /*---------------------------------------------*/
    ptcb->OSTCBIsPeriodic  = TRUE;
#endif

    /* Create task2: priority 2 */
    OSTaskCreate(PeriodicTask, (void *)0, &Task2Stk[TASK_STK_SIZE - 1], 2);
    ptcb = OSTCBPrioTbl[2];
#if TASK_SET == 1
    ptcb->OSTCBCompTime    = 3;
    ptcb->OSTCBCompTimeMax = 3;
    ptcb->OSTCBPeriod      = 5;
    /*----- Lab 2: EDF Scheduler Modification -----*/
    ptcb->OSTCBDeadline    = 5;
    /*---------------------------------------------*/
    ptcb->OSTCBIsPeriodic  = TRUE;
#else
    ptcb->OSTCBCompTime    = 2;
    ptcb->OSTCBCompTimeMax = 2;
    ptcb->OSTCBPeriod      = 5;
    /*----- Lab 2: EDF Scheduler Modification -----*/
    ptcb->OSTCBDeadline    = 5;
    /*---------------------------------------------*/
    ptcb->OSTCBIsPeriodic  = TRUE;
#endif

#if TASK_SET == 2
    /* Create task3: priority 3 */
    OSTaskCreate(PeriodicTask, (void *)0, &Task3Stk[TASK_STK_SIZE - 1], 3);
    ptcb = OSTCBPrioTbl[3];
    ptcb->OSTCBCompTime    = 2;
    ptcb->OSTCBCompTimeMax = 2;
    ptcb->OSTCBPeriod      = 10;
    /*----- Lab 2: EDF Scheduler Modification -----*/
    ptcb->OSTCBDeadline    = 10;
    /*---------------------------------------------*/
    ptcb->OSTCBIsPeriodic  = TRUE;
#endif

    /* Reset clock to 0 now that all tasks are created and ready.         */
    /* This eliminates any spurious tick that fired during task creation  */
    /* (PC_SetTickRate starts the timer before tasks are fully set up).  */
    OSTimeSet(0);

    /* Unlock scheduler — all tasks are now fully initialised.           */
    /* OSSchedUnlock() internally calls OS_Sched() but TaskStart (prio=0)*/
    /* remains the highest-priority ready task, so no switch occurs yet. */
    OSSchedUnlock();

    /* Wait for simulation to complete */
    OSTimeDly(SIM_TICKS);

    /* Print all collected events */
    PrintEvents();

    /* Wait for keypress before returning to DOS */
    printf("\nPress any key to exit...\n");
    getch();

    /* Process automated exit */
    PC_DOSReturn();
}

/*$PAGE*/
/*
*********************************************************************************************************
*                                         PERIODIC TASK
*
* Description: Generic periodic task body. Busy-waits while compTime > 0.
*              When compTime reaches 0, removes itself from the ready list
*              and calls OS_Sched() to yield (generates a Complete event).
*              OSTimeTick will re-enable this task at the next period boundary.
*********************************************************************************************************
*/

void  PeriodicTask (void *pdata)
{
#if OS_CRITICAL_METHOD == 3
    OS_CPU_SR  cpu_sr;
#endif

    pdata = pdata;

    for (;;) {
        /* Busy-wait while there is computation remaining */
        while (OSTCBCur->OSTCBCompTime > 0) {
            /* Spin: compTime is decremented by OSTimeTick */
        }

        /* Computation done for this period.              */
        /* Advance deadline, remove self from ready list and yield. */
        OS_ENTER_CRITICAL();
        /*----- Lab 2: EDF Scheduler Modification -----*/
        OSTCBCur->OSTCBDeadline += OSTCBCur->OSTCBPeriod; /* EDF: Advance deadline */
        /*---------------------------------------------*/
        if ((OSRdyTbl[OSTCBCur->OSTCBY] &= ~OSTCBCur->OSTCBBitX) == 0) {
            OSRdyGrp &= ~OSTCBCur->OSTCBBitY;
        }
        OS_EXIT_CRITICAL();

        OS_Sched();
        /* Returns here when OSTimeTick makes us ready at next period boundary */
    }
}
