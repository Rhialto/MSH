;/*-
dcc -r -ms -proto -new -v mountshare.c -o mountshare
quit
*   ^^ This is PURE code!
*/
/*-
 * $Id$
 * $Log$
 *
 * SHARE.C
 *
 * This code is (C) Copyright 1992 by Olaf Seibert. All rights reserved.
 * May not be used or copied without a licence.
-*/

#include <functions.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif
#ifndef EXEC_MEMORY_H
#include <exec/memory.h>
#endif
#ifndef LIBRARIES_DOSEXTENS_H
#include <libraries/dosextens.h>
#endif
#ifndef LIBRARIES_FILEHANDLER_H
#include <libraries/filehandler.h>
#endif
#ifndef RESOURCES_FILESYSRES_H
#include <resources/filesysres.h>
#endif

extern struct DosLibrary *DOSBase;

typedef void   *(*fptr) (long *, void *);

struct DosInfo *DosInfo;
struct FileSysResource *FileSysResource;
char		CreateFileSysEntry,
		TryLoadHandler,
		TryHandlerNames = 1,
		TryDosType = 1,
		TryFileSysEntry;

#define nodb(x)   x
#define db(x) /* x */

void
printbstr(BPTR bstr)
{
    char	   *str = BADDR(bstr);

    if (bstr)
	fwrite(str + 1, str[0], 1, stdout);
    else
	printf("<NULL>");
}

void	       *
traverse_bcpl(fptr function, BPTR start, void *context)
{
    db(printf("traverse_bcpl: %x -> ", start);)
    start = BADDR(start);
    db(printf("%x\n", start);)
    while (start && (context = function((long *) start, context))) {
	start = BADDR(((long *) start)[0]);
	db(printf("traverse_bcpl: next is %x\n", start);)
    }
    return context;
}

struct c1 {
    char	   *name;
    struct DeviceNode *dn;
};

void	       *
compare_name(long *node, void *context)
{
    struct c1	   *c1 = (struct c1 *) context;
    struct DeviceNode *dnv = (struct DeviceNode *) node;
    char	   *dnvname = BADDR(dnv->dn_Name);

    db(printf("compare_name: ");
       printbstr(dnv->dn_Name);
       printf("\n");)
    if (dnv->dn_Type == DLT_DEVICE &&
	dnvname[0] == strlen(c1->name) &&
	strnicmp(c1->name, &dnvname[1], dnvname[0]) == 0) {
	c1->dn = dnv;
	db(printf("Type %ld Stack %ld Pri %ld Start %lx SegList %lx Task %lx GV %ld\n",
	       dnv->dn_Type,
	       dnv->dn_StackSize,
	       dnv->dn_Priority,
	       dnv->dn_Startup,
	       dnv->dn_SegList,
	       dnv->dn_Task,
	       dnv->dn_GlobalVec);)
	return NULL;
    }
    return context;
}

struct DeviceNode *
find_name(char *name)
{
    struct c1	    dn;

    dn.name = name;
    dn.dn = NULL;

    traverse_bcpl(compare_name, DosInfo->di_DevInfo, &dn);

    return dn.dn;
}

void
load_handler(struct DeviceNode *dn)
{
    char	    name[256];
    char	   *handler;

    if (!TryLoadHandler)
	return;

    nodb(if (dn->dn_SegList != 0) {
	printf("Handler already loaded!\n");
	return;
    })
    nodb(if (dn->dn_Task != 0) {
	printf("Handler already running!\n");
	return;
    })
    handler = BADDR(dn->dn_Handler);
    if (handler == 0) {
	printf("Device has no Handler file\n");
	return;
    }
    strncpy(name, handler + 1, *handler);
    name[*handler] = '\0';

    dn->dn_SegList = LoadSeg(name);
    if (dn->dn_SegList) {
	printf("%s loaded.\n", name);
    } else {
	printf("Loading %s failed: error %ld.\n", name, IoErr());
    }
}

/*
 * Try to get the DosType values from the mountlist. This is quite a
 * work!
 * We must be reasonable sure that the dn_Startup fields both refer
 * to a FileSysStarupMsg. They must both be pointers (and not some
 * small integral values, as used to distinguish CON and RAW, for
 * example). We also assume that fssm_Unit values <= 0xFFFF.
 */

