/*
 *  Source generated with GadToolsBox V1.3
 *  which is (c) Copyright 1991,92 Jaba Development
 */

#define GD_BOOTJMP                             0
#define GD_MD40TRACK                           1
#define GD_SANITY                              2
#define GD_SAN_DEFAULT                         3
#define GD_USE_DEFAULT                         4
#define GD_CONVERSION                          5
#define GD_LOAD                                6
#define GD_HANDLERS                            7

#define GDX_BOOTJMP                            0
#define GDX_MD40TRACK                          1
#define GDX_SANITY                             2
#define GDX_SAN_DEFAULT                        3
#define GDX_USE_DEFAULT                        4
#define GDX_CONVERSION                         5
#define GDX_LOAD                               6
#define GDX_HANDLERS                           7

extern struct Screen        *Scr;
extern APTR                  VisualInfo;
extern struct Window        *MSHSettingsWnd;
extern struct Gadget        *MSHSettingsGList;
extern struct Menu          *MSHSettingsMenus;
extern UWORD                 MSHSettingsZoom[4];
extern struct Gadget        *MSHSettingsGadgets[8];
extern UWORD                 MSHSettingsLeft;
extern UWORD                 MSHSettingsTop;
extern UWORD                 MSHSettingsWidth;
extern UWORD                 MSHSettingsHeight;
extern UBYTE                *MSHSettingsWdt;
extern UBYTE                *CONVERSION0Labels[];
extern struct TextAttr       topaz8;
extern struct NewMenu        MSHSettingsNewMenu[];

extern int SetupScreen( void );
extern void CloseDownScreen( void );
extern int OpenMSHSettingsWindow( void );
extern void CloseMSHSettingsWindow( void );
