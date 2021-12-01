#
# Makefile for netMonitor and co2Monitor.
#
# Type 'make' or 'make netMonitor' or 'make co2Monitor'to create the binary.
# Type 'make clean' or 'make cleaner' to delete all temporaries.
#
#DEV = Debug
DEV = Rel

CO2_HW = y

RPI_HW = y

# build target specs
BASE_DIR=.
SRC_DIR = $(BASE_DIR)/src
OBJ_DIR = $(BASE_DIR)/obj
RESOURCE_DIR = $(BASE_DIR)/resources
SYS_DIR = $(BASE_DIR)/systemd
SHELL = /bin/bash

ifeq ($(DEV),Rel)
	BIN_DIR = $(BASE_DIR)/bin/Rel
else
	BIN_DIR = $(BASE_DIR)/bin/Debug
endif

TARGET = co2Monitor
TARGET_BIN_DIR = /usr/local/bin
TARGET_RESOURCE_DIR = $(TARGET_BIN_DIR)/$(TARGET).d
SDL_BMP_DIR = $(TARGET_RESOURCE_DIR)/bmp
SDL_TTF_DIR = $(TARGET_RESOURCE_DIR)/ttf

CC = g++
PROTOC = protoc
PROTOCFLAGS = -I=$(SRC_DIR) --cpp_out=$(SRC_DIR)

CFLAGS = -Wall -Werror --pedantic -std=c++11 -D_REENTRANT -D_GNU_SOURCE -Wall -Wno-unused -fno-strict-aliasing -DBASE_THREADSAFE -I.
ifeq ($(DEV),Rel)
	CFLAGS +=  -O2 
else
	CFLAGS += -g -DDEBUG -D_DEBUG -O0 -fsanitize=address -fno-omit-frame-pointer
endif

ifeq ($(CO2_HW),y)
	CFLAGS += -DHAS_CO2_SENSOR
endif

ifeq ($(RPI_HW),y)
	CFLAGS += -DTARGET_RPI
endif

LIBS =

# SDL headers and libs
# sdl-config, when present, tells us where to find SDL headers and libs.
#ifneq (,$(shell which sdl-config 2>/dev/null))
#	SDL_CFLAGS =
#	SDL_LIBS =
#	SDL_CFLAGS := $(shell sdl-config --cflags)
#	SDL_LIBS := $(shell sdl-config --libs)
#	SDL_TTF_LIBS = $(strip $(shell sdl-config --libs | cut -d' ' -f1 | sed -e 's/-L//')/libSDL_ttf)
#	ifneq ("$(wildcard $(SDL_TTF_LIBS)*)","")
#		SDL_LIBS += -l SDL_ttf
#	endif
#	CFLAGS += $(SDL_CFLAGS) -DHAS_SDL
#	LIBS += $(SDL_LIBS)
#endif

ifneq (,$(shell which sdl2-config 2>/dev/null))
	SDL2_CFLAGS =
	SDL2_LIBS =
	SDL2_CFLAGS := $(shell sdl2-config --cflags)
	SDL2_LIBS := $(shell sdl2-config --libs)
	SDL2_TTF_LIBS = $(strip $(shell sdl2-config --libs | cut -d' ' -f1 | sed -e 's/-L//')/libSDL2_ttf)
	SDL2_LIBS += -lSDL2_ttf
	CFLAGS += $(SDL2_CFLAGS) -DHAS_SDL2
	LIBS += $(SDL2_LIBS)
endif

# WiringPi headers and libs
# gpio is a WiringPi utility which should be a good indicator
# of WiringPi being installed on this platform.
ifneq (,$(shell which gpio 2>/dev/null))
	CFLAGS += -DHAS_WIRINGPI
	LIBS += -l wiringPi
endif

# Protocol Buffers
ifneq (,$(shell pkg-config --libs protobuf 2>/dev/null))
	PROTOBUF_CFLAGS := -DHAS_PROTOBUF $(shell pkg-config --cflags protobuf)
	PROTOBUF_LIBS := $(shell pkg-config --libs protobuf)
	CFLAGS += $(PROTOBUF_CFLAGS)
	LIBS += $(PROTOBUF_LIBS)