unsigned long
get_dostype(struct DeviceNode *dn)
{
    struct FileSysStartupMsg *dnmsg;

    dnmsg = BADDR(dn->dn_Startup);
    if (dnmsg &&
	(long)dnmsg > 100 &&
	dnmsg->fssm_Unit <= 0x0000FFFF
    ) {
	long	       *dnenv = BADDR(dnmsg->fssm_Environ);

	/* actually struct DosEnvec */

	if (dnenv && dnenv[0] >= DE_DOSTYPE)
	    return dnenv[DE_DOSTYPE];
    }

#define NODOSTYPE	ID_NO_DISK_PRESENT
    return NODOSTYPE;
}

int
find_filesysentry(unsigned long dostype, struct DeviceNode *dn)
{
    if (TryFileSysEntry && FileSysResource) {
	struct FileSysEntry *fse,
		       *next;

	for (fse = FileSysResource->fsr_FileSysEntries.lh_Head;
	     next = fse->fse_Node.ln_Succ;
	     fse = next) {
	    db(printf("Name %lx %s DosType %lx Version %lx SegList %lx\n",
		fse->fse_Node.ln_Name,
		fse->fse_Node.ln_Name,
		fse->fse_DosType,
		fse->fse_Version,
		fse->fse_SegList);)
	    if (fse->fse_DosType == dostype) {
		/*
		 * Patch the data from the FileSysEntry into the
		 * DeviceNode.
		 */
		if (dn) {
		    long	   *dnl = (long *)&dn->dn_Type;
		    long	   *fsel = (long *)&fse->fse_Type;
		    unsigned long   patchflags = fse->fse_PatchFlags;

		    while (patchflags) {
			if (patchflags & 1) {
			    nodb(*dnl = *fsel;)
			}
			dnl++;
			fsel++;
			patchflags >>= 1;
		    }
		}

		return 1;	/* We found it */
	    }
	}
    }
    return 0;			/* Didn't find anything */
}

void
make_filesysentry(struct DeviceNode *master)
{
    unsigned long dostype;

    if (!CreateFileSysEntry) {
	return;
    }

    dostype = get_dostype(master);
    if (dostype != NODOSTYPE && find_filesysentry(dostype, NULL) == 0) {

	/*
	 * So, there is no appropriate FileSysEntry for this DosType.
	 * Let's create one!
	 */
	if (FileSysResource == NULL) {
	    FileSysResource = AllocMem(sizeof(struct FileSysResource),
				       MEMF_PUBLIC | MEMF_CLEAR);
	    if (FileSysResource) {
		NewList(&FileSysResource->fsr_FileSysEntries);
		AddResource(FileSysResource);
	    }
	}

	if (FileSysResource) {
	    struct FileSysEntry *fse;

	    fse = AllocMem(sizeof(*fse), MEMF_PUBLIC | MEMF_CLEAR);
	    if (fse) {
		fse->fse_DosType = dostype;
		fse->fse_SegList = master->dn_SegList;
#define patchbit(x) ((offsetof(struct FileSysEntry, x) - \
		      offsetof(struct FileSysEntry, fse_Type)) / sizeof(long))
		fse->fse_PatchFlags |= 1 << patchbit(fse_SegList);

		AddTail(&FileSysResource->fsr_FileSysEntries, &fse->fse_Node);

	    }
	}
    }
}

struct c2 {
    struct DeviceNode *master;
};

