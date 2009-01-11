#if 1
#include <Carbon/Carbon.h>
#include <QuickTime/QuickTime.h>
#else
#include <Types.h>
#include <Memory.h>
#include <Quickdraw.h>
#include <Fonts.h>
#include <Events.h>
#include <Menus.h>
#include <TextEdit.h>
#include <MacWindows.h>
#if UNIVERSAL_INTERFACES_VERSION >= 0x0330
#include <ControlDefinitions.h>
#endif
#include <Dialogs.h>
#include <OSUtils.h>
#include <ToolUtils.h>
#include <Devices.h>
#include <StandardFile.h>
#include <Movies.h>
#include <Sound.h>
#include <QuickTimeComponents.h>
#include <ImageCompression.h>
#include <Resources.h>
#include <FixMath.h>
#endif

#include "SortFrames.h"
#include "Promiscuity.h"

extern void createWindow(void);
extern void idleWindow(void);
extern void destroyWindow(void);

Boolean gDone = false;
Boolean gDrewJPEG = false;
WindowPtr window = nil;

enum {
	kAppleMenuID			= 128,
		kAppleMenuAbout			= 1,
	kFileMenuID				= 129,
		kFileMenuClear			= 1,
		kFileMenuQuit			= 3
	
};

static void doMenu( long menuSelection )
{
	short whichMenu = HiWord(menuSelection);
	short whichMenuItem = LoWord(menuSelection);
	
	switch (whichMenu) {
		case kAppleMenuID:
			switch (whichMenuItem) {
				case kAppleMenuAbout:
					StandardAlert( 
							kAlertNoteAlert,
							"\pEtherPEG for Mac OS X",
							"\phttp://www.etherpeg.org",
							NULL,
							NULL );
					break;

				default:
#if ! TARGET_API_MAC_CARBON
					{
					Str255 daName;
					GetMenuItemText(GetMenuHandle(kAppleMenuID), whichMenuItem, daName);
					OpenDeskAcc(daName);
					}
#endif // not TARGET_API_MAC_CARBON
					break;
			}
			break;

		case kFileMenuID:
			switch (whichMenuItem) {
				case kFileMenuClear:
				{
					Rect r;
					SetPortWindowPort( window );
					GetPortBounds( GetWindowPort( window ), &r );
					EraseRect( &r );
				}
					break;
				case kFileMenuQuit:
					gDone = true;
					break;
			}
			break;
	}
	HiliteMenu(0);
}

static pascal OSErr handleQuitAE( const AppleEvent *inputEvent, AppleEvent *outputEvent, SInt32 handlerRefCon )
{
	inputEvent, outputEvent, handlerRefCon;
	ExitToShell();
	return noErr;
}
static void initializeAppleEvents(void)
{
	AEInstallEventHandler( kCoreEventClass, kAEQuitApplication, NewAEEventHandlerUPP( handleQuitAE ), 0, false );
}

