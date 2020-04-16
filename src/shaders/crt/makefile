# define the correct path and name
export PATH 		:= c:/tcc/:$(PATH)
PROJECT_NAME        ?= GAMENAME
OUTDIR				= out/
SOURCE				= src/
DEBUGGING           ?= FALSE

CC = tcc
MAKE = make

ifeq ($(DEBUGGING), TRUE)
	CFLAGS += -g
endif

CFLAGS += -O2 -Wall -DPLATFORM_DESKTOP -DSTBI_NO_SIMD
CFLAGS += -Wl,--subsystem=gui

INCLUDE_PATHS = -I.
LDFLAGS = -L.
LDLIBS = -lmsvcrt -lraylib -lopengl32 -lgdi32 -lkernel32 -lshell32 -luser32

PROJECT_SOURCE_FILES ?= $(wildcard $(SOURCE)*.c $(SOURCE)**/*.c)

OBJS = $(patsubst %.c, $(PROJECT_SOURCE_FILES))

.PHONY: all clean

all:
	$(MAKE) $(PROJECT_NAME)
	$(OUTDIR)$(PROJECT_NAME)

$(PROJECT_NAME):
	$(CC) -o $(OUTDIR)$(PROJECT_NAME).exe $(PROJECT_SOURCE_FILES) $(CFLAGS) $(INCLUDE_PATHS) $(LDFLAGS) $(LDLIBS)

clean:
	del $(wildcard $(SOURCE)*.o $(SOURCE)**/*.o)
