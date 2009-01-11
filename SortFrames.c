#include "SortFrames.h"

//#include <Errors.h>
//#include <MacMemory.h>
//#include <TextUtils.h>

enum { kFree, kCaptured, kDisplaying};
typedef struct StashedPacket StashedPacket;
struct StashedPacket
	{
	SInt32 state;				// Free, captured packet, or data being displayed
	Packet *data;
	SInt32 payloadoffset;
	SInt32 SOI;
	SInt32 EOI;
	StashedPacket *parent;
	StashedPacket *next;
	StashedPacket *following;
	};

enum {
	kMaxPacketLength = 1500
};
enum {
	kStashSize = 1000
};

StashedPacket stash[kStashSize];
UInt32 nextStashEntry = 0;

OSErr createStash(void)
{
	int i;
	for (i = 0; i < kStashSize; i++)
	{
		stash[i].state = kFree;
		stash[i].data = (Packet*)NewPtr(kMaxPacketLength);
		if (!stash[i].data) { DebugStr("\p out of memory, createStash"); return(memFullErr); }
	}
	return noErr;
}

void destroyStash(void)
{
	SInt16 probe;
	for( probe = 0; probe < kStashSize; probe++ ) {
		DisposePtr((Ptr)stash[probe].data);
		stash[probe].data = nil;
	}
}

static void RecyclePacketChain(StashedPacket *parent)
{
	StashedPacket *p;
	// Recycle the chain of packets chained off this parent packet
	for (p=parent; p; p=p->next) p->state = kFree;
}

static void TrimPacketChain(StashedPacket *p)
{
	StashedPacket *q;
	
	if (!p) return;
	
	// Free up to the next packet with a start marker
	do { p->state = kFree; p=p->next; } while (p && (p->SOI == -1));
	
	if (p)	// If we have packet with a start marker
	{
		for (q = p; q; q=q->next) q->parent = p;
		p->parent = NULL;
	}
}

static void harvestJPEG(StashedPacket *parent)
{
	SInt32 totalSize;
	StashedPacket *p;
	Handle h;
	
	if (parent->SOI == -1) { DebugStr("\pERROR! parent packet has no SOI"); return; }
	
	totalSize = parent->payloadoffset - parent->SOI;
	if (parent->EOI != -1 && parent->EOI < parent->SOI) parent->EOI = -1;
	
	p = parent;
	while (p->EOI == -1)		// While we've not found the end, look for more in-order packets
	{
		StashedPacket *srch;
		SInt32 targetseqnum;
		totalSize += p->data->totalLength - p->payloadoffset;
		targetseqnum = p->data->sequenceNumber + p->data->totalLength - p->payloadoffset;
		for (srch = parent; srch; srch=srch->next)
			if (srch->data->sequenceNumber == targetseqnum)		// We found the right packet
			{
				if (srch->data->totalLength <= srch->payloadoffset) {
					// packets like this could cause us to hang, so skip 'em
					// DebugStr("\pharvestJPEG: skipping empty payload");
					continue;
				}
				p->following = srch;							// Link it in
				p = srch;										// Move p one packet forward
				break;											// and continue
			}
		// If we couldn't find the desired sequence number, leave the chain in place
		// -- it might get completed later
		if (!srch) return;
	}

	totalSize += p->EOI - p->payloadoffset;
	h = NewHandle( totalSize );
	
	if( h ) {
		Ptr ptr = *h;
		
		SInt32 size = parent->data->totalLength - parent->SOI;
		if (parent->following == NULL) size += parent->EOI - parent->data->totalLength;
		BlockMoveData( ((Ptr)(parent->data)) + parent->SOI, ptr, size );
		ptr += size;
		
		p = parent->following;
		while (p)
			{
			size = p->data->totalLength - p->payloadoffset;
			if (p->following == NULL) size += p->EOI - p->data->totalLength;
			BlockMoveData( ((Ptr)(p->data)) + p->payloadoffset, ptr, size );
			ptr += size;
			p = p->following;
			}

		DisplayJPEGAndDisposeHandle(h);
	}
	//else DebugStr("\p out of memory, harvestJPEG");

	TrimPacketChain(parent);
}

static SInt32 getOffsetToPayload( const Packet *packet )
{
	#define kIPHeaderLength	20
	short tcpHeaderLength = (packet->dataOffsetAndJunk >> 2) & ~3;
	return kIPHeaderLength + tcpHeaderLength;
}

static void ensureFreeSlotInStash()
{
	StashedPacket *p = &stash[nextStashEntry];
	
	if (p->state != kFree)
	{
		if (p->SOI != -1) harvestJPEG(p);
		while (p->state != kFree)		// If harvestJPEG was unable to pull a good image
		{								// out of the chain, then trash it anyway to make space
	 		TrimPacketChain(p->parent ? p->parent : p);
 		}
	}
}

