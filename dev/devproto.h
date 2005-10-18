
/* MACHINE GENERATED */


/* device1.a            */


/* device2.c            */

Prototype __geta4 DEV *Init(__A0 long segment, __D0 struct MessyDevice *dev, __A6 struct ExecBase *execbase);
Prototype __geta4 void DevOpen(__D0 ulong unitno, __D1 ulong flags, __A1 struct IOStdReq *ioreq, __A6 DEV *dev);
Prototype __geta4 long DevClose(__A1 struct IOStdReq *ioreq, __A6 DEV *dev);
Prototype __geta4 long DevExpunge(__A6 DEV *dev);
Prototype __geta4 void DevBeginIO(__A1 struct IOStdReq *ioreq, __A6 DEV *dev);
Prototype __geta4 long DevAbortIO(__A1 struct IOStdReq *ioreq, __A6 DEV *dev);
Prototype void TermIO(struct IOStdReq *ioreq);
Prototype void WakePort(struct MsgPort *port);
Prototype __geta4 void UnitTask(void);
Prototype void CMD_Invalid(struct IOStdReq *ioreq, UNIT *unit);
Prototype void CMD_Stop(struct IOStdReq *ioreq, UNIT *unit);
Prototype void CMD_Start(struct IOStdReq *ioreq, UNIT *unit);
Prototype void CMD_Flush(struct IOStdReq *ioreq, UNIT *unit);
Prototype void TrackdiskGateway(struct IOStdReq *ioreq, UNIT *unit);
Prototype const char DevName[];
Prototype const char idString[];

/* devio1.a             */


/* devio2.c             */

Prototype void CMD_Read(struct IOStdReq *ioreq, UNIT *unit);
Prototype void CMD_Write(struct IOStdReq *ioreq, UNIT *unit);
Prototype void TD_Format(struct IOStdReq *ioreq, UNIT *unit);
Prototype void CMD_Reset(struct IOStdReq *ioreq, UNIT *unit);
Prototype void CMD_Update(struct IOStdReq *ioreq, UNIT *unit);
Prototype void CMD_Clear(struct IOStdReq *ioreq, UNIT *unit);
Prototype void TD_Seek(struct IOStdReq *ioreq, UNIT *unit);
Prototype void TD_Changenum(struct IOStdReq *ioreq, UNIT *unit);
Prototype void TD_Addchangeint(struct IOStdReq *ioreq, UNIT *unit);
Prototype void TD_Remchangeint(struct IOStdReq *ioreq, UNIT *unit);
Prototype void TD_Getgeometry(struct IOStdReq *ioreq, UNIT *unit);
Prototype int DevInit(DEV *dev);
Prototype void InitDecoding(byte  *decode);
Prototype long MyDoIO(struct IORequest *req);
Prototype int TDMotorOn(struct IOExtTD *tdreq);
Prototype int TDGetNumTracks(struct IOExtTD *tdreq);
Prototype int TDSeek(UNIT *unit, int track);
Prototype void *GetDrive(struct DiskResourceUnit *drunit);
Prototype void FreeDrive(void);
Prototype int GetTrack(struct IOStdReq *ioreq, int track);
Prototype int CheckChanged(struct IOExtTD *ioreq, UNIT *unit);
Prototype int DevCloseDown(DEV *dev);
Prototype int CheckRequest(struct IOExtTD *ioreq, UNIT *unit);
Prototype UNIT *UnitInit(DEV *dev, ulong UnitNr, ulong Flags);
Prototype int UnitCloseDown(DEV *dev, UNIT *unit);
Prototype __geta4 void DiskChangeHandler(__A1 UNIT *unit);
Prototype void DiskChangeHandler0(void);
Prototype word CalculateGapLength(int sectors);
Prototype int ObtainRawBuffer(DEV *dev, UNIT *unit);
Prototype void FreeRawBuffer(DEV *dev);
Prototype void Internal_Update(struct IOStdReq *ioreq, UNIT *unit);
Prototype __stkargs void EncodeTrack(byte *TrackBuffer, byte *Rawbuffer, word *Crcs, long Cylinder, long Side, long GapLen, long NumSecs, long WriteLen);

/* dev.h                */


/* device.h             */


/* layout.h             */

