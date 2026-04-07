/*
*********************************************************************************************************
*                                                uC/OS-II
*                                          The Real-Time Kernel
*
*                           (c) Copyright 1992-1999, Jean J. Labrosse, Weston, FL
*                                           All Rights Reserved
*
*                                           MASTER INCLUDE FILE
*********************************************************************************************************
*/

#include    <stdio.h>
#include    <string.h>
#include    <ctype.h>
#include    <stdlib.h>
#include    <conio.h>
#include    <dos.h>
#include    <setjmp.h>

#include    "\software\ucos-ii\ix86l\bc45\os_cpu.h"
#include    "os_cfg.h"
#include    "\software\ucos-ii\source\ucos_ii.h"
#include    "\software\blocks\pc\bc45\pc.h"

/*
*********************************************************************************************************
*                                    LAB1 EVENT LOGGING INTERFACE
*********************************************************************************************************
*/

#define  EVENT_COMPLETE    0
#define  EVENT_PREEMPT     1

#define  MAX_EVENTS       50

typedef struct {
    INT32U  time;
    INT8U   event;
    INT8U   from;
    INT8U   to;
} EVENT_ENTRY;

extern EVENT_ENTRY  EventBuf[MAX_EVENTS];
extern INT8U        EventCount;
extern INT8U        SimDone;
extern INT8U        DeadlineFlag;
extern INT32U       DeadlineTime;
extern INT8U        DeadlineTaskPrio;

extern void         LogEvent(INT32U time, INT8U event, INT8U from, INT8U to);
