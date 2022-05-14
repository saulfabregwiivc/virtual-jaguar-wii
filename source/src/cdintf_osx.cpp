//
// OS specific CDROM interface (Mac OS X)
//
// by James L. Hammons & ?
//

#include "log.h"

//
// OS X support functions
// OS specific implementation of OS agnostic functions
//

bool CDIntfInit(void)
{
	WriteLog("CDINTF: Init unimplemented!\n");
	return false;
}

void CDIntfDone(void)
{
}

bool CDIntfReadBlock(unsigned sector, unsigned char * buffer)
{
	WriteLog("CDINTF: ReadBlock unimplemented!\n");
	return false;
}

unsigned CDIntfGetNumSessions(void)
{
	// Still need relevant code here... !!! FIX !!!
	return 2;
}

void CDIntfSelectDrive(unsigned driveNum)
{
	WriteLog("CDINTF: SelectDrive unimplemented!\n");
}

unsigned CDIntfGetCurrentDrive(void)
{
	WriteLog("CDINTF: GetCurrentDrive unimplemented!\n");
	return 0;
}

const unsigned char * CDIntfGetDriveName(unsigned)
{
	WriteLog("CDINTF: GetDriveName unimplemented!\n");
	return NULL;
}

unsigned char CDIntfGetSessionInfo(unsigned session, unsigned offset)
{
	WriteLog("CDINTF: GetSessionInfo unimplemented!\n");
	return 0xFF;
}

unsigned char CDIntfGetTrackInfo(unsigned track, unsigned offset)
{
	WriteLog("CDINTF: GetTrackInfo unimplemented!\n");
	return 0xFF;
}
