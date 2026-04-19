#include <stdio.h>
#include "includes.h"

#define TASK_SET            1

#define TASK_STACKSIZE   2048
#define TASK_START_PRIO     0

#if TASK_SET == 1
#define MAX_PRINT_EVENTS   50
#define SIM_TICKS          35
#else
#define MAX_PRINT_EVENTS   50
#define SIM_TICKS          31
#endif

#define MAX_EVENTS         64

static OS_STK task_start_stk[TASK_STACKSIZE];
static OS_STK task1_stk[TASK_STACKSIZE];
static OS_STK task2_stk[TASK_STACKSIZE];
static OS_STK task3_stk[TASK_STACKSIZE];

EVENT_ENTRY EventBuf[MAX_EVENTS];
INT8U       EventCount;
INT8U       SimDone;
INT8U       DeadlineFlag;
INT32U      DeadlineTime;
INT8U       DeadlineTaskPrio;

static void TaskStart(void *pdata);
static void PeriodicTask(void *pdata);

/*
 * Preserve the Lab1 board-side bridge so tracked -> hidden -> tracked
 * transitions still collapse into one logical scheduling edge.
 */
static INT8U  BridgeValid;
static INT32U BridgeTime;
static INT8U  BridgeEvent;
static INT8U  BridgeFrom;

static INT8U IsTrackedPrio(INT8U prio)
{
    if ((prio == TASK_START_PRIO) || (prio == 1u) || (prio == 2u) || (prio == OS_TASK_IDLE_PRIO)) {
        return OS_TRUE;
    }
#if TASK_SET == 2
    if (prio == 3u) {
        return OS_TRUE;
    }
#endif
    return OS_FALSE;
}

void LogEvent(INT32U time, INT8U event, INT8U from, INT8U to)
{
    INT8U fromTracked;
    INT8U toTracked;

    if (SimDone == OS_TRUE) {
        return;
    }
    if (EventCount >= MAX_EVENTS) {
        return;
    }
    if (to == TASK_START_PRIO) {
        return;
    }

    fromTracked = IsTrackedPrio(from);
    toTracked   = IsTrackedPrio(to);

    if ((fromTracked == OS_TRUE) && (toTracked == OS_TRUE)) {
        BridgeValid = OS_FALSE;
    } else if ((fromTracked == OS_TRUE) && (toTracked == OS_FALSE)) {
        BridgeValid = OS_TRUE;
        BridgeTime  = time;
        BridgeEvent = event;
        BridgeFrom  = from;
        return;
    } else if ((fromTracked == OS_FALSE) && (toTracked == OS_TRUE)) {
        if ((BridgeValid == OS_TRUE) && (BridgeTime == time)) {
            from = BridgeFrom;
            event = BridgeEvent;
            BridgeValid = OS_FALSE;
        } else {
            return;
        }
    } else {
        return;
    }

    if (from == to) {
        return;
    }

    EventBuf[EventCount].time  = time;
    EventBuf[EventCount].event = event;
    EventBuf[EventCount].from  = from;
    EventBuf[EventCount].to    = to;
    EventCount++;

#if TASK_SET == 1
    if (EventCount >= MAX_PRINT_EVENTS) {
        SimDone = OS_TRUE;
    }
#endif
}

static void PrintEvents(void)
{
    INT8U i;

    for (i = 0u; i < EventCount; i++) {
        if (EventBuf[i].event == EVENT_PREEMPT) {
            printf("%lu Preempt %u %u\n",
                   (unsigned long)EventBuf[i].time,
                   (unsigned)EventBuf[i].from,
                   (unsigned)EventBuf[i].to);
        } else {
            printf("%lu Complete %u %u\n",
                   (unsigned long)EventBuf[i].time,
                   (unsigned)EventBuf[i].from,
                   (unsigned)EventBuf[i].to);
        }
    }
}

int main(void)
{
    OSInit();

    EventCount       = 0u;
    SimDone          = OS_FALSE;
    DeadlineFlag     = OS_FALSE;
    DeadlineTime     = 0u;
    DeadlineTaskPrio = 0u;
    BridgeValid      = OS_FALSE;
    BridgeTime       = 0u;
    BridgeEvent      = 0u;
    BridgeFrom       = 0u;

    OSTaskCreateExt(TaskStart,
                    (void *)0,
                    (OS_STK *)&task_start_stk[TASK_STACKSIZE - 1],
                    TASK_START_PRIO,
                    TASK_START_PRIO,
                    (OS_STK *)&task_start_stk[0],
                    TASK_STACKSIZE,
                    (void *)0,
                    OS_TASK_OPT_STK_CHK | OS_TASK_OPT_STK_CLR);

    OSStart();
    return 0;
}

