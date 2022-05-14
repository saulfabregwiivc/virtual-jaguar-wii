//
// VIDEO.CPP: SDL/local hardware specific video routines
//
// by James L. Hammons
//

#include "tom.h"
#include "sdlemu_opengl.h"
#include "settings.h"
#include "video.h"

// External global variables

//shouldn't these exist here??? Prolly.
//And now, they do! :-)
SDL_Surface * surface, * mainSurface;
Uint32 mainSurfaceFlags;
int16 * backbuffer;
SDL_Joystick * joystick;

// One of the reasons why OpenGL is slower then normal SDL rendering, is because
// the data is being pumped into the buffer every frame with a overflow as result.
// So, we going tot render every 1 frame instead of every 0 frame.
int frame_ticker  = 0;

//
// Create SDL/OpenGL surfaces
//
bool InitVideo(void)
{
	// Get proper info about the platform we're running on...
	const SDL_VideoInfo * info = SDL_GetVideoInfo();

	if (!info)
	{
		WriteLog("VJ: SDL is unable to get the video info: %s\n", SDL_GetError());
		return false;
	}

	if (vjs.useOpenGL)
	{
	    // Initializing SDL attributes with OpenGL
	    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		mainSurfaceFlags = SDL_OPENGL;
		
	}
	else
	{
		if (info->hw_available)
			mainSurfaceFlags = SDL_HWSURFACE | SDL_HWPALETTE | SDL_DOUBLEBUF;

		if (info->blit_hw)
			mainSurfaceFlags |= SDL_HWACCEL;
	}

	if (vjs.fullscreen)
		mainSurfaceFlags |= SDL_FULLSCREEN;

	if (!vjs.useOpenGL)
//		mainSurface = SDL_SetVideoMode(VIRTUAL_SCREEN_WIDTH, VIRTUAL_SCREEN_HEIGHT_NTSC, 16, mainSurfaceFlags);
		//mainSurface = SDL_SetVideoMode(VIRTUAL_SCREEN_WIDTH,
		//	(vjs.hardwareTypeNTSC ? VIRTUAL_SCREEN_HEIGHT_NTSC : VIRTUAL_SCREEN_HEIGHT_PAL),
		//	16, mainSurfaceFlags);
		//mainSurface = SDL_SetVideoMode(320, 225, 16,SDL_FULLSCREEN);
		mainSurface = SDL_SetVideoMode(320, 225, 16,SDL_HWSURFACE|SDL_FULLSCREEN|SDL_DOUBLEBUF);
	else
	    // When OpenGL is used, we're going to use a standard resolution of 640x480.
	    // This way we have good scaling functionality and when the screen is resized
	    // because of the NTSC <-> PAL resize, we only have to re-create the texture
	    // instead of initializing the entire OpenGL texture en screens.
		//mainSurface = SDL_SetVideoMode(640, 480, 16, mainSurfaceFlags);
		mainSurface = SDL_SetVideoMode(640,480,16,SDL_HWSURFACE|SDL_FULLSCREEN|SDL_DOUBLEBUF);

	if (mainSurface == NULL)
	{
		WriteLog("VJ: SDL is unable to set the video mode: %s\n", SDL_GetError());
		return false;
	}

	SDL_WM_SetCaption("Virtual Jaguar", "Virtual Jaguar");

	// Create the primary SDL display (16 BPP, 5/5/5 RGB format)
	/*surface = SDL_CreateRGBSurface(SDL_SWSURFACE, VIRTUAL_SCREEN_WIDTH,
		(vjs.hardwareTypeNTSC ? VIRTUAL_SCREEN_HEIGHT_NTSC : VIRTUAL_SCREEN_HEIGHT_PAL),
		16, 0x7C00, 0x03E0, 0x001F, 0);*/
	surface = SDL_CreateRGBSurface(SDL_SWSURFACE, tom_getVideoModeWidth(),
		tom_getVideoModeHeight(), 16, 0x7C00, 0x03E0, 0x001F, 0);


	if (surface == NULL)
	{
		WriteLog("VJ: Could not create primary SDL surface: %s\n", SDL_GetError());
		return false;
	}

	if (vjs.useOpenGL)
	    // Let us setup OpenGL and our rendering texture. We give the src (surface) and the
	    // dst (mainSurface) display as well as the automatic bpp selection as options so that
	    // our texture is automaticly created :)
		sdlemu_init_opengl(surface, mainSurface, 1 /*method*/, 
                           vjs.glFilter /*texture type (linear, nearest)*/, 
                           NULL /* Automatic bpp selection based upon src */);
		

	// Initialize Joystick support under SDL
	if (vjs.useJoystick)
	{
		if (SDL_NumJoysticks() <= 0)
		{
			vjs.useJoystick = false;
			printf("VJ: No joystick(s) or joypad(s) detected on your system. Using keyboard...\n");
		}
		else
		{
			if ((joystick = SDL_JoystickOpen(vjs.joyport)) == 0)
			{
				vjs.useJoystick = false;
				printf("VJ: Unable to open a Joystick on port: %d\n", (int)vjs.joyport);
			}
			else
				printf("VJ: Using: %s\n", SDL_JoystickName(vjs.joyport));
		}
	}

	// Set up the backbuffer
//To be safe, this should be 1280 * 625 * 2...
	backbuffer = (int16 *)malloc(845 * 525 * sizeof(int16));
	//backbuffer = (int16 *)malloc(1280 * 625 * sizeof(int16));
//	memset(backbuffer, 0x44, VIRTUAL_SCREEN_WIDTH * VIRTUAL_SCREEN_HEIGHT_NTSC * sizeof(int16));
	/*memset(backbuffer, 0x44, VIRTUAL_SCREEN_WIDTH *
		(vjs.hardwareTypeNTSC ? VIRTUAL_SCREEN_HEIGHT_NTSC : VIRTUAL_SCREEN_HEIGHT_PAL)
		* sizeof(int16));*/
	memset(backbuffer, 0x22, tom_getVideoModeWidth() * tom_getVideoModeHeight() * sizeof(int16));

	return true;
}

