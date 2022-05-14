//
// Virtual Jaguar Emulator
//
// Original codebase by David Raingeard (Cal2)
// GCC/SDL port by Niels Wagenaar (Linux/WIN32) and Caz (BeOS)
// Cleanups/fixes/enhancements by James L. Hammons and Adam Green
//

#ifdef __GCCUNIX__
#include <unistd.h>									// Is this necessary anymore?
#endif

#include <time.h>
#include <SDL/SDL.h>
#include "jaguar.h"
#include "video.h"
#include "gui.h"
#include "sdlemu_opengl.h"
#include "settings.h"								// Pull in "vjs" struct
#include <gccore.h>
#include <ogcsys.h>
#include "fat.h"
#include <sys/stat.h>
// Uncomment this for speed control (?)
//#define SPEED_CONTROL

// Private function prototypes

// External variables

extern uint8 * jaguar_mainRam;
extern uint8 * jaguar_mainRom;
extern uint8 * jaguar_bootRom;
extern uint8 * jaguar_CDBootROM;

// Global variables (export capable)
//should these even be here anymore?

bool finished = false;
bool showGUI = false;
bool showMessage = false;
uint32 showMessageTimeout;
char messageBuffer[200];
bool BIOSLoaded = false;
bool CDBIOSLoaded = false;


//
// The main emulator loop (what else?)
//
//Maybe we should move the video stuff to TOM? Makes more sense to put it there...
//Actually, it would probably be better served in VIDEO.CPP... !!! FIX !!! [DONE]
uint32 totalFrames;//temp, so we can grab this from elsewhere...

#undef main
//int main(int argc, char * argv[])
extern "C" int SDL_main(int argc, char * argv[]) 
{
	fatInitDefault();

//	uint32 startTime;//, totalFrames;//, endTime;//, w, h;
//	uint32 nNormalLast = 0;
//	int32 nNormalFrac = 0; 
	//int32 nFrameskip = 0;							// Default: Show every frame
//	int32 nFrame = 0;								// No. of Frame

	printf("Virtual Jaguar GCC/SDL Portable Jaguar Emulator v1.0.7\n");
	printf("Based upon Virtual Jaguar core v1.0.0 by David Raingeard.\n");
	printf("Written by Niels Wagenaar (Linux/WIN32), Carwin Jones (BeOS),\n");
	printf("James L. Hammons (WIN32) and Adam Green (MacOS)\n");
	printf("Contact: http://sdlemu.ngemu.com/ | sdlemu@ngemu.com\n");
	SDL_Delay(2000);

	bool haveCart = false;							// Assume there is no cartridge...!

	log_init("/apps/jaguar/vj.log");

	LoadVJSettings();								// Get config file settings...


	// Check the switches... ;-)
	// NOTE: Command line switches can override any config file settings, thus the
	//       proliferation of the noXXX switches. ;-)


/* TEST
vjs.useJoystick=true;
vjs.useOpenGL = false;
vjs.fullscreen = true;
vjs.useJaguarBIOS=true;
vjs.DSPEnabled=false;
vjs.hardwareTypeNTSC = true;*/
//haveCart=true;
//nFrameskip = 2;

	// Set up SDL library
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0)
//		| SDL_INIT_CDROM) < 0)
//		| SDL_INIT_CDROM | SDL_INIT_NOPARACHUTE) < 0)
	{
		WriteLog("VJ: Could not initialize the SDL library: %s\n", SDL_GetError());
		return -1;
	}
	WriteLog("VJ: SDL successfully initialized.\n");

WriteLog("Initializing memory subsystem...\n");
	InitMemory();
WriteLog("Initializing version...\n");
	InitVersion();
	version_display(log_get());
WriteLog("Initializing jaguar subsystem...\n");
	jaguar_init();

	// Get the BIOS ROM
//	if (vjs.useJaguarBIOS)
// What would be nice here would be a way to check if the BIOS was loaded so that we
// could disable the pushbutton on the Misc Options menu... !!! FIX !!! [DONE here, but needs to be fixed in GUI as well!]
WriteLog("About to attempt to load BIOSes...\n");
	BIOSLoaded = (JaguarLoadROM(jaguar_bootRom, vjs.jagBootPath) == 0x20000 ? true : false);
	WriteLog("VJ: BIOS is %savailable...\n", (BIOSLoaded ? "" : "not "));
	CDBIOSLoaded = (JaguarLoadROM(jaguar_CDBootROM, vjs.CDBootPath) == 0x40000 ? true : false);
	WriteLog("VJ: CD BIOS is %savailable...\n", (CDBIOSLoaded ? "" : "not "));

	SET32(jaguar_mainRam, 0, 0x00200000);			// Set top of stack...

WriteLog("Initializing video subsystem...\n");
	InitVideo();
WriteLog("Initializing GUI subsystem...\n");
	InitGUI();

	// Get the cartridge ROM (if passed in)
	// Now with crunchy GUI goodness!
//	JaguarLoadCart(jaguar_mainRom, (haveCart ? argv[1] : vjs.ROMPath));
//Need to find a better way to handle this crap...
WriteLog("About to start GUI...\n");
//	GUIMain();
	GUIMain(haveCart ? argv[1] : NULL);
	// TEST
	//GUIMain("/apps/jaguar/ROMs/attack.j64");

//This is no longer accurate...!
//	int elapsedTime = clock() - startTime;
//	int fps = (1000 * totalFrames) / elapsedTime;
//	WriteLog("VJ: Ran at an average of %i FPS.\n", fps);

	jaguar_done();
	VersionDone();
	MemoryDone();
	VideoDone();
	log_done();	

	// Free SDL components last...!
//	SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_CDROM);
	SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_AUDIO | SDL_INIT_TIMER);
	SDL_Quit();

    return 0;
}