static void TaskStart(void *pdata)
{
    OS_TCB *ptcb;
    (void)pdata;

    OSSchedLock();

    OSTaskCreateExt(PeriodicTask,
                    (void *)1,
                    (OS_STK *)&task1_stk[TASK_STACKSIZE - 1],
                    1u,
                    1u,
                    (OS_STK *)&task1_stk[0],
                    TASK_STACKSIZE,
                    (void *)0,
                    OS_TASK_OPT_STK_CHK | OS_TASK_OPT_STK_CLR);
    ptcb = OSTCBPrioTbl[1];
    ptcb->OSTCBCompTime    = 1u;
    ptcb->OSTCBCompTimeMax = 1u;
#if TASK_SET == 1
    ptcb->OSTCBPeriod      = 3u;
    ptcb->OSTCBDeadline    = 3u;
#else
    ptcb->OSTCBPeriod      = 4u;
    ptcb->OSTCBDeadline    = 4u;
#endif
    ptcb->OSTCBIsPeriodic  = OS_TRUE;

    OSTaskCreateExt(PeriodicTask,
                    (void *)2,
                    (OS_STK *)&task2_stk[TASK_STACKSIZE - 1],
                    2u,
                    2u,
                    (OS_STK *)&task2_stk[0],
                    TASK_STACKSIZE,
                    (void *)0,
                    OS_TASK_OPT_STK_CHK | OS_TASK_OPT_STK_CLR);
    ptcb = OSTCBPrioTbl[2];
#if TASK_SET == 1
    ptcb->OSTCBCompTime    = 3u;
    ptcb->OSTCBCompTimeMax = 3u;
    ptcb->OSTCBPeriod      = 5u;
    ptcb->OSTCBDeadline    = 5u;
#else
    ptcb->OSTCBCompTime    = 2u;
    ptcb->OSTCBCompTimeMax = 2u;
    ptcb->OSTCBPeriod      = 5u;
    ptcb->OSTCBDeadline    = 5u;
#endif
    ptcb->OSTCBIsPeriodic  = OS_TRUE;

#if TASK_SET == 2
    OSTaskCreateExt(PeriodicTask,
                    (void *)3,
                    (OS_STK *)&task3_stk[TASK_STACKSIZE - 1],
                    3u,
                    3u,
                    (OS_STK *)&task3_stk[0],
                    TASK_STACKSIZE,
                    (void *)0,
                    OS_TASK_OPT_STK_CHK | OS_TASK_OPT_STK_CLR);
    ptcb = OSTCBPrioTbl[3];
    ptcb->OSTCBCompTime    = 2u;
    ptcb->OSTCBCompTimeMax = 2u;
    ptcb->OSTCBPeriod      = 10u;
    ptcb->OSTCBDeadline    = 10u;
    ptcb->OSTCBIsPeriodic  = OS_TRUE;
#endif

    OSTimeSet(0u);
    OSSchedUnlock();

    OSTimeDly(SIM_TICKS);
    PrintEvents();

    for (;;) {
        OSTimeDly(1000u);
    }
}

static void PeriodicTask(void *pdata)
{
#if OS_CRITICAL_METHOD == 3
    OS_CPU_SR cpu_sr = 0u;
#endif
    INT8U  prio;
    OS_TCB *me;

    prio = (INT8U)(INT32U)pdata;
    me   = OSTCBPrioTbl[prio];

    for (;;) {
        while (((volatile OS_TCB *)me)->OSTCBCompTime > 0u) {
        }

        OS_ENTER_CRITICAL();
        me->OSTCBDeadline += me->OSTCBPeriod;
        if ((OSRdyTbl[me->OSTCBY] &= ~me->OSTCBBitX) == 0u) {
            OSRdyGrp &= ~me->OSTCBBitY;
        }
        OS_EXIT_CRITICAL();

        OS_Sched();
    }
}
