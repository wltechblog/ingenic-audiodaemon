# Variables
commit_tag=$(shell git rev-parse --short HEAD)

CC = $(CROSS_COMPILE)gcc
CPLUSPLUS = $(CROSS_COMPILE)g++
STRIP = $(CROSS_COMPILE)strip

SDK_INC_DIR = include
INCLUDES = -I$(SDK_INC_DIR) -I./network -I./audio -I./client -I./utils
CFLAGS = $(INCLUDES) -O2 -Wall -march=mips32r2
LDFLAG += -Wl,-gc-sections

# Configuration
CONFIG_UCLIBC_BUILD=n
CONFIG_MUSL_BUILD=y
CONFIG_STATIC_BUILD=y

ifeq ($(CONFIG_UCLIBC_BUILD), y)
CROSS_COMPILE?= mips-linux-uclibc-gnu-
CFLAGS += -muclibc
LDFLAG += -muclibc
SDK_LIB_DIR = lib
endif

ifeq ($(CONFIG_MUSL_BUILD), y)
CROSS_COMPILE?= mipsel-openipc-linux-musl-
SDK_LIB_DIR = lib
SHIM = utils/musl_shim.o
endif

ifeq ($(CONFIG_STATIC_BUILD), y)
LDFLAG += -static
LIBS = $(SDK_LIB_DIR)/libimp.a $(SDK_LIB_DIR)/libalog.a
else
LIBS = $(SDK_LIB_DIR)/libimp.so $(SDK_LIB_DIR)/libalog.so
endif

# Targets and Object Files
AUDIO_PROGS = audioplay_t31 audio_daemon audio_client

AUDIO_DAEMON_OBJS = main.o audio/output.o audio/input.o network/network.o utils/utils.o utils/logging.o $(SHIM)
AUDIO_CLIENT_OBJS = audio_client.o client/cmdline.o client/client_network.o client/playback.o client/record.o $(SHIM)

.PHONY: all version clean distclean

all: version $(AUDIO_PROGS)

version:
	@if  ! grep "$(commit_tag)" version.h >/dev/null ; then \
	echo "update version.h" ; \
	sed 's/COMMIT_TAG/"$(commit_tag)"/g' version.tpl.h > version.h ; \
	fi

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

audio_daemon: $(AUDIO_DAEMON_OBJS)
	$(CPLUSPLUS) $(LDFLAG) -o $@ $^ $(LIBS) -lpthread -lm -lrt -ldl
	$(STRIP) $@

audio_client: $(AUDIO_CLIENT_OBJS)
	$(CPLUSPLUS) $(LDFLAG) -o $@ $^ -lpthread -lm -lrt -ldl
	$(STRIP) $@

audioplay_t31: old/audioplay_t31.o $(SHIM)
	$(CPLUSPLUS) $(LDFLAG) -o $@ $^ $(LIBS) -lpthread -lm -lrt -ldl
	$(STRIP) $@

clean:
	rm -f *.o *~ audio/*.o network/*.o client/*.o utils/*.o version.h audio_daemon audio_client audioplay_t31

distclean: clean
	rm -f $(AUDIO_PROGS)
