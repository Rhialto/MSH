
/* MACHINE GENERATED */


/* hancrtso.a           */


/* pack.c               */

Prototype struct MsgPort *DosPort;
Prototype struct DeviceNode *DevNode;
Prototype struct DeviceList *VolNode;
Prototype short     DiskChanged;
Prototype long	    UnitNr;
Prototype char	   *DevName;
Prototype ulong     DevFlags;
Prototype long	    Interleave;
Prototype struct DosEnvec *Environ;
Prototype struct DosPacket *DosPacket;
Prototype short     Inhibited;
Prototype byte     *StackBottom;
Prototype struct DeviceList *NewVolNode(char *name, struct DateStamp *date);
Prototype int	    MayFreeVolNode(struct DeviceList *volnode);
Prototype void	    FreeVolNode(struct DeviceList *volnode);
Prototype void	    FreeVolNodeDeferred(void);
Prototype struct FileLock *NewFileLock(struct MSFileLock *msfl, struct FileLock *fl);
Prototype long	    FreeFileLock(struct FileLock *lock);
Prototype long	    DiskRemoved(void);
Prototype void	    DiskInserted(struct DeviceList *volnode);
Prototype void	    CheckDriveType(void);
Prototype struct DeviceList *WhichDiskInserted(void);
Prototype void	    DiskChange(void);
Prototype int	    CheckRead(struct FileLock *lock);
Prototype int	    CheckWrite(struct FileLock *lock);

/* support.c            */

Prototype void returnpacket(struct DosPacket *packet);
Prototype struct DosPacket *taskwait(struct Process *myproc);
Prototype long packetsqueued(void);
Prototype void *dosalloc(ulong bytes);
Prototype void dosfree(ulong *ptr);
Prototype void btos(byte *bstr, byte *buf);
Prototype void *GetHead(struct MinList *list);
Prototype void *GetTail(struct MinList *list);
Prototype char *typetostr(long ty);
Prototype void * NextNode(struct MinNode *node);

/* hanmain.c            */

Prototype byte ToUpper(byte ch);
Prototype long lmin(long a, long b);
Prototype byte *ZapSpaces(byte *begin, byte *end);
Prototype byte *ToMSName(byte *dest, byte *source);
Prototype long MSDiskInfo(struct InfoData *infodata);
Prototype void MSDiskInserted(struct LockList **locks, void *cookie);
Prototype int MSDiskRemoved(struct LockList **locks);
Prototype void InputDiskInserted(void);
Prototype void InputDiskRemoved(void);
Prototype void HanCloseDown(void);
Prototype int HanOpenUp(void);
Prototype long MSRelabel(byte *newname);
Prototype struct PrivateInfo *PrivateInfo(void);
Prototype byte		*ComponentEnd(char *source);

/* hansec.c             */