endif

# zeromq
ifneq (,$(shell pkg-config --libs libzmq 2>/dev/null))
	ZMQ_CFLAGS := -DHAS_ZEROMQ $(shell pkg-config --cflags libzmq)
	ZMQ_LIBS := $(shell pkg-config --libs libzmq)
	CFLAGS += $(ZMQ_CFLAGS)
	LIBS += $(ZMQ_LIBS)
endif

# System Watchdog only exists on ArchLinux
ifneq ("$(wildcard /usr/include/systemd/sd-daemon.h)","")
	HAS_SYSD_WDOG = Yes
	CFLAGS += -DSYSTEMD_WDOG
	LIBS += -l systemd
else
	HAS_SYSD_WDOG = No
endif

CO2MON_OBJS = $(OBJ_DIR)/co2MonitorMain.o \
	$(OBJ_DIR)/restartMgr.o \
	$(OBJ_DIR)/co2PersistentStore.o \
	$(OBJ_DIR)/co2Monitor.o \
	$(OBJ_DIR)/co2Display.o \
	$(OBJ_DIR)/co2Screen.o \
	$(OBJ_DIR)/statusScreen.o \
	$(OBJ_DIR)/fanControlScreen.o \
	$(OBJ_DIR)/relHumCo2ThresholdScreen.o \
	$(OBJ_DIR)/shutdownRebootScreen.o \
	$(OBJ_DIR)/confirmCancelScreen.o \
	$(OBJ_DIR)/blankScreen.o \
	$(OBJ_DIR)/splashScreen.o \
	$(OBJ_DIR)/displayElement.o \
	$(OBJ_DIR)/co2TouchScreen.o \
	$(OBJ_DIR)/screenBacklight.o \
	$(OBJ_DIR)/netMonitor.o \
	$(OBJ_DIR)/netLink.o \
	$(OBJ_DIR)/ping.o \
	$(OBJ_DIR)/parseConfigFile.o \
	$(OBJ_DIR)/config.o \
	$(OBJ_DIR)/co2Defaults.o \
	$(OBJ_DIR)/co2Sensor.o \
	$(OBJ_DIR)/serialPort.o \
	$(OBJ_DIR)/co2Message.pb.o \
	$(OBJ_DIR)/utils.o \
	$(OBJ_DIR)/sysdWatchdog.o

# first target entry is the target invoked when typing 'make'
all: $(OBJ_DIR) $(BIN_DIR) $(TARGET)
.PHONY:	all $(TARGET) clean cleaner install uninstall xxx

$(TARGET): $(BIN_DIR)/$(TARGET)