int main( void )
{
	GrafPtr wmgrPort;
	long result = 0;
	
#if ! TARGET_API_MAC_CARBON
	InitGraf(&qd.thePort);
	InitFonts();
	InitWindows();
	InitMenus();
	TEInit();
	InitDialogs(nil);
	MaxApplZone();
#endif

	initializeAppleEvents();

	InitCursor();

#if TARGET_API_MAC_CARBON
	wmgrPort = CreateNewPort();
#else // not TARGET_API_MAC_CARBON
	GetWMgrPort( &wmgrPort );
#endif  // not TARGET_API_MAC_CARBON
	SetPort( wmgrPort );
	EnterMovies();

	SetMenuBar(GetNewMBar(128));
	AppendResMenu(GetMenuHandle(kAppleMenuID), 'DRVR');
	// Delete the Quit item under Aqua
	if( ( noErr == Gestalt( gestaltMenuMgrAttr, &result ) )
	 && ( result & gestaltMenuMgrAquaLayoutMask ) ) {
		DeleteMenuItem( GetMenuHandle( kFileMenuID ), kFileMenuQuit );
		DeleteMenuItem( GetMenuHandle( kFileMenuID ), kFileMenuQuit-1 );
	}
	DrawMenuBar();

	createStash();
	initPromiscuity();
	createWindow();
	
	while (gDone == false) {
		EventRecord theEvent;
		WindowPtr whichWindow;
		short windowPart;
		UInt32 timeoutTicks;

		idleWindow();
		
		gDrewJPEG = false;
		timeoutTicks = TickCount() + 5;
		while( !gDrewJPEG && ( TickCount() < timeoutTicks ) ) {
			idlePromiscuity();
		}
		
		WaitNextEvent(everyEvent, &theEvent, 0, nil);
		
		switch (theEvent.what) {
			case updateEvt:
				whichWindow = (WindowPtr)theEvent.message;
				SetPortWindowPort(whichWindow);
				BeginUpdate(whichWindow);
				//drawWindow(whichWindow);
				EndUpdate(whichWindow);
				break;
			
			case keyDown:
				if (theEvent.modifiers & cmdKey) {
					doMenu(MenuKey(theEvent.message & charCodeMask));
				}
				break;
			
			case mouseDown:
				windowPart = FindWindow(theEvent.where, &whichWindow);

				switch (windowPart) {
					case inDrag:
#if TARGET_API_MAC_CARBON
						DragWindow(whichWindow, theEvent.where, nil);
#else // not TARGET_API_MAC_CARBON
						DragWindow(whichWindow, theEvent.where, &qd.screenBits.bounds);
#endif // not TARGET_API_MAC_CARBON
						break;

					case inGoAway:
						if (TrackGoAway(whichWindow, theEvent.where))
							gDone = true;
						break;

					case inContent:
						if (whichWindow != FrontWindow())
						{
							SelectWindow(whichWindow);
						}
						else
						{
							//clickWindow(whichWindow, theEvent.where);
						}
						break;
					
					case inGrow:
						eraseBlobArea();
						ResizeWindow( whichWindow, theEvent.where, nil, nil );
						eraseBlobArea();
						break;

					case inMenuBar:
						doMenu(MenuSelect(theEvent.where));
						break;
				}
				break;
			
			case kHighLevelEvent:
				AEProcessAppleEvent(&theEvent);
				break;
		}
	}
	
	destroyWindow();
	termPromiscuity();
	destroyStash();
	
	return 0;	
}

GraphicsImportComponent gripJ = 0;
GraphicsImportComponent gripG = 0;

void createWindow( void )
{
	OSErr err;
	BitMap screenBits;
	Rect availableRect;
	Rect windowBounds;

	windowBounds = GetQDGlobalsScreenBits( &screenBits )->bounds;
	InsetRect(&windowBounds, 30, 50);
	
	window = NewCWindow( nil, &windowBounds, "\pEtherPEG", false, documentProc, 
			(WindowPtr)-1, true, 0);
	
	if( noErr == GetAvailableWindowPositioningBounds( GetMainDevice(), &availableRect ) ) {
		SetWindowBounds( window, kWindowStructureRgn, &availableRect );
	}
	
	ShowWindow( window );
	SetPortWindowPort( window );
	
	err = OpenADefaultComponent( GraphicsImporterComponentType, kQTFileTypeJPEG, &gripJ );
	if( err ) DebugStr( "\p couldn't open jpeg grip." );
	err = OpenADefaultComponent( GraphicsImporterComponentType, kQTFileTypeGIF, &gripG );
	if( err ) DebugStr( "\p couldn't open gif grip." );
}

void DisplayJPEGAndDisposeHandle( Handle jpeg );

void idleWindow()
{
}

void destroyWindow()
{
	if( gripJ )
		CloseComponent( gripJ );
	if( gripG )
		CloseComponent( gripG );
}

