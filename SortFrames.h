
//#include <MacTypes.h>
#include <Carbon/Carbon.h>

typedef struct {
	// no ethernet header; please remove it first.
	// IP Header:
	UInt8	versionAndIHL;
	UInt8		typeOfService;
	UInt16			totalLength;
	UInt32	ip1;
	UInt8	timeToLive;
	UInt8		protocol;
	UInt16			headerChecksum;
	UInt32	sourceIP;
	UInt32	destIP;
	// TCP header:
	UInt16	sourcePort;
	UInt16	destPort;
	UInt32	sequenceNumber;
	UInt32	ackNumber;
	UInt8	dataOffsetAndJunk;	// dataOffset is high 4 bits; dataOffset is number of UInt32s in TCP header
	#define kFINBit 0x01
	UInt8	moreFlagsAndJunk;
	// etc.
	// whatever
} Packet;

OSErr createStash(void);
void destroyStash(void);

void ConsumePacket( const Packet *packet );

void searchForJPEGMarkers( const Packet *packet, Boolean *foundSOI, SInt32 *offsetOfSOI, 
							Boolean *foundEOI, SInt32 *offsetAfterEOI );

void harvestJPEGFromSinglePacket( const Packet *packet, SInt32 offsetOfSOI, SInt32 offsetAfterEOI );
void harvestJPEGFromStashAndThisPacket( const Packet *packet, SInt32 offsetAfterEOI );
void addPacketToStashIfItContinuesASequence( const Packet *packet );

Boolean scanForAnotherImageMarker( Handle imageData );

void DisplayJPEGAndDisposeHandle( Handle jpeg );
void showBlob( short n );
void eraseBlobArea( void );
