/*
 *  Source generated with GadToolsBox V1.3
 *  which is (c) Copyright 1991,92 Jaba Development
 */

#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/imageclass.h>
#include <intuition/gadgetclass.h>
#include <libraries/gadtools.h>
#include <graphics/displayinfo.h>
#include <graphics/gfxbase.h>
#include <clib/exec_protos.h>
#include <clib/intuition_protos.h>
#include <clib/gadtools_protos.h>
#include <clib/graphics_protos.h>
#include <string.h>

#include "fmtwindow.h"

struct Screen        *Scr = NULL;
APTR                  VisualInfo = NULL;
struct Window        *MainWnd = NULL;
struct Window        *ParmWnd = NULL;
struct Gadget        *MainGList = NULL;
struct Gadget        *ParmGList = NULL;
UWORD                 MainZoom[4];
UWORD                 ParmZoom[4];
struct Gadget        *MainGadgets[5];
struct Gadget        *ParmGadgets[14];
UWORD                 MainLeft = 28;
UWORD                 MainTop = 19;
UWORD                 MainWidth = 420;
UWORD                 MainHeight = 88;
UWORD                 ParmLeft = 11;
UWORD                 ParmTop = 19;
UWORD                 ParmWidth = 522;
UWORD                 ParmHeight = 129;
UBYTE                *MainWdt = (UBYTE *)"MSH Format";
UBYTE                *ParmWdt = (UBYTE *)"MSH Format Parameters";

UBYTE         *FMT_WHAT0Labels[] = {
    (UBYTE *)"Bootblock only",
    (UBYTE *)"Bootblock + root dir",
    (UBYTE *)"Format whole disk",
    NULL };

struct TextAttr topaz8 = {
    ( STRPTR )"topaz.font", 8, 0x00, 0x01 };

int SetupScreen( void )
{
    if ( ! ( Scr = LockPubScreen( NULL )))
        return( 1L );

    if ( ! ( VisualInfo = GetVisualInfo( Scr, TAG_DONE )))
        return( 2L );

    return( 0L );
}

void CloseDownScreen( void )
{
    if ( VisualInfo ) {
        FreeVisualInfo( VisualInfo );
        VisualInfo = NULL;
    }

    if ( Scr        ) {
        UnlockPubScreen( NULL, Scr );
        Scr = NULL;
    }
}

int OpenMainWindow( void )
{
    struct NewGadget     ng;
    struct Gadget       *g;
    UWORD               offx, offy;

    offx = Scr->WBorLeft;
    offy = Scr->WBorTop + Scr->RastPort.TxHeight + 1;

    if ( ! ( g = CreateContext( &MainGList )))
        return( 1L );

    ng.ng_LeftEdge        =    offx + 12;
    ng.ng_TopEdge         =    offy + 16;
    ng.ng_Width           =    174;
    ng.ng_Height          =    64;
    ng.ng_GadgetText      =    (UBYTE *)"Format device:";
    ng.ng_TextAttr        =    &topaz8;
    ng.ng_GadgetID        =    GD_HANDLERS;
    ng.ng_Flags           =    PLACETEXT_ABOVE;
    ng.ng_VisualInfo      =    VisualInfo;

    g = CreateGadget( LISTVIEW_KIND, g, &ng, GTLV_Labels, NULL, GTLV_ShowSelected, NULL, TAG_DONE );

    MainGadgets[ 0 ] = g;

    ng.ng_LeftEdge        =    offx + 211;
    ng.ng_TopEdge         =    offy + 6;
    ng.ng_GadgetText      =    NULL;
    ng.ng_GadgetID        =    GD_FMT_WHAT;
    ng.ng_Flags           =    PLACETEXT_RIGHT;

    g = CreateGadget( MX_KIND, g, &ng, GTMX_Labels, &FMT_WHAT0Labels[0], GTMX_Spacing, 3, TAG_DONE );

    MainGadgets[ 1 ] = g;

    ng.ng_TopEdge         =    offy + 45;
    ng.ng_Width           =    91;
    ng.ng_Height          =    16;
    ng.ng_GadgetText      =    (UBYTE *)"Options";
    ng.ng_GadgetID        =    GD_OPTIONS;
    ng.ng_Flags           =    PLACETEXT_IN;

    g = CreateGadget( BUTTON_KIND, g, &ng, GA_Disabled, TRUE, TAG_DONE );

    MainGadgets[ 2 ] = g;

    ng.ng_TopEdge         =    offy + 67;
    ng.ng_GadgetText      =    (UBYTE *)"Format";
    ng.ng_GadgetID        =    GD_DOIT;

    g = CreateGadget( BUTTON_KIND, g, &ng, GA_Disabled, TRUE, TAG_DONE );

    MainGadgets[ 3 ] = g;

    ng.ng_LeftEdge        =    offx + 314;
    ng.ng_GadgetText      =    (UBYTE *)"Cancel";
    ng.ng_GadgetID        =    GD_CANCEL;

    g = CreateGadget( BUTTON_KIND, g, &ng, TAG_DONE );

    MainGadgets[ 4 ] = g;

    if ( ! g )
        return( 2L );

    MainZoom[0] = 0;
    MainZoom[1] = 0;
    MainZoom[2] = 168;
    MainZoom[3] = 11;

    if ( ! ( MainWnd = OpenWindowTags( NULL,
                    WA_Left,          MainLeft,
                    WA_Top,           MainTop,
                    WA_InnerWidth,    MainWidth,
                    WA_InnerHeight,   MainHeight,
                    WA_IDCMP,         IDCMP_CLOSEWINDOW|IDCMP_REFRESHWINDOW,
                    WA_Flags,         WFLG_DRAGBAR|WFLG_DEPTHGADGET|WFLG_CLOSEGADGET|WFLG_SMART_REFRESH,
                    WA_Gadgets,       MainGList,
                    WA_Title,         MainWdt,
                    WA_ScreenTitle,   "MSH Format",
                    WA_PubScreen,     Scr,
                    WA_Zoom,          MainZoom,
                    WA_AutoAdjust,    TRUE,
                    TAG_DONE )))
        return( 4L );

    MainZoom[0] = MainWnd->LeftEdge;
    MainZoom[1] = MainWnd->TopEdge;
    MainZoom[2] = MainWnd->Width;
    MainZoom[3] = MainWnd->Height;

    GT_RefreshWindow( MainWnd, NULL );

    return( 0L );
}