//
// Free various SDL components
//
void VideoDone(void)
{
	if (vjs.useOpenGL)
		sdlemu_close_opengl();

	SDL_JoystickClose(joystick);
	SDL_FreeSurface(surface);
	free(backbuffer);
}

//
// Render the backbuffer to the primary screen surface
//
void RenderBackbuffer(void)
{
	if (SDL_MUSTLOCK(surface))
		while (SDL_LockSurface(surface) < 0)
			SDL_Delay(10);

	memcpy(surface->pixels, backbuffer, tom_getVideoModeWidth() * tom_getVideoModeHeight() * 2);

	if (SDL_MUSTLOCK(surface))
		SDL_UnlockSurface(surface);

	if (vjs.useOpenGL) {
	    // One of the reasons why OpenGL is slower then normal SDL rendering, is because
	    // the data is being pumped into the buffer every frame with a overflow as result.
	    // So, we going tot render every 1 fps instead of every 0 fps.
	    if ( frame_ticker != 0 ) {
			    sdlemu_draw_texture(mainSurface, surface, 1/*1=GL_QUADS*/);
			    frame_ticker = 0;  // Reset frame_ticker to 0 otherwise we won't get
                                   // backrendering "frameskip".
		} else
                frame_ticker = frame_ticker + 1;    
    }  
	else
	{
		SDL_Rect rect = { 0, 0, surface->w, surface->h };
		SDL_BlitSurface(surface, &rect, mainSurface, &rect);
		SDL_Flip(mainSurface);
    }
}

//
// Resize the main SDL screen & backbuffer
//
void ResizeScreen(uint32 width, uint32 height)
{
	char window_title[256];

	SDL_FreeSurface(surface);
	surface = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, 16, 0x7C00, 0x03E0, 0x001F, 0);

	if (surface == NULL)
	{
		WriteLog("Video: Could not create primary SDL surface: %s", SDL_GetError());
//This is just crappy. We shouldn't exit this way--it leaves all kinds of memory leaks
//as well as screwing up SDL... !!! FIX !!!
		exit(1);
	}

	if (vjs.useOpenGL)
	{
	    // Recreate the texture because of the NTSC <-> PAL screen resize.
		sdlemu_create_texture( surface, mainSurface, vjs.glFilter , NULL);
	}
	else
	{
		mainSurface = SDL_SetVideoMode(width, height, 16, SDL_HWSURFACE|SDL_FULLSCREEN|SDL_DOUBLEBUF);

		if (mainSurface == NULL)
		{
			WriteLog("Video: SDL is unable to set the video mode: %s\n", SDL_GetError());
			exit(1);
		}
	}

	sprintf(window_title, "Virtual Jaguar (%i x %i)", (int)width, (int)height);
	SDL_WM_SetCaption(window_title, window_title);
}

//
// Return the screen's pitch
//
uint32 GetSDLScreenPitch(void)
{
	return surface->pitch;
}

//
// Fullscreen <-> window switching
//
void ToggleFullscreen(void)
{
//NOTE: This does *NOT* work with OpenGL rendering! !!! FIX !!!
	if (vjs.useOpenGL)
		return;										// Until we can fix it...

	vjs.fullscreen = !vjs.fullscreen;
	mainSurfaceFlags &= ~SDL_FULLSCREEN;

	if (vjs.fullscreen)
		mainSurfaceFlags |= SDL_FULLSCREEN;

	mainSurface = SDL_SetVideoMode(tom_width, tom_height, 16, SDL_HWSURFACE|SDL_FULLSCREEN|SDL_DOUBLEBUF);

	if (mainSurface == NULL)
	{
		WriteLog("Video: SDL is unable to set the video mode: %s\n", SDL_GetError());
		exit(1);
	}

	SDL_WM_SetCaption("Virtual Jaguar", "Virtual Jaguar");
}
