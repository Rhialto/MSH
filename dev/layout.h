/*-
 * $Id$
 * $Log$
 *
 *  This code is (C) Copyright 1989-1993 by Olaf Seibert. All rights reserved.
 *  May not be used or copied without a licence.
-*/

#define TLEN	12500	    /* In BYTES */
			    /* 2 microsec/bit, 200 ms/track -> 100000 bits */
#define RLEN	(TLEN+1324) /* 1 sector extra */
#define WLEN	(TLEN+20)   /* 20 bytes more than the theoretical track size */

#define SYNC	    0x4489  /* Normal sector sync */
#define INDEXSYNC   0x5224  /* Special sync for software index mark */

#define INDEXGAP	40  /* All these values are in WORDS */
#define INDXGAP 	12
#define INDXSYNC	 3
#define INDXMARK	 1
#define INDEXGAP2	40
#define INDEXLEN	(INDEXGAP+INDXGAP+INDXSYNC+INDXMARK+INDEXGAP2)

#define IDGAP2		12  /* Sector header: 22 words */
#define IDSYNC		 3
#define IDMARK		 1
#define IDDATA		 4
#define IDCRC		 2
#define IDLEN		(IDGAP2+IDSYNC+IDMARK+IDDATA+IDCRC)

#define DATAGAP1	22  /* Sector itself: 552 words */
#define DATAGAP2	12
#define DATASYNC	 3
#define DATAMARK	 1
#define DATACRC 	 2
#define DATAGAP3_9	78  /* for 9 or less sectors/track */
#define DATAGAP3_10	40  /* for 10 sectors/track */
#define DATALEN 	(DATAGAP1+DATAGAP2+DATASYNC+DATAMARK+MS_BPS+DATACRC)

#define BLOCKLEN	(IDLEN+DATALEN)     /* Total: 574 words */