void	       *
change(long *node, void *context)
{
    struct c2	   *c2 = (struct c2 *) context;
    struct DeviceNode *dnv = (struct DeviceNode *) node;
    struct DeviceNode *master = c2->master;
    unsigned long   dnvDosType;

    db(printf("change: ");
       printbstr(dnv->dn_Name);
       printf("\n");)

    if (dnv->dn_Type != DLT_DEVICE || dnv->dn_Type != master->dn_Type) {
	db(printf("Not a device\n");)
	return context; 	/* keep looking */
    }
    nodb(if (dnv->dn_SegList != 0) {
	db(printf("Device has SegList\n");)
	return context; 	/* keep looking */
    })
    nodb(if (dnv->dn_Task != 0) {
	db(printf("Device has Task\n");)
	return context; 	/* keep looking */
    })
    /*
     * See if both have the same handler filename.
     */
    if (TryHandlerNames) {
	char	       *dnvhandler = BADDR(dnv->dn_Handler);
	char	       *masterhandler = BADDR(master->dn_Handler);

	db(printf("Handlers: '");
	   printbstr(dnv->dn_Handler);
	   printf("' - '");
	   printbstr(master->dn_Handler);
	   printf("'\n");)

	if (dnvhandler && masterhandler) {

	    if (strnicmp(&dnvhandler[0], &masterhandler[0], masterhandler[0])
		== 0) {
		if (dnv->dn_SegList == 0) {
		    dnv->dn_SegList = master->dn_SegList;

		    /* Inform the user what happened */
		    printbstr(master->dn_Name);
		    printf(" and ");
		    printbstr(dnv->dn_Name);
		    printf(" share handler ");
		    printbstr(dnv->dn_Handler);
		    printf(": SegList copied.\n");
		    goto done;
		}
	    }
	}
    }
    /*
     * Try if the DosType values from the mountlist match.
     */
    dnvDosType = get_dostype(dnv);
    if (TryDosType) {

	if (dnvDosType != NODOSTYPE && dnvDosType == get_dostype(master)) {
	    db(printf("Dostypes %08x match!\n", dnvDosType);)
	    if (dnv->dn_SegList == 0) {
		nodb(dnv->dn_SegList = master->dn_SegList;)

		/* Inform the user what happened */
		printbstr(master->dn_Name);
		printf(" and ");
		printbstr(dnv->dn_Name);
		printf(" have the same DosType (0x%08lx): SegList copied\n",
		    dnvDosType);
		goto done;
	    }
	}
    }
    /*
     * Try to find a FileSysResource with this DosType.
     */
    if (TryFileSysEntry && dnvDosType != NODOSTYPE) {
	if (find_filesysentry(dnvDosType, dnv)) {
	    /* Inform the user what happened */
	    printf("There is a FileSysEntry with DosType 0x%08lx: "
		   "DeviceNode of ",
		dnvDosType);
	    printbstr(dnv->dn_Name);
	    printf(" patched.\n");

	    goto done;
	}
    }
done:
    return context;
}

void
spread(struct DeviceNode * master)
{
    struct c2	    dn;

    dn.master = master;
    traverse_bcpl(change, DosInfo->di_DevInfo, &dn);
}

void
find_devlist(void)
{
    struct RootNode *rn = (struct RootNode *) DOSBase->dl_Root;

    DosInfo = (struct DosInfo *) BADDR(rn->rn_Info);
}

void
find_filesysr(void)
{
    FileSysResource = OpenResource(FSRNAME);
}

void
usage(void)
{
    printf("usage: MountShare {{[+-][lchdf]} {<devicename>}}\n"
	   " +l: load Handler/FileSystem file\n"
	   " +c: create FileSysEntry\n"
	   " -h: don't compare Handler/FileSystem filenames\n"
	   " -d: don't compare DosType values\n"
	   " +f: search FileSysEntries\n"
	   "The option opposite to default is shown.\n");
    exit(10);
}

int
main(int argc, char **argv)
{
    int ac;
    char **av;

    ac = argc - 1;
    av = argv + 1;

    if (ac < 1)
	usage();

    find_devlist();
    find_filesysr();

    while (ac > 0) {
	if (av[0][0] == '-' || av[0][0] == '+') {
	    int val = av[0][0] == '+';

	    switch (av[0][1]) {
	    case 'l':
		TryLoadHandler = val;
		break;
	    case 'c':
		CreateFileSysEntry = val;
		break;
	    case 'h':
		TryHandlerNames = val;
		break;
	    case 'd':
		TryDosType = val;
		break;
	    case 'f':
		TryFileSysEntry = val;
		break;
	    default:
		usage();
	    }
	} else {
	    struct DeviceNode *master;

	    master = find_name(av[0]);
	    if (master) {
		load_handler(master);
		make_filesysentry(master);
		spread(master);
	    } else {
		printf("No device %s found.\n", av[0]);
	    }
	}
	ac--;
	av++;
    }
}