Prototype struct MsgPort *DiskReplyPort;
Prototype struct IOExtTD *DiskIOReq;
Prototype struct IOStdReq *DiskChangeReq;
Prototype struct DiskParam DefaultDisk;
Prototype struct DiskParam Disk;
Prototype struct Partition Partition;
Prototype byte *Fat;
Prototype short FatDirty;
Prototype short error;
Prototype long	IDDiskState;
Prototype long	IDDiskType;
Prototype struct timerequest *TimeIOReq;
Prototype int	MaxCache;
Prototype ulong BufMemType;
Prototype char	CacheDirty;
Prototype char	DelayCount;
Prototype short CheckBootBlock;
Prototype word	Get8086Word(byte *Word8086);
Prototype ulong	Get8086Long(byte *Long8086);
Prototype word	OtherEndianWord(long oew);     /* long should become word */
Prototype ulong OtherEndianLong(ulong oel);
Prototype sector_t	ClusterToSector(cluster_t cluster);
Prototype sector_t	ClusterOffsetToSector(cluster_t cluster, int offset);
Prototype sector_t	DirClusterToSector(cluster_t cluster);
Prototype cluster_t	SectorToCluster(sector_t sector);
Prototype cluster_t	NextCluster(cluster_t cluster);
Prototype sector_t	NextClusteredSector(sector_t sector);
Prototype sector_t	FindFreeSector(sector_t prev);
Prototype struct CacheSec *FindSecByNumber(sector_t number);
Prototype struct CacheSec *FindSecByBuffer(byte *buffer);
Prototype struct CacheSec *NewCacheSector(struct MinNode *pred);
Prototype void	FreeCacheSector(struct CacheSec *sec);
Prototype void	InitCacheList(void);
Prototype void	FreeCacheList(void);
Prototype void	MSUpdate(int immediate);
Prototype void	StartTimer(int);
Prototype byte *ReadSec(sector_t sector);
Prototype byte *EmptySec(sector_t sector);
Prototype void	WriteSec(sector_t sector, byte *data);
Prototype void	MayWriteTrack(struct CacheSec *cache);
Prototype void	FreeSec(byte *buffer);
Prototype void	MarkSecDirty(byte *buffer);
Prototype void	WriteFat(void);
Prototype int	AwaitDFx(void);
Prototype int	ReadBootBlock(void);
Prototype int	IdentifyDisk(char *name, struct DateStamp *date);
Prototype void	TDRemChangeInt(void);
Prototype int	TDAddChangeInt(struct Interrupt *interrupt);
Prototype int	TDChangeNum(void);
Prototype int	TDProtStatus(void);
Prototype int	TDMotorOff(void);
Prototype int	TDClear(void);
Prototype int	TDUpdate(void);
Prototype int   CheckETDCommands(void);
Prototype int	MyDoIO(struct IOStdReq *ioreq);
Prototype cluster_t PrevCluster(cluster_t cluster);
Prototype sector_t PrevDirSector(sector_t sector);

/* hanlock.c            */

Prototype void NextDirEntry(sector_t *sector, int *offset);
Prototype struct DirEntry *FindNext(struct DirEntry *previous, int createit);
Prototype void PrintDirEntry(struct DirEntry *de);
Prototype struct MSFileLock *MakeLock(struct MSFileLock *parentdir, struct DirEntry *dir, ulong mode);
Prototype struct MSFileLock *MSLock(struct MSFileLock *parentdir, byte *name, ulong mode);
Prototype struct MSFileLock *MSDupLock(struct MSFileLock *fl);
Prototype struct MSFileLock *MSParentDir(struct MSFileLock *fl);
Prototype long MSUnLock(struct MSFileLock *fl);
Prototype void ExamineDirEntry(struct MsDirEntry *msd, struct FileInfoBlock *fib, int checksum);
Prototype long MSExamine(struct MSFileLock *fl, struct FileInfoBlock *fib);
Prototype long MSExNext(struct MSFileLock *fl, struct FileInfoBlock *fib);
Prototype long ExamineDirEntries(sector_t sector, int offset, struct FileInfoBlock *fib);
Prototype long MSSetProtect(struct MSFileLock *parentdir, char *name, long mask);
Prototype int CheckLock(struct MSFileLock *lock);
Prototype void WriteDirtyFileLock(struct MSFileLock *fl);
Prototype void WriteFileLock(struct MSFileLock *fl);
Prototype void DirtyFileLock(struct MSFileLock *fl);
Prototype void UpdateFileLock(struct MSFileLock *fl);
Prototype struct LockList *NewLockList(void *cookie);
Prototype void FreeLockList(struct LockList *ll);
Prototype struct LockList *LockList;
Prototype struct MSFileLock *RootLock;
Prototype struct MSFileLock *EmptyFileLock;
Prototype const struct DirEntry FakeRootDirEntry;
Prototype const byte DotDot[1 + L_8 + L_3];
Prototype sector_t NextDirSector(sector_t sector);
Prototype void PrevDirEntry(sector_t  *sector, int *offset);

/* hanfile.c            */