static StashedPacket *addPacketToStash(const Packet *packetdata, SInt32 SOI, SInt32 EOI, StashedPacket *parent)
{
	StashedPacket *p;
	
	if (!parent && SOI == -1) { DebugStr("\paddPacketToStash invalid packet"); return(NULL); }
	if (packetdata->totalLength > kMaxPacketLength) return(NULL);
	
	p = &stash[nextStashEntry];
	if (p->state != kFree) { DebugStr("\paddPacketToStash no free space"); return(NULL); }
	if (++nextStashEntry >= kStashSize) nextStashEntry = 0;

	p->state  = kCaptured;
	BlockMoveData(packetdata, p->data, packetdata->totalLength);
	p->payloadoffset = getOffsetToPayload(p->data);
	p->SOI    = SOI;
	p->EOI    = EOI;
	p->parent = parent;
	p->next   = NULL;
	p->following = NULL;
	
	if (parent)
		{
		p->next = parent->next;
		parent->next = p;
		}
	
	return(p);
}

static StashedPacket *findParentPacket(const Packet *packet)
{
	int i;
	for (i = 0; i < kStashSize; i++)		// Search for matching packet
	{
		const Packet *p = stash[i].data;
		if (stash[i].state == kCaptured &&
			p->sourceIP   == packet->sourceIP   && p->destIP   == packet->destIP &&
			p->sourcePort == packet->sourcePort && p->destPort == packet->destPort)
		{
			// If this packet already has a parent, we share the same parent
			if (stash[i].parent) return (stash[i].parent);
			else return(&stash[i]);		// Else this packet is our parent
		}
	}
	return (NULL);
}

static void searchForImageMarkers(const Packet *packet, SInt32 *offsetOfSOI, SInt32 *offsetAfterEOI)
{
	
	UInt8 *packetStart, *dataStart, *dataEnd, *data;
	packetStart = (UInt8 *) packet;
	dataStart = packetStart + getOffsetToPayload(packet);	// first byte that might contain actual payload
	dataEnd = packetStart + packet->totalLength;	// byte after last byte that might contain actual payload
	
	*offsetOfSOI = -1;
	*offsetAfterEOI = -1;
	
	for( data = dataStart; data <= dataEnd-3; data++ ) {
		// JPEG SOI is FF D8, but it's always followed by another FF.
		if( ( 0xff == data[0] ) && ( 0xd8 == data[1] ) && ( 0xff == data[2] ) )
			*offsetOfSOI = data - packetStart;
		
		// GIF start marker is 'GIF89a' etc.
		if ('G' == data[0] && 'I' == data[1] && 'F' == data[2] && '8' == data[3])
			*offsetOfSOI = data - packetStart;
	}
	for( data = dataStart; data <= dataEnd-2; data++ ) {
		// JPEG EOI is always FF D9.
		if( ( 0xff == data[0] ) && ( 0xd9 == data[1] ) )
			*offsetAfterEOI = data - packetStart + 2; // caller will need to grab 2 extra bytes.
	}
	
	if (packet->moreFlagsAndJunk & kFINBit)
		*offsetAfterEOI = packet->totalLength;
}

// look for image-start markers more than 4 bytes into imageData.
// if one is found, remove the portion of the handle before it and return true.
// if none found, return false.
Boolean scanForAnotherImageMarker( Handle imageData )
{
	UInt8 *packetStart, *dataStart, *dataEnd, *data;
	Size handleSize = GetHandleSize( imageData );
	SInt32 offsetOfStart = -1L;
	
	packetStart = *imageData;
	dataStart = packetStart + 4;
	dataEnd = packetStart + handleSize;
	
	for( data = dataStart; data <= dataEnd-3; data++ ) {
		// JPEG SOI is FF D8, but it's always followed by another FF.
		if( ( 0xff == data[0] ) && ( 0xd8 == data[1] ) && ( 0xff == data[2] ) ) {
			offsetOfStart = data - packetStart;
			break;
		}
		
		// GIF start marker is 'GIF89a' etc.
		if ('G' == data[0] && 'I' == data[1] && 'F' == data[2] && '8' == data[3]) {
			offsetOfStart = data - packetStart;
			break;
		}
	}
	
	if( offsetOfStart > 0 ) {
		char mungerPleaseDelete;
		Munger( imageData, 0, nil, offsetOfStart, &mungerPleaseDelete, 0 );
		return true;
	}
	else {
		return false;
	}
}

void ConsumePacket( const Packet *packet )
{
	SInt32 SOI, EOI;
	StashedPacket *p;
	StashedPacket *parent;
	Boolean addMe = false, harvestMe = false;
	
	if( packet->protocol != 6 ) goto toss; // only TCP packets please
	if( ( packet->versionAndIHL & 0x0f ) != 5 ) goto toss; // minimal IP headers only (lame?)
	
	ensureFreeSlotInStash();
	
	p = NULL;
	parent = findParentPacket(packet);
	searchForImageMarkers(packet, &SOI, &EOI);
	
	// If this packet contains an image start marker, or continues an existing sequence, then stash it
	if (parent || SOI != -1) addMe = true;
	if (addMe) p = addPacketToStash(packet, SOI, EOI, parent);
	
	// If this packet contains an image end marker, and we successfully stashed it, then harvest the packet
	if (p && EOI != -1) harvestMe = true;
	if (harvestMe) harvestJPEG(parent ? parent : p);

	if      (harvestMe) showBlob(3); // blue
	else if (addMe)     showBlob(2); // green
	else                showBlob(1); // black
	return;
toss:
	showBlob( 0 ); // yellow
}
