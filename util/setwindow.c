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

#include "setwindow.h"

struct Screen	     *Scr = NULL;
APTR		      VisualInfo = NULL;
struct Window	     *MSHSettingsWnd = NULL;
struct Gadget	     *MSHSettingsGList = NULL;
struct Menu	     *MSHSettingsMenus = NULL;
UWORD		      MSHSettingsZoom[4];
struct Gadget	     *MSHSettingsGadgets[8];
UWORD		      MSHSettingsLeft = 10;
UWORD		      MSHSettingsTop = 14;
UWORD		      MSHSettingsWidth = 447;
UWORD		      MSHSettingsHeight = 92;
UBYTE		     *MSHSettingsWdt = (UBYTE *)"MSH Settings";

UBYTE	      *CONVERSION0Labels[] = {
    (UBYTE *)"None",
    (UBYTE *)"A: PC Ascii",
    (UBYTE *)"B: ST Ascii",
    NULL };

struct TextAttr topaz8 = {
    ( STRPTR )"topaz.font", 8, 0x00, 0x01 };

struct NewMenu MSHSettingsNewMenu[] = {
    NM_TITLE, (STRPTR)"Project", NULL, 0, 0L, NULL,
    NM_ITEM, (STRPTR)"Hide", (STRPTR)"H", NM_ITEMDISABLED, 0L, NULL,
    NM_ITEM, (STRPTR)"Quit", (STRPTR)"Q", 0, 0L, NULL,
    NM_END, NULL, NULL, 0, 0L, NULL };

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