Prototype int GetFat(void);
Prototype void FreeFat(void);
Prototype cluster_t GetFatEntry(cluster_t cluster);
Prototype void SetFatEntry(cluster_t cluster, cluster_t value);
Prototype cluster_t FindFreeCluster(cluster_t prev);
Prototype cluster_t ExtendClusterChain(cluster_t cluster);
Prototype void FreeClusterChain(cluster_t cluster);
Prototype struct MSFileHandle *MakeMSFileHandle(struct MSFileLock *fl, long mode);
Prototype struct MSFileHandle *MSOpen(struct MSFileLock *parentdir, char *name, long mode);
Prototype long MSClose(struct MSFileHandle *fh);
Prototype long FilePos(struct MSFileHandle *fh, long position, long mode);
Prototype void AdjustSeekPos(struct MSFileHandle *fh);
Prototype long MSSeek(struct MSFileHandle *fh, long position, long mode);
Prototype long MSRead(struct MSFileHandle *fh, byte *userbuffer, long size);
Prototype long MSWrite(struct MSFileHandle *fh, byte *userbuffer, long size);
Prototype long MSDeleteFile(struct MSFileLock *parentdir, byte *name);
Prototype long MSSetDate(struct MSFileLock *parentdir, byte *name, struct DateStamp *datestamp);
Prototype struct MSFileLock *MSCreateDir(struct MSFileLock *parentdir, byte *name);
Prototype long MSRename(struct MSFileLock *slock, byte *sname, struct MSFileLock *dlock, byte *dname);

/* hanconv.c            */

Prototype short 	    ConversionImbeddedInFileName;
Prototype short 	    DefaultConversion;
Prototype __shared unsigned char *Table_ToPC;
Prototype __shared unsigned char *Table_FromPC;
Prototype __shared unsigned char *Table_ToST;
Prototype __shared unsigned char *Table_FromST;
Prototype void		  ConvCleanUp(void);

/* hanreq.c             */

Prototype short Cancel; 	/* Cancel all R/W errors */
Prototype long	RetryRwError(struct IOExtTD *req);
Prototype void	DisplayMessage(char *msg);

/* hancmd.c             */

Prototype void HandleCommand(char *cmd);

/* date.c               */

Prototype void ToDateStamp(struct DateStamp *datestamp, word date, word time);
Prototype void ToMSDate(word *date, word *time, struct DateStamp *datestamp);

/* han2.c               */

Prototype long	MSSameLock(struct MSFileLock *lock1, struct MSFileLock *lock2);
Prototype struct MSFileHandle *MSOpenFromLock(struct MSFileLock *lock);
Prototype struct MSFileLock *MSDupLockFromFH(struct MSFileHandle *msfh);
Prototype struct MSFileLock *MSParentOfFH(struct MSFileHandle *msfh);
Prototype long	MSExamineFH(struct MSFileHandle *msfh, struct FileInfoBlock *fib);
Prototype long	MSSetFileSize(struct MSFileHandle *msfh, long pos, long mode);
Prototype long	MSChangeModeLock(struct MSFileLock *object, long newmode);
Prototype long	MSChangeModeFH(struct MSFileHandle *object, long newmode);
Prototype long	MSFormat(char *vol, long type);
Prototype long	MSSerializeDisk(void);

/* hanvfat.c            */

Prototype int CheckVfatSubentry(const byte *aminame, int amilength, const struct MsVfatSubEntry *se, int checksum);
Prototype int VfatChecksum(byte *name);
Prototype int ExamineVfatSubEntry(const struct MsVfatSubEntry *msd, struct FileInfoBlock *fib, int checksum);
Prototype int dos2amifn(byte dn[11], byte *un, int lower);
Prototype int ami2dosfn(byte *un, byte dn[L_8+L_3+1], int unlen, int gen);
Prototype int WriteLongName(struct DirEntry *de, int wincnt, byte *name, int componentlength);
Prototype int ToUniqueMSName(byte *component, struct MSFileLock *parentdir, byte *name, int longcomponentlength);
Prototype int winSlotCnt(byte *un, int unlen);
Prototype void EraseLongName(struct MSFileLock *fl);
Prototype void CleanupDirectory(struct MSFileLock *dir);
Prototype int CleanupDirectoryRec(struct MsDirEntry *nextmsd, sector_t sector);

/* dos.h                */


/* han.h                */


/* hanconv.h            */

