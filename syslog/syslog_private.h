/*-
 * $Id: syslog_private.h,v 1.1 1994/10/24 20:38:56 Rhialto Exp $
 *
 * $Log: syslog_private.h,v $
 * Revision 1.1  1994/10/24  20:38:56  Rhialto
 * Initial revision
 *
 * DEBUGGING LOG DEAMON PRIVATE DEFINITIONS
-*/

#include "syslog.h"

#define MAXLOGS 	4

struct LogFile {
    unsigned long   lf_FileHandle;
    long	    lf_Flags;
    char	    lf_FileName[80];
};

#define LF_DISABLED	0x01

struct LogPort {
    struct MsgPort  lp_MsgPort;
    short	    lp_OpenCount;
    struct LogFile  lp_LogFile[MAXLOGS];
};

#define DEBUGSIGNAL	    2
#define DEBUGSIGMASK	    (1L << DEBUGSIGNAL)

#define SYSLOG_PRINT	    0
#define SYSLOG_START	    1
#define SYSLOG_END	    2
#define SYSLOG_QUIT	    3

struct LogMsg {
    struct Message  lm_Msg;
    char	   *lm_Format;
    void	   *lm_Args;
    long	    lm_Flags;
};