void DisplayJPEGAndDisposeHandle( Handle imageData )
{
	OSErr err;
	Rect naturalBounds;
	MatrixRecord matrix;
	SInt32 gapH, gapV;
	Fixed scaleH, scaleV;
	Rect boundsRect;
	GraphicsImportComponent grip;
	Rect windowPortRect;
	static char gifSentinel[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
	static char jpegSentinel[] = {0xFF,0xD9,0xFF,0xD9,0xFF,0xD9,0xFF,0xD9,0xFF,0xD9,0xFF,0xD9,0xFF,0xD9,0xFF,0xD9};
	Ptr sentinel;
	Size sentinelSize;
	
	if( !imageData ) return;
	
again:
	if( 'G' == **imageData ) {
		grip = gripG;
		// for GIF:
		// FF FF FF FF will ensure that the bit parser aborts, since you can't
		// have two consecutive all-ones symbols in the LZW codestream --
		// you can sometimes have one (say, a 9-bit code), but after consuming
		// it the code width increases (so we're now using 10-bit codes) 
		// and the all-ones code won't be valid for a while yet.
		sentinel = gifSentinel;
		sentinelSize = sizeof(gifSentinel);
	}
	else {
		grip = gripJ;
		// for JPEG:
		// FF D9 FF D9 will ensure (a) that the bit-parser aborts, since FF D9
		// is an "unstuffed" FF and hence illegal in the entropy-coded datastream,
		// and (b) be long enough to stop overreads.
		sentinel = jpegSentinel;
		sentinelSize = sizeof(jpegSentinel);
	}
	
	//ее add sentinel pattern to the end of the handle.
	err = PtrAndHand( sentinel, imageData, sentinelSize );
	
	err = GraphicsImportSetDataHandle( grip, imageData );
	if( err ) goto bail;
	err = GraphicsImportGetNaturalBounds( grip, &naturalBounds );
	if( err ) goto bail;
	
	GetPortBounds( GetWindowPort( window ), &windowPortRect );
	gapH = windowPortRect.right - naturalBounds.right;
	gapV = windowPortRect.bottom - naturalBounds.bottom;
	
	if( gapH >= 0 ) {
		gapH = ((UInt16)Random()) % gapH;
		scaleH = fixed1;
	}
	else {
		gapH = 0;
		scaleH = FixDiv( windowPortRect.right, naturalBounds.right );
	}
	
	if( gapV >= 0 ) {
		gapV = ((UInt16)Random()) % gapV;
		scaleV = fixed1;
	}
	else {
		gapV = 0;
		scaleV = FixDiv( windowPortRect.bottom, naturalBounds.bottom );
	}
	
	// need to use smaller scale of the two, and then recalc the other gap.
	if( scaleH > scaleV ) {
		scaleH = scaleV;
		gapH = windowPortRect.right - FixMul(scaleH, naturalBounds.right);
		gapH = ((UInt16)Random()) % gapH;
	} else if( scaleH < scaleV ) {
		scaleV = scaleH;
		gapV = windowPortRect.bottom - FixMul(scaleV, naturalBounds.bottom);
		gapV = ((UInt16)Random()) % gapV;
	}
	
	SetIdentityMatrix( &matrix );
	ScaleMatrix( &matrix, scaleH, scaleV, 0, 0 );
	TranslateMatrix( &matrix, gapH<<16, gapV<<16 );

	err = GraphicsImportSetMatrix( grip, &matrix );
	if( err ) goto bail;

	err = GraphicsImportDraw( grip );
	if( err ) goto bail;
	
	err = GraphicsImportGetBoundsRect( grip, &boundsRect );
	if( err ) goto bail;
	InsetRect( &boundsRect, -1, -1 );
	SetPortWindowPort( window );
	FrameRect( &boundsRect );
	
	if( scanForAnotherImageMarker( imageData ) ) {
		// DebugStr("\p again!");
		goto again;
	}
	
bail:
	DisposeHandle( imageData );
	gDrewJPEG = true;
}


enum { 
	kBlobSize = 6
};
Boolean resetBlobRect = false;
void showBlob( short n )
{
	static Boolean first = true;
	static Rect r, rx;
	static RGBColor black = { 0,0,0 };
	static RGBColor white = { 0xffff,0xffff,0xffff };
	static RGBColor blue = { 0,0,0xffff };
	static RGBColor green = { 0,0x8000,0 };
	static RGBColor yellow = { 0xffff,0xffff,0 };
	Rect windowPortRect;
	GetPortBounds( GetWindowPort( window ), &windowPortRect );
	
	SetPortWindowPort( window );
	RGBForeColor( 0 == n ? &yellow :
				  1 == n ? &black :
				  2 == n ? &green :
				  		   &blue );
	
	if( resetBlobRect || first || r.right > windowPortRect.right ) {
		first = false;
		resetBlobRect = false;
		r.left = 1;
		r.bottom = windowPortRect.bottom - 1;
		r.top = r.bottom - kBlobSize;
		r.right = r.left + kBlobSize;
	}
	PaintOval( &r );
	RGBForeColor( &black );
	
	r.left += kBlobSize + 1;
	r.right += kBlobSize + 1;
	
	rx = r;
	rx.right += 10 * (kBlobSize + 1);
	EraseRect( &rx );
}
void eraseBlobArea( void )
{
	Rect r;
	Rect windowPortRect;
	SetPortWindowPort( window );
	GetPortBounds( GetWindowPort( window ), &windowPortRect );
	r = windowPortRect;
	r.top = r.bottom - (kBlobSize+2);
	EraseRect( &r );
	resetBlobRect = true;
}
