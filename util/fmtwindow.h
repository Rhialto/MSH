/*
 *  Source generated with GadToolsBox V1.3
 *  which is (c) Copyright 1991,92 Jaba Development
 */

#define GD_HANDLERS                            0
#define GD_FMT_WHAT                            1
#define GD_OPTIONS                             2
#define GD_DOIT                                3
#define GD_CANCEL                              4

#define GDX_HANDLERS                           0
#define GDX_FMT_WHAT                           1
#define GDX_OPTIONS                            2
#define GDX_DOIT                               3
#define GDX_CANCEL                             4

#define GD_BPS                                 40
#define GD_SPT                                 41
#define GD_NSIDES                              42
#define GD_FIRSTCYL                            43
#define GD_NCYLS                               44
#define GD_RESERVED                            45
#define GD_NHID                                46
#define GD_SPC                                 47
#define GD_NFATS                               48
#define GD_SPF                                 49
#define GD_NSECTS                              50
#define GD_MEDIA                               51
#define GD_NDIRS                               52
#define GD_OK                                  53

#define GDX_BPS                                0
#define GDX_SPT                                1
#define GDX_NSIDES                             2
#define GDX_FIRSTCYL                           3
#define GDX_NCYLS                              4
#define GDX_RESERVED                           5
#define GDX_NHID                               6
#define GDX_SPC                                7
#define GDX_NFATS                              8
#define GDX_SPF                                9
#define GDX_NSECTS                             10
#define GDX_MEDIA                              11
#define GDX_NDIRS                              12
#define GDX_OK                                 13

extern struct Screen        *Scr;
extern APTR                  VisualInfo;
extern struct Window        *MainWnd;
extern struct Window        *ParmWnd;
extern struct Gadget        *MainGList;
extern struct Gadget        *ParmGList;
extern UWORD                 MainZoom[4];
extern UWORD                 ParmZoom[4];
extern struct Gadget        *MainGadgets[5];
extern struct Gadget        *ParmGadgets[14];
extern UWORD                 MainLeft;
extern UWORD                 MainTop;
extern UWORD                 MainWidth;
extern UWORD                 MainHeight;
extern UWORD                 ParmLeft;
extern UWORD                 ParmTop;
extern UWORD                 ParmWidth;
extern UWORD                 ParmHeight;
extern UBYTE                *MainWdt;
extern UBYTE                *ParmWdt;
extern UBYTE                *FMT_WHAT0Labels[];
extern struct TextAttr       topaz8;

extern int SetupScreen( void );
extern void CloseDownScreen( void );
extern int OpenMainWindow( void );
extern void CloseMainWindow( void );
extern int OpenParmWindow( void );
extern void CloseParmWindow( void );