void CloseMainWindow( void )
{
    if ( MainWnd        ) {
        CloseWindow( MainWnd );
        MainWnd = NULL;
    }

    if ( MainGList      ) {
        FreeGadgets( MainGList );
        MainGList = NULL;
    }
}

int OpenParmWindow( void )
{
    struct NewGadget     ng;
    struct Gadget       *g;
    UWORD               offx, offy;

    offx = Scr->WBorLeft;
    offy = Scr->WBorTop + Scr->RastPort.TxHeight + 1;

    if ( ! ( g = CreateContext( &ParmGList )))
        return( 1L );

    ng.ng_LeftEdge        =    offx + 7;
    ng.ng_TopEdge         =    offy + 5;
    ng.ng_Width           =    53;
    ng.ng_Height          =    13;
    ng.ng_GadgetText      =    (UBYTE *)"Bytes per sector";
    ng.ng_TextAttr        =    &topaz8;
    ng.ng_GadgetID        =    GD_BPS;
    ng.ng_Flags           =    PLACETEXT_RIGHT;
    ng.ng_VisualInfo      =    VisualInfo;

    g = CreateGadget( STRING_KIND, g, &ng, GTST_MaxChars, 16, TAG_DONE );

    ParmGadgets[ 0 ] = g;

    ng.ng_TopEdge         =    offy + 20;
    ng.ng_GadgetText      =    (UBYTE *)"Sectors per track";
    ng.ng_GadgetID        =    GD_SPT;

    g = CreateGadget( STRING_KIND, g, &ng, GTST_MaxChars, 16, TAG_DONE );

    ParmGadgets[ 1 ] = g;

    ng.ng_TopEdge         =    offy + 35;
    ng.ng_GadgetText      =    (UBYTE *)"Number of sides";
    ng.ng_GadgetID        =    GD_NSIDES;

    g = CreateGadget( STRING_KIND, g, &ng, GTST_MaxChars, 16, TAG_DONE );

    ParmGadgets[ 2 ] = g;

    ng.ng_TopEdge         =    offy + 50;
    ng.ng_GadgetText      =    (UBYTE *)"Starting cylinder";
    ng.ng_GadgetID        =    GD_FIRSTCYL;

    g = CreateGadget( STRING_KIND, g, &ng, GTST_MaxChars, 16, TAG_DONE );

    ParmGadgets[ 3 ] = g;

    ng.ng_TopEdge         =    offy + 65;
    ng.ng_GadgetText      =    (UBYTE *)"Number of cylinders";
    ng.ng_GadgetID        =    GD_NCYLS;

    g = CreateGadget( STRING_KIND, g, &ng, GTST_MaxChars, 16, TAG_DONE );

    ParmGadgets[ 4 ] = g;

    ng.ng_TopEdge         =    offy + 80;
    ng.ng_GadgetText      =    (UBYTE *)"Boot sectors";
    ng.ng_GadgetID        =    GD_RESERVED;

    g = CreateGadget( STRING_KIND, g, &ng, GTST_MaxChars, 16, TAG_DONE );

    ParmGadgets[ 5 ] = g;

    ng.ng_TopEdge         =    offy + 95;
    ng.ng_GadgetText      =    (UBYTE *)"Number of hidden sectors";
    ng.ng_GadgetID        =    GD_NHID;

    g = CreateGadget( STRING_KIND, g, &ng, GTST_MaxChars, 16, TAG_DONE );

    ParmGadgets[ 6 ] = g;

    ng.ng_LeftEdge        =    offx + 273;
    ng.ng_TopEdge         =    offy + 5;
    ng.ng_GadgetText      =    (UBYTE *)"Sectors per cluster";
    ng.ng_GadgetID        =    GD_SPC;

    g = CreateGadget( STRING_KIND, g, &ng, GTST_MaxChars, 16, TAG_DONE );

    ParmGadgets[ 7 ] = g;

    ng.ng_TopEdge         =    offy + 20;
    ng.ng_GadgetText      =    (UBYTE *)"Number of FAT copies";
    ng.ng_GadgetID        =    GD_NFATS;

    g = CreateGadget( STRING_KIND, g, &ng, GTST_MaxChars, 16, TAG_DONE );

    ParmGadgets[ 8 ] = g;

    ng.ng_TopEdge         =    offy + 35;
    ng.ng_GadgetText      =    (UBYTE *)"Sectors per FAT";
    ng.ng_GadgetID        =    GD_SPF;

    g = CreateGadget( STRING_KIND, g, &ng, GTST_MaxChars, 16, TAG_DONE );

    ParmGadgets[ 9 ] = g;

    ng.ng_TopEdge         =    offy + 50;
    ng.ng_GadgetText      =    (UBYTE *)"Total number of sectors";
    ng.ng_GadgetID        =    GD_NSECTS;

    g = CreateGadget( STRING_KIND, g, &ng, GTST_MaxChars, 16, TAG_DONE );

    ParmGadgets[ 10 ] = g;

    ng.ng_TopEdge         =    offy + 80;
    ng.ng_GadgetText      =    (UBYTE *)"Media byte";
    ng.ng_GadgetID        =    GD_MEDIA;

    g = CreateGadget( STRING_KIND, g, &ng, GTST_MaxChars, 16, TAG_DONE );

    ParmGadgets[ 11 ] = g;

    ng.ng_TopEdge         =    offy + 95;
    ng.ng_GadgetText      =    (UBYTE *)"Root directory entries";
    ng.ng_GadgetID        =    GD_NDIRS;

    g = CreateGadget( STRING_KIND, g, &ng, GTST_MaxChars, 16, TAG_DONE );

    ParmGadgets[ 12 ] = g;

    ng.ng_LeftEdge        =    offx + 8;
    ng.ng_TopEdge         =    offy + 111;
    ng.ng_Width           =    504;
    ng.ng_Height          =    16;
    ng.ng_GadgetText      =    (UBYTE *)"OK";
    ng.ng_GadgetID        =    GD_OK;
    ng.ng_Flags           =    PLACETEXT_IN;

    g = CreateGadget( BUTTON_KIND, g, &ng, TAG_DONE );

    ParmGadgets[ 13 ] = g;

    if ( ! g )
        return( 2L );

    ParmZoom[0] = 160;
    ParmZoom[1] = 0;
    ParmZoom[2] = 248;
    ParmZoom[3] = 11;

    if ( ! ( ParmWnd = OpenWindowTags( NULL,
                    WA_Left,          ParmLeft,
                    WA_Top,           ParmTop,
                    WA_InnerWidth,    ParmWidth,
                    WA_InnerHeight,   ParmHeight,
                    WA_IDCMP,         IDCMP_CLOSEWINDOW|IDCMP_REFRESHWINDOW,
                    WA_Flags,         WFLG_DRAGBAR|WFLG_DEPTHGADGET|WFLG_CLOSEGADGET|WFLG_SMART_REFRESH,
                    WA_Gadgets,       ParmGList,
                    WA_Title,         ParmWdt,
                    WA_ScreenTitle,   "MSH Format",
                    WA_PubScreen,     Scr,
                    WA_Zoom,          ParmZoom,
                    TAG_DONE )))
        return( 4L );

    ParmZoom[0] = ParmWnd->LeftEdge;
    ParmZoom[1] = ParmWnd->TopEdge;
    ParmZoom[2] = ParmWnd->Width;
    ParmZoom[3] = ParmWnd->Height;

    GT_RefreshWindow( ParmWnd, NULL );

    return( 0L );
}

void CloseParmWindow( void )
{
    if ( ParmWnd        ) {
        CloseWindow( ParmWnd );
        ParmWnd = NULL;
    }

    if ( ParmGList      ) {
        FreeGadgets( ParmGList );
        ParmGList = NULL;
    }
}