int OpenMSHSettingsWindow( void )
{
    struct NewGadget	 ng;
    struct Gadget	*g;
    UWORD		offx, offy;

    offx = Scr->WBorLeft;
    offy = Scr->WBorTop + Scr->RastPort.TxHeight + 1;

    if ( ! ( g = CreateContext( &MSHSettingsGList )))
	return( 1L );

    ng.ng_LeftEdge	  =    offx + 206;
    ng.ng_TopEdge	  =    offy + 18;
    ng.ng_Width 	  =    26;
    ng.ng_Height	  =    11;
    ng.ng_GadgetText	  =    (UBYTE *)"Check JUMP in bootblock";
    ng.ng_TextAttr	  =    &topaz8;
    ng.ng_GadgetID	  =    GD_BOOTJMP;
    ng.ng_Flags 	  =    PLACETEXT_RIGHT;
    ng.ng_VisualInfo	  =    VisualInfo;

    g = CreateGadget( CHECKBOX_KIND, g, &ng, GTCB_Checked, TRUE, TAG_DONE );

    MSHSettingsGadgets[ 0 ] = g;

    ng.ng_TopEdge	  =    offy + 4;
    ng.ng_GadgetText	  =    (UBYTE *)"40 track mode";
    ng.ng_GadgetID	  =    GD_MD40TRACK;

    g = CreateGadget( CHECKBOX_KIND, g, &ng, TAG_DONE );

    MSHSettingsGadgets[ 1 ] = g;

    ng.ng_LeftEdge	  =    offx + 222;
    ng.ng_TopEdge	  =    offy + 46;
    ng.ng_GadgetText	  =    (UBYTE *)"Check bootblock sanity";
    ng.ng_GadgetID	  =    GD_SANITY;

    g = CreateGadget( CHECKBOX_KIND, g, &ng, GTCB_Checked, TRUE, TAG_DONE );

    MSHSettingsGadgets[ 2 ] = g;

    ng.ng_LeftEdge	  =    offx + 238;
    ng.ng_TopEdge	  =    offy + 60;
    ng.ng_GadgetText	  =    (UBYTE *)"Not sane -> defaults";
    ng.ng_GadgetID	  =    GD_SAN_DEFAULT;

    g = CreateGadget( CHECKBOX_KIND, g, &ng, GTCB_Checked, TRUE, TAG_DONE );

    MSHSettingsGadgets[ 3 ] = g;

    ng.ng_LeftEdge	  =    offx + 206;
    ng.ng_TopEdge	  =    offy + 32;
    ng.ng_GadgetText	  =    (UBYTE *)"Always default bootblock";
    ng.ng_GadgetID	  =    GD_USE_DEFAULT;

    g = CreateGadget( CHECKBOX_KIND, g, &ng, TAG_DONE );

    MSHSettingsGadgets[ 4 ] = g;

    ng.ng_LeftEdge	  =    offx + 33;
    ng.ng_TopEdge	  =    offy + 74;
    ng.ng_Width 	  =    121;
    ng.ng_Height	  =    12;
    ng.ng_GadgetText	  =    (UBYTE *)"Default conversion";
    ng.ng_GadgetID	  =    GD_CONVERSION;
    ng.ng_Flags 	  =    PLACETEXT_ABOVE;

    g = CreateGadget( CYCLE_KIND, g, &ng, GTCY_Labels, &CONVERSION0Labels[0], TAG_DONE );

    MSHSettingsGadgets[ 5 ] = g;

    ng.ng_LeftEdge	  =    offx + 171;
    ng.ng_Width 	  =    57;
    ng.ng_GadgetText	  =    (UBYTE *)"Load";
    ng.ng_GadgetID	  =    GD_LOAD;
    ng.ng_Flags 	  =    PLACETEXT_IN;

    g = CreateGadget( BUTTON_KIND, g, &ng, TAG_DONE );

    MSHSettingsGadgets[ 6 ] = g;

    ng.ng_LeftEdge	  =    offx + 16;
    ng.ng_TopEdge	  =    offy + 18;
    ng.ng_Width 	  =    158;
    ng.ng_Height	  =    40;
    ng.ng_GadgetText	  =    (UBYTE *)"Available handlers";
    ng.ng_GadgetID	  =    GD_HANDLERS;
    ng.ng_Flags 	  =    PLACETEXT_ABOVE;

    g = CreateGadget( LISTVIEW_KIND, g, &ng, GTLV_Labels, NULL, GTLV_ShowSelected, NULL, TAG_DONE );

    MSHSettingsGadgets[ 7 ] = g;

    if ( ! g )
	return( 2L );

    if ( ! ( MSHSettingsMenus = CreateMenus( MSHSettingsNewMenu, GTMN_FrontPen, 0L, TAG_DONE )))
	return( 3L );

    LayoutMenus( MSHSettingsMenus, VisualInfo, GTMN_TextAttr, &topaz8, TAG_DONE );

    MSHSettingsZoom[0] = 0;
    MSHSettingsZoom[1] = 0;
    MSHSettingsZoom[2] = 176;
    MSHSettingsZoom[3] = 11;

    if ( ! ( MSHSettingsWnd = OpenWindowTags( NULL,
		    WA_Left,	      MSHSettingsLeft,
		    WA_Top,	      MSHSettingsTop,
		    WA_InnerWidth,    MSHSettingsWidth,
		    WA_InnerHeight,   MSHSettingsHeight,
		    WA_IDCMP,	      IDCMP_MENUPICK|IDCMP_CLOSEWINDOW|IDCMP_DISKINSERTED|IDCMP_DISKREMOVED|IDCMP_REFRESHWINDOW,
		    WA_Flags,	      WFLG_DRAGBAR|WFLG_DEPTHGADGET|WFLG_CLOSEGADGET|WFLG_SMART_REFRESH|WFLG_ACTIVATE,
		    WA_Gadgets,       MSHSettingsGList,
		    WA_Title,	      MSHSettingsWdt,
		    WA_ScreenTitle,   "MSH Settings",
		    WA_PubScreen,     Scr,
		    WA_Zoom,	      MSHSettingsZoom,
		    WA_AutoAdjust,    TRUE,
		    TAG_DONE )))
	return( 4L );

    MSHSettingsZoom[0] = MSHSettingsWnd->LeftEdge;
    MSHSettingsZoom[1] = MSHSettingsWnd->TopEdge;
    MSHSettingsZoom[2] = MSHSettingsWnd->Width;
    MSHSettingsZoom[3] = MSHSettingsWnd->Height;

    SetMenuStrip( MSHSettingsWnd, MSHSettingsMenus );
    GT_RefreshWindow( MSHSettingsWnd, NULL );

    return( 0L );
}

void CloseMSHSettingsWindow( void )
{
    if ( MSHSettingsMenus      ) {
	ClearMenuStrip( MSHSettingsWnd );
	FreeMenus( MSHSettingsMenus );
	MSHSettingsMenus = NULL;    }

    if ( MSHSettingsWnd        ) {
	CloseWindow( MSHSettingsWnd );
	MSHSettingsWnd = NULL;
    }

    if ( MSHSettingsGList      ) {
	FreeGadgets( MSHSettingsGList );
	MSHSettingsGList = NULL;
    }
}

