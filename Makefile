
all: EtherPEG

clean: 
	rm EtherPEG.o PromiscuityX.o SortFrames.o

#CCOPTIONS=-g -fpascal-strings
CCOPTIONS=-O -fpascal-strings

EtherPEG.o: EtherPEG.c Promiscuity.h SortFrames.h Makefile
	cc -c $(CCOPTIONS) EtherPEG.c

PromiscuityX.o: PromiscuityX.c Promiscuity.h SortFrames.h Makefile
	cc -c $(CCOPTIONS) PromiscuityX.c

SortFrames.o: SortFrames.c SortFrames.h Makefile
	cc -c $(CCOPTIONS) SortFrames.c

EtherPEG: EtherPEG.o PromiscuityX.o SortFrames.o EtherPEG.r Makefile
	cc -o EtherPEG EtherPEG.o PromiscuityX.o SortFrames.o -framework Carbon -framework QuickTime -lpcap
	/Developer/Tools/Rez EtherPEG.r -o EtherPEG -a
	/Developer/Tools/SetFile -t APPL -c '????' EtherPEG
