/*-
 *  $Id: device.h,v 1.34 91/01/24 00:13:57 Rhialto Exp $
 *  $Log:	device.h,v $
 * Revision 1.40  91/03/03  17:56:47  Rhialto
 * Freeze for MAXON
 * 
 * Revision 1.34  91/01/24  00:13:57  Rhialto
 * Use TD_RAWWRITE under AmigaOS 2.0.
 *
 *  This code is (C) Copyright 1989 by Olaf Seibert. All rights reserved. May
 *  not be used or copied without a licence.
 *
 *  This code is (C) Copyright 1989-1993 by Olaf Seibert. All rights reserved.

#define TRACKS(cyls)    ((cyls) * NUMHEADS)
#define VERSION 	34L
#define REVISION	8

#asm
VERSION     equ 	34
RTPRI	    equ 	0
#endasm

#define VERSION 	SYS2_04
#define REVISION	16

struct MessyDevice {
    long	    md_UseRawWrite;
    struct MessyUnit *md_Unit[MD_NUMUNITS];
    long	    md_System_2_04;
    struct SignalSemaphore md_HardwareUse;
    long	    md_RawbufferSize;
    byte	   *md_Rawbuffer;
    byte	    md_MfmDecode[128];
};

#define dev_Node	md_Dev.dd_Library.lib_Node
#define dev_Flags	md_Dev.dd_Library.lib_Flags
#define dev_NegSize	md_Dev.dd_Library.lib_NegSize
#define dev_PosSize	md_Dev.dd_Library.lib_PosSize
#define dev_Version	md_Dev.dd_Library.lib_Version
#define dev_Revision	md_Dev.dd_Library.lib_Revision
#define dev_IdString	md_Dev.dd_Library.lib_IdString
#define dev_OpenCnt	md_Dev.dd_Library.lib_OpenCnt

struct MessyUnit {
    struct MsgPort  mu_Port;
    short	    mu_OpenCnt;
    short	    mu_UnitNr;
    char	    mu_DiskState;
    ulong	    mu_ChangeNum;
    ulong	    mu_OpenFlags;
    byte	    mu_DiskState;
    short	    mu_CurrentCylinder; /* Position of the head, and */
    short	    mu_CurrentSide;	/* what's in the track buffer */
    short	    mu_CurrentSectors;	/* The current #sectors on this track */
    short	    mu_TrackChanged;
    short	    mu_ReadLen; 	/* 1 track + ~1 sector */
    short	    mu_WriteLen;	/* ~1 track */
    struct DiskResourceUnit mu_DRUnit;
    struct MsgPort  mu_DiskReplyPort;
    struct IOExtTD *mu_DiskIOReq;
    short	    mu_NumCyls;
    byte	    mu_TrackBuffer[MS_SPT_MAX * MS_BPS];   /* Must be word aligned */
    word	    mu_CrcBuffer[MS_SPT_MAX];
    char	    mu_SectorStatus[MS_SPT_MAX];
    byte	   *mu_TrackBuffer;
    char	    mu_SectorStatus[MS_SPT_MAX];
    word	    mu_CrcBuffer[MS_SPT_MAX];
};

#define     TDERR_NoError   0
#define     CRC_UNCHECKED   -1
#define     CRC_CHANGED     -2

#define UNITB_ACTIVE	0
#define UNITF_STOPPED	(1<<2)
#define UNITF_WAKETASK	(1<<3)

#define STATEF_PRESENT	(1<<1)
#define STATEF_WRITABLE (1<<2)
#define STATEF_HIGHDENSITY (1<<3)

#define SYS1_3		34	/* System version 1.3 */
/* Some constants related to #defines */
#asm
MS_BPS		    equ 512
MS_BPScode	    equ 2		    ; 2log(MSBPS/128)
LOG2_MS_BPS	    equ 9
MS_SPT		    equ 9
MS_SPT_MAX	    equ 10
#endasm

#define SYS2_0		36	/* System version 2.0 */
#define SYS2_04 	37	/* System version 2.04 */

typedef struct MessyDevice DEV;
typedef struct MessyUnit   UNIT;


#define TASKPRI     5L
#define TASKSTACK   1024L

/*
 *  Which of the device commands are real, and which are
/*  #define CMD_Invalid     /**/
/*  #define CMD_Reset	    /**/
/*  #define CMD_Read	    /**/
/*  #define CMD_Write	    /**/
/*  #define CMD_Update	    /**/
/*  #define CMD_Clear	    /**/
/*  #define CMD_Stop	    /**/
/*  #define CMD_Start	    /**/
/*  #define CMD_Flush	    /**/
/*  #define CMD_Stop	    */
/*  #define TD_Seek	    /**/
/*  #define TD_Format	    /**/
    #define TD_Motor	    TrackdiskGateway
/*  #define TD_Changenum    /**/
/*  #define TD_Format	    */
    #define TD_Remove	    TrackdiskGateway
/*  #define TD_Changenum    */
    #define TD_Changestate  TrackdiskGateway
    #define TD_Protstatus   TrackdiskGateway
    #define TD_Rawread	    TrackdiskGateway
/*  #define TD_Addchangeint /**/
/*  #define TD_Remchangeint /**/
/*  #define TD_Remchangeint */
/*  #define TD_Getgeometry  */
    #define TD_Eject	    TrackdiskGateway

#define STRIP(cmd)  ((unsigned char)cmd)
#define IMMEDIATE   ((1<<CMD_INVALID)|(1<<CMD_RESET)|\
		     (1<<CMD_STOP)|(1<<CMD_START)|(1<<CMD_FLUSH)|\
		     (1L<<TD_ADDCHANGEINT))
extern DEV *MakeLibrary();
extern struct Task *FindTask();
extern struct Task *CreateTask();

#define Prototype extern
 *  Forward declarations:

/*
extern char EndCode;
extern void Init(), _DevOpen(), _DevClose(), _DevExpunge(), _LibNull();
extern void _DevBeginIO(), _DevAbortIO();

extern char DevName[], idString[];

extern DEV *CInit();
extern void DevOpen();
extern long DevClose(), DevExpunge();
extern void DevBeginIO(), TermIO();
extern long DevAbortIO();

extern void WakePort();
extern void UnitTask();

extern int DevInit();
extern UNIT *UnitInit();
extern void DiskChangeHandler();

extern void TrackdiskGateway();
extern void CMD_Invalid();
extern void CMD_Reset();
extern void CMD_Read();
extern void CMD_Write();
extern void CMD_Update();
extern void CMD_Clear();
extern void CMD_Stop();
extern void CMD_Start();
extern void CMD_Flush();
extern void TD_Seek();
extern void TD_Format();
extern void TD_Changenum();
extern void TD_Addchangeint();
extern void TD_Remchangeint();

extern struct DiskResource *OpenResource();
extern struct MsgPort *DeviceProc();
extern struct MsgPort *CreatePort();
extern struct IOExtTD *CreateExtIO();
extern void    *GetUnit(), *GetMsg();
extern long	Wait();
extern void    *AllocMem(), FreeMem();
extern byte    *index(), *rindex();

extern int	ReadTrack();
extern void	InitDecoding();
extern int	TDSeek();
extern long	MyDoIO();
extern int	TDMotorOn();
extern int	TDMotorOff();
extern int	TDGetNumCyls();
extern void    *GetDrive();
extern void	FreeDrive();
extern int	DevOpenUp();
extern int	DevCloseDown();
extern int	GetTrack();
extern int	CheckRequest();

#ifndef READONLY
extern int	InitWrite();
extern void	FreeBuffer();
extern void	EncodeTrack();
#endif
 */

#include "devproto.h"
