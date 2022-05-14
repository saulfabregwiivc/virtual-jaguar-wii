#
# Unified Makefile for Virtual Jaguar GCC/SDL Portable Jaguar Emulator
#
# Note that SYSTYPE, EXESUFFIX, GLLIB, ICON, and SDLLIBTYPE are passed in by the
# script called "compile". The unified makefile should make life a little
# easier for the maintainers. :-)
#

CC  = powerpc-eabi-gcc
CXX = powerpc-eabi-g++
#LD         = powerpc-eabi-gcc
TARGET     = vj$(EXESUFFIX)
include $(DEVKITPPC)/wii_rules
# Note that we use optimization level 2 instead of 3--3 doesn't seem to gain much over 2
CFLAGS = -Wall -Wno-switch -Wno-non-virtual-dtor -O2 -D__GCCUNIX__ \
		-ffast-math -fomit-frame-pointer -I$(DEVKITPRO)/libogc/include -g
#		-fomit-frame-pointer `sdl-config --cflags` -g
#		-fomit-frame-pointer `sdl-config --cflags` -DLOG_UNMAPPED_MEMORY_ACCESSES
LDFLAGS ?=   -g  -mrvl -Wl

LIBS = -L$(DEVKITPRO)/libogc/lib/wii -lSDL_mixer -lSDL_ttf -lSDL_gfx -lSDL_image -lsmpeg -lSDL -ljpeg -lpng -lfreetype  -lstdc++ -ldi -lfat -lwiiuse  -lbte -lz -logc -lm -lgl2gx -lwiikeyboard 

INCS = -I. -Isrc -Isrc/include -I$(DEVKITPRO)/libogc/include 

THECC = $(CC) $(CFLAGS) $(INCS)

OBJS = $(.cpp=.o)  $(.c=.o) 
DEPS = $(.cpp=.d)

OBJS = \
	obj/anajoy.o \
	obj/blitter.o \
	obj/cdrom.o \
	obj/cdintf.o \
	obj/clock.o \
	obj/crc32.o \
	obj/dac.o \
	obj/dsp.o \
	obj/eeprom.o \
	obj/gpu.o \
	obj/gui.o \
	obj/jagdasm.o \
	obj/jaguar.o \
	obj/jerry.o \
	obj/joystick.o \
	obj/log.o \
	obj/m68kcpu.o obj/m68kops.o obj/m68kopac.o obj/m68kopdm.o obj/m68kopnz.o \
	obj/m68kdasm.o \
	obj/memory.o \
	obj/objectp.o \
	obj/sdlemu_config.o \
	obj/sdlemu_opengl.o \
	obj/settings.o \
	obj/tom.o \
	obj/unzip.o \
	obj/version.o \
	obj/video.o \
	obj/vj.o 
	#$(ICON)
#	obj/cdbios.o 
#	obj/cdi.o 

all: obj $(TARGET)

clean:
	rm -rf obj
	rm -f ./vj$(EXESUFFIX)

obj:
	mkdir obj

obj/%.o: src/%.c src/%.h src/include/%.h
	$(THECC) -c $< -o $@

obj/%.o: src/%.cpp src/%.h src/include/%.h
	$(THECC) -c $< -o $@

obj/%.o: src/%.c
	$(THECC) -c $< -o $@

obj/%.o: src/%.cpp
	$(THECC) -c $< -o $@

$(TARGET): $(OBJS)
	 $(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)
#	strip --strip-all vj$(EXESUFFIX)
#	upx -9 vj$(EXESUFFIX)

# Other stuff that has unusual dependencies

obj/gui.o: src/gui.cpp src/include/gui.h src/include/font1.h
obj/cdintf.o: src/cdintf.cpp src/cdintf_win32.cpp src/cdintf_osx.cpp src/cdintf_linux.cpp src/include/cdintf.h

-include obj/*.d

#
# Musashi specific stuffola
#

#obj/m68k%.o: obj/m68k%.c obj/m68kops.h
#	$(CC) $(CFLAGS) -Iobj -c src/m68k%.c -o obj/m68k%.o

#obj/m68k%.o: obj/m68k%.c obj/m68kmake$(EXESUFFIX)
#	$(CC) $(CFLAGS) -Iobj -c src/m68k%.c -o obj/m68k%.o