$(BIN_DIR)/$(TARGET): $(BIN_DIR) $(OBJ_DIR) $(CO2MON_OBJS) $(SYSD_WDOG_OBJ)
	@printf "\033[1;34mLinking  \033[0m co2Monitor...\t\t\t"
	@$(CC) $(CFLAGS) -o $(BIN_DIR)/$(TARGET) $(CO2MON_OBJS) $(LIBS)
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/co2MonitorMain.o: $(SRC_DIR)/co2MonitorMain.cpp \
		$(SRC_DIR)/co2Message.pb.h \
		$(SRC_DIR)/parseConfigFile.h $(SRC_DIR)/utils.h
	@printf "\033[1;34mCompiling\033[0m co2MonitorMain.cpp...\t\t"
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/co2MonitorMain.o -c $(SRC_DIR)/co2MonitorMain.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/restartMgr.o: $(SRC_DIR)/restartMgr.cpp $(SRC_DIR)/restartMgr.h \
		$(SRC_DIR)/co2PersistentStore.h $(SRC_DIR)/utils.h
	@printf "\033[1;34mCompiling\033[0m restartMgr.cpp...\t\t"
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/restartMgr.o -c $(SRC_DIR)/restartMgr.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/co2PersistentStore.o: $(SRC_DIR)/co2PersistentStore.cpp $(SRC_DIR)/co2PersistentStore.h \
		$(SRC_DIR)/utils.h
	@printf "\033[1;34mCompiling\033[0m co2PersistentStore.cpp...\t"
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/co2PersistentStore.o -c $(SRC_DIR)/co2PersistentStore.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/co2Monitor.o: $(SRC_DIR)/co2Monitor.cpp $(SRC_DIR)/co2Monitor.h \
		$(SRC_DIR)/co2Message.pb.h \
		$(SRC_DIR)/parseConfigFile.h $(SRC_DIR)/utils.h
	@printf "\033[1;34mCompiling\033[0m co2Monitor.cpp...\t\t"
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/co2Monitor.o -c $(SRC_DIR)/co2Monitor.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/co2Display.o: $(SRC_DIR)/co2Display.cpp $(SRC_DIR)/co2Display.h \
		$(SRC_DIR)/co2Message.pb.h \
		$(SRC_DIR)/parseConfigFile.h $(SRC_DIR)/utils.h
	@printf "\033[1;34mCompiling\033[0m co2Display.cpp...\t\t"
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/co2Display.o -c $(SRC_DIR)/co2Display.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/co2Screen.o: $(SRC_DIR)/co2Screen.cpp $(SRC_DIR)/co2Screen.h \
		$(SRC_DIR)/co2Message.pb.h \
		$(SRC_DIR)/parseConfigFile.h $(SRC_DIR)/utils.h
	@printf "\033[1;34mCompiling\033[0m co2Screen.cpp...\t\t"
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/co2Screen.o -c $(SRC_DIR)/co2Screen.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/statusScreen.o: $(SRC_DIR)/statusScreen.cpp $(SRC_DIR)/co2Screen.h \
		$(SRC_DIR)/co2Message.pb.h \
		$(SRC_DIR)/parseConfigFile.h $(SRC_DIR)/utils.h
	@printf "\033[1;34mCompiling\033[0m statusScreen.cpp...\t\t"
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/statusScreen.o -c $(SRC_DIR)/statusScreen.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/fanControlScreen.o: $(SRC_DIR)/fanControlScreen.cpp $(SRC_DIR)/co2Screen.h \
		$(SRC_DIR)/co2Message.pb.h \
		$(SRC_DIR)/parseConfigFile.h $(SRC_DIR)/utils.h
	@printf "\033[1;34mCompiling\033[0m fanControlScreen.cpp...\t\t"
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/fanControlScreen.o -c $(SRC_DIR)/fanControlScreen.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/relHumCo2ThresholdScreen.o: $(SRC_DIR)/relHumCo2ThresholdScreen.cpp $(SRC_DIR)/co2Screen.h \
		$(SRC_DIR)/co2Message.pb.h \
		$(SRC_DIR)/parseConfigFile.h $(SRC_DIR)/utils.h
	@printf "\033[1;34mCompiling\033[0m relHumCo2ThresholdScreen.cpp...\t\t"
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/relHumCo2ThresholdScreen.o -c $(SRC_DIR)/relHumCo2ThresholdScreen.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/shutdownRebootScreen.o: $(SRC_DIR)/shutdownRebootScreen.cpp $(SRC_DIR)/co2Screen.h \
		$(SRC_DIR)/co2Message.pb.h \
		$(SRC_DIR)/parseConfigFile.h $(SRC_DIR)/utils.h
	@printf "\033[1;34mCompiling\033[0m shutdownRebootScreen.cpp...\t\t"
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/shutdownRebootScreen.o -c $(SRC_DIR)/shutdownRebootScreen.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/confirmCancelScreen.o: $(SRC_DIR)/confirmCancelScreen.cpp $(SRC_DIR)/co2Screen.h \
		$(SRC_DIR)/co2Message.pb.h \
		$(SRC_DIR)/parseConfigFile.h $(SRC_DIR)/utils.h
	@printf "\033[1;34mCompiling\033[0m confirmCancelScreen.cpp...\t\t"
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/confirmCancelScreen.o -c $(SRC_DIR)/confirmCancelScreen.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/blankScreen.o: $(SRC_DIR)/blankScreen.cpp $(SRC_DIR)/co2Screen.h \
		$(SRC_DIR)/co2Message.pb.h \
		$(SRC_DIR)/parseConfigFile.h $(SRC_DIR)/utils.h
	@printf "\033[1;34mCompiling\033[0m blankScreen.cpp...\t\t"
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/blankScreen.o -c $(SRC_DIR)/blankScreen.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/splashScreen.o: $(SRC_DIR)/splashScreen.cpp $(SRC_DIR)/co2Screen.h \
		$(SRC_DIR)/co2Message.pb.h \
		$(SRC_DIR)/parseConfigFile.h $(SRC_DIR)/utils.h
	@printf "\033[1;34mCompiling\033[0m splashScreen.cpp...\t\t"
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/splashScreen.o -c $(SRC_DIR)/splashScreen.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/displayElement.o: $(SRC_DIR)/displayElement.cpp $(SRC_DIR)/displayElement.h \
		$(SRC_DIR)/parseConfigFile.h $(SRC_DIR)/utils.h
	@printf "\033[1;34mCompiling\033[0m displayElement.cpp...\t\t"
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/displayElement.o -c $(SRC_DIR)/displayElement.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/co2TouchScreen.o: $(SRC_DIR)/co2TouchScreen.cpp $(SRC_DIR)/co2TouchScreen.h \
		$(SRC_DIR)/co2Message.pb.h \
		$(SRC_DIR)/parseConfigFile.h $(SRC_DIR)/utils.h
	@printf "\033[1;34mCompiling\033[0m co2TouchScreen.cpp...\t\t"
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/co2TouchScreen.o -c $(SRC_DIR)/co2TouchScreen.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/screenBacklight.o: $(SRC_DIR)/screenBacklight.cpp $(SRC_DIR)/screenBacklight.h \
		$(SRC_DIR)/co2Message.pb.h \
		$(SRC_DIR)/parseConfigFile.h $(SRC_DIR)/utils.h
	@printf "\033[1;34mCompiling\033[0m screenBacklight.cpp...\t"
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/screenBacklight.o -c $(SRC_DIR)/screenBacklight.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/netMonitor.o: $(SRC_DIR)/netMonitor.cpp $(SRC_DIR)/netMonitor.h \
		$(SRC_DIR)/co2Message.pb.h \
		$(SRC_DIR)/parseConfigFile.h $(SRC_DIR)/utils.h
	@printf "\033[1;34mCompiling\033[0m netMonitor.cpp...\t\t"
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/netMonitor.o -c $(SRC_DIR)/netMonitor.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/netLink.o: $(SRC_DIR)/netLink.cpp $(SRC_DIR)/netLink.h
	@printf "\033[1;34mCompiling\033[0m netLink.cpp...\t\t"
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/netLink.o -c $(SRC_DIR)/netLink.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/ping.o: $(SRC_DIR)/ping.cpp $(SRC_DIR)/ping.h
	@printf "\033[1;34mCompiling\033[0m ping.cpp...\t\t\t"
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/ping.o -c $(SRC_DIR)/ping.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/parseConfigFile.o: $(SRC_DIR)/parseConfigFile.cpp $(SRC_DIR)/parseConfigFile.h
	@printf "\033[1;34mCompiling\033[0m parseConfigFile.cpp...\t"
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/parseConfigFile.o -c $(SRC_DIR)/parseConfigFile.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/config.o: $(SRC_DIR)/config.cpp $(SRC_DIR)/config.h
	@printf "\033[1;34mCompiling\033[0m config.cpp...\t\t\t"
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/config.o -c $(SRC_DIR)/config.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/co2Defaults.o: $(SRC_DIR)/co2Defaults.cpp $(SRC_DIR)/co2Defaults.h
	@printf "\033[1;34mCompiling\033[0m co2Defaults.cpp...\t\t"
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/co2Defaults.o -c $(SRC_DIR)/co2Defaults.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/co2Sensor.o: $(SRC_DIR)/co2Sensor.cpp $(SRC_DIR)/co2Sensor.h
	@printf "\033[1;34mCompiling\033[0m co2Sensor.cpp...\t\t"
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/co2Sensor.o -c $(SRC_DIR)/co2Sensor.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/serialPort.o: $(SRC_DIR)/serialPort.cpp $(SRC_DIR)/serialPort.h
	@printf "\033[1;34mCompiling\033[0m serialPort.cpp...\t\t"
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/serialPort.o -c $(SRC_DIR)/serialPort.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/utils.o: $(SRC_DIR)/utils.cpp $(SRC_DIR)/utils.h
	@printf "\033[1;34mCompiling\033[0m utils.cpp...\t\t\t"
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/utils.o -c $(SRC_DIR)/utils.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/sysdWatchdog.o: $(SRC_DIR)/sysdWatchdog.cpp $(SRC_DIR)/sysdWatchdog.h
	@printf "\033[1;34mCompiling\033[0m sysdWatchdog.cpp...\t\t"
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/sysdWatchdog.o -c $(SRC_DIR)/sysdWatchdog.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/co2Message.pb.o: $(SRC_DIR)/co2Message.pb.cc
	@printf "\033[1;34mCompiling\033[0m co2Message.pb.cpp...\t\t"
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/co2Message.pb.o -c $(SRC_DIR)/co2Message.pb.cc
	@printf "\033[1;32mDone\033[0m\n"

$(SRC_DIR)/co2Message.pb.h: $(SRC_DIR)/co2Message.pb.cc

$(SRC_DIR)/co2Message.pb.cc: $(SRC_DIR)/co2Message.proto
	@printf "\033[1;34mCompiling\033[0m co2Message.proto...\t\t"
	@protoc $(PROTOCFLAGS) $(SRC_DIR)/co2Message.proto
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR) 2>/dev/null

$(BIN_DIR):
	@mkdir -p $(BIN_DIR) 2>/dev/null

clean:
	@echo -n 'Removing all temporary binaries... '
	@-rm -f $(BIN_DIR)/* $(OBJ_DIR)/*.o 2>/dev/null
	@echo Done.

cleaner:
	@echo -n 'Removing all temporary binaries and their directories... '
	@-rm -rf $(OBJ_DIR) $(BIN_DIR) $(SRC_DIR)/*.pb.cc $(SRC_DIR)/*.pb.h 2>/dev/null
	@echo Done.

install:
ifeq ($(shell id -u), 0)
	@install -d $(TARGET_BIN_DIR)
	@install -d $(TARGET_RESOURCE_DIR)
	@install -d $(SDL_BMP_DIR)
	@install -d $(SDL_TTF_DIR)
	@install -m 444 -D $(RESOURCE_DIR)/*.bmp $(SDL_BMP_DIR)
	@install -m 444 -D $(RESOURCE_DIR)/*.ttf $(SDL_TTF_DIR)
	@install -m 755 -D $(BIN_DIR)/$(TARGET) $(TARGET_BIN_DIR)
	@shasum -a 512256 $(TARGET_BIN_DIR)/$(TARGET) >$(TARGET_RESOURCE_DIR)/$(TARGET).cksum
	@$(SHELL) -c $(SYS_DIR)/mksystemd.sh 
else
	@echo "Must be root to run make install"
endif

uninstall:
ifeq ($(shell id -u), 0)
	@$(SHELL) -c "$SYS_DIR)/mksystemd.sh -u"
	@-rm -fr $(TARGET_RESOURCE_DIR)
	@-rm -f $(TARGET_BIN_DIR)/$(TARGET)
else
	@echo "Must be root to run make uninstall"
endif

xxx:
	echo $(LIBS)


