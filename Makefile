#
# Makefile for netMonitor and co2Monitor.
#
# Type 'make' or 'make netMonitor' or 'make co2Monitor'to create the binary.
# Type 'make clean' or 'make cleaner' to delete all temporaries.
#


# build target specs
BASE_DIR=.
SRC_DIR = $(BASE_DIR)/src
RESOURCE_DIR = $(BASE_DIR)/resources
SYS_DIR = $(BASE_DIR)/systemd
SCRIPT_DIR = $(BASE_DIR)/scripts
SCRIPTS = co2mon co2log.py change-wifi-passwd
SHELL = /bin/bash

# Valid Development builds are Rel(ease) and Debug
# These determine logging levels, compiler optimisation, etc.
valid_DEV = Rel Debug

ifndef DEV
DEV = Rel
else ifneq ($(filter-out $(valid_DEV),$(DEV)),)
$(error invalid DEV "$(DEV)". Must be one of: $(valid_DEV))
endif

# Address Sanitizer (ASAN), if set, is only used in Debug builds.
ifdef ASAN
ifeq ($(filter y n,$(ASAN)),)
$(error invalid ASAN: must be y or n)
endif
else
ASAN = n
endif

BIN_DIR = $(BASE_DIR)/bin/$(DEV)
OBJ_DIR = $(BASE_DIR)/obj/$(DEV)
LATEST_DIR = $(BASE_DIR)/bin/latest

BUILD_BIN_DIR = $(BIN_DIR)

# Use the most recent target bin for install if DEV
# was not specified on command line or environment
ifeq "$(origin DEV)" "file"
ifneq (,$(findstring install,$(MAKECMDGOALS)))
BUILD_BIN_DIR = $(LATEST_DIR)
endif
endif

TARGET = co2Monitor
TARGET_BIN_DIR = /usr/local/bin
TARGET_RESOURCE_DIR = $(TARGET_BIN_DIR)/$(TARGET).d
SDL_BMP_DIR = $(TARGET_RESOURCE_DIR)/bmp
SDL_TTF_DIR = $(TARGET_RESOURCE_DIR)/ttf

CC = g++
PROTOC = protoc
PROTOCFLAGS = -I=$(SRC_DIR) --cpp_out=$(SRC_DIR)

CFLAGS = -Wall -Werror --pedantic -std=c++20 -D_REENTRANT -D_GNU_SOURCE -Wall -Wno-unused -fno-strict-aliasing -DBASE_THREADSAFE -I.

CODECHECK = cppcheck
CODECHECK_DIR = $(BASE_DIR)/check
CODECHECKFLAGS = --force --cppcheck-build-dir=$(CODECHECK_DIR) --std=c++20 -DBASE_THREADSAFE -I.

ifeq ($(DEV),Rel)
	CFLAGS +=  -O3
	SYSLOGLEVEL = INFO
else
	CFLAGS += -g -DDEBUG -D_DEBUG
	SYSLOGLEVEL = DEBUG
	ifeq ($(ASAN),y)
		CFLAGS += -fsanitize=address -O1 -fno-omit-frame-pointer -fsanitize-address-use-after-scope
		CFLAGS += -DASAN_OPTS="\"strict_string_checks=1:detect_stack_use_after_return=1:check_initialization_order=1:strict_init_order=1:verbosity=1\""
	else
		CFLAGS += -O0
	endif
	CODECHECKFLAGS += -DDEBUG -D_DEBUG
endif

LIBS = -lfmt -lpthread -lconfig++

# SDL headers and libs
# sdl-config, when present, tells us where to find SDL headers and libs.
ifneq (,$(shell which sdl2-config 2>/dev/null))
	SDL2_CFLAGS =
	SDL2_LIBS =
	SDL2_CFLAGS := $(shell sdl2-config --cflags)
	SDL2_LIBS := $(shell sdl2-config --libs)
	SDL2_TTF_LIBS = $(strip $(shell sdl2-config --libs | cut -d' ' -f1 | sed -e 's/-L//')/libSDL2_ttf)
	SDL2_LIBS += -lSDL2_ttf
	CFLAGS += $(SDL2_CFLAGS) -DHAS_SDL2
	LIBS += $(SDL2_LIBS)
	CODECHECKFLAGS += $(SDL2_CFLAGS) -DHAS_SDL2
endif

# WiringPi headers and libs
# gpio is a WiringPi utility which should be a good indicator
# of WiringPi being installed on this platform.
ifneq (,$(shell which gpio 2>/dev/null))
	CFLAGS += -DHAS_WIRINGPI
	LIBS += -lwiringPi
	CODECHECKFLAGS += -DHAS_WIRINGPI
endif

# Protocol Buffers
ifneq (,$(shell pkg-config --libs protobuf 2>/dev/null))
	PROTOBUF_CFLAGS := -DHAS_PROTOBUF $(shell pkg-config --cflags protobuf)
	PROTOBUF_LIBS := $(shell pkg-config --libs protobuf)
	CFLAGS += $(PROTOBUF_CFLAGS)
	LIBS += $(PROTOBUF_LIBS)
	CODECHECKFLAGS += $(PROTOBUF_CFLAGS)
endif

# zeromq
ifneq (,$(shell pkg-config --libs libzmq 2>/dev/null))
	ZMQ_CFLAGS := -DHAS_ZEROMQ $(shell pkg-config --cflags libzmq)
	ZMQ_LIBS := $(shell pkg-config --libs libzmq)
	CFLAGS += $(ZMQ_CFLAGS)
	LIBS += $(ZMQ_LIBS)
	CODECHECKFLAGS += $(ZMQ_CFLAGS)
endif

# System Watchdog only exists on ArchLinux
ifneq ("$(wildcard /usr/include/systemd/sd-daemon.h)","")
	HAS_SYSD_WDOG = Yes
	CFLAGS += -DSYSTEMD_WDOG
	LIBS += -lsystemd
	CODECHECKFLAGS += -DSYSTEMD_WDOG
else
	HAS_SYSD_WDOG = No
endif

# I2C only on RPi devices
ifneq ("$(wildcard /usr/include/i2c/smbus.h)","")
	CFLAGS += -DHAS_I2C
	LIBS += -li2c
	CODECHECKFLAGS += -DHAS_I2C
endif

CO2MON_OBJFILES = co2MonitorMain.o \
	restartMgr.o \
	co2PersistentStore.o \
	co2PersistentConfigStore.o \
	co2Monitor.o \
	co2Display.o \
	co2Screen.o \
	statusScreen.o \
	fanControlScreen.o \
	relHumCo2ThresholdScreen.o \
	shutdownRebootScreen.o \
	confirmCancelScreen.o \
	blankScreen.o \
	splashScreen.o \
	displayElement.o \
	co2TouchScreen.o \
	screenBacklight.o \
	netMonitor.o \
	ping.o \
	parseConfigFile.o \
	config.o \
	co2Defaults.o \
	co2Sensor.o \
	co2SensorK30.o \
	co2SensorSCD30.o \
	co2SensorSim.o \
	serialPort.o \
	co2Message.pb.o \
	utils.o \
	sysdWatchdog.o

CO2MON_OBJS := $(CO2MON_OBJFILES:%=$(OBJ_DIR)/%)
# protobuf sources have .cc filename extension, rather than .cpp
CO2MON_SRCS = $(patsubst %.pb.cpp,%.pb.cc,$(CO2MON_OBJFILES:%.o=$(SRC_DIR)/%.cpp))

# first target entry is the target invoked when typing 'make'
all: $(OBJ_DIR) $(BIN_DIR) $(TARGET)
.PHONY:	all $(TARGET) codecheck clean cleaner install_k30 install_scd30 install_sim install uninstall xxx

$(TARGET): $(BIN_DIR)/$(TARGET)

$(BIN_DIR)/$(TARGET): $(BIN_DIR) $(OBJ_DIR) $(CO2MON_OBJS) $(SYSD_WDOG_OBJ)
	@printf "\033[1;34mLinking  \033[0m %-35.35s " $$(basename $@)"..."
	@$(CC) $(CFLAGS) -o $(BIN_DIR)/$(TARGET) $(CO2MON_OBJS) $(LIBS)
	@-rm -f $(LATEST_DIR) 2>/dev/null
	@-ln -s $(DEV) $(LATEST_DIR)
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/co2MonitorMain.o: $(SRC_DIR)/co2MonitorMain.cpp \
		$(SRC_DIR)/co2Message.pb.h \
		$(SRC_DIR)/parseConfigFile.h $(SRC_DIR)/utils.h
	@printf "\033[1;34mCompiling\033[0m %-35.35s " $$(basename $<)"..."
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/co2MonitorMain.o -c $(SRC_DIR)/co2MonitorMain.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/restartMgr.o: $(SRC_DIR)/restartMgr.cpp $(SRC_DIR)/restartMgr.h \
		$(SRC_DIR)/co2PersistentStore.h $(SRC_DIR)/utils.h
	@printf "\033[1;34mCompiling\033[0m %-35.35s " $$(basename $<)"..."
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/restartMgr.o -c $(SRC_DIR)/restartMgr.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/co2PersistentStore.o: $(SRC_DIR)/co2PersistentStore.cpp $(SRC_DIR)/co2PersistentStore.h \
		$(SRC_DIR)/utils.h
	@printf "\033[1;34mCompiling\033[0m %-35.35s " $$(basename $<)"..."
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/co2PersistentStore.o -c $(SRC_DIR)/co2PersistentStore.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/co2PersistentConfigStore.o: $(SRC_DIR)/co2PersistentConfigStore.cpp $(SRC_DIR)/co2PersistentConfigStore.h \
		$(SRC_DIR)/utils.h
	@printf "\033[1;34mCompiling\033[0m %-35.35s " $$(basename $<)"..."
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/co2PersistentConfigStore.o -c $(SRC_DIR)/co2PersistentConfigStore.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/co2Monitor.o: $(SRC_DIR)/co2Monitor.cpp $(SRC_DIR)/co2Monitor.h \
		$(SRC_DIR)/co2Message.pb.h \
		$(SRC_DIR)/parseConfigFile.h $(SRC_DIR)/utils.h
	@printf "\033[1;34mCompiling\033[0m %-35.35s " $$(basename $<)"..."
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/co2Monitor.o -c $(SRC_DIR)/co2Monitor.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/co2Display.o: $(SRC_DIR)/co2Display.cpp $(SRC_DIR)/co2Display.h \
		$(SRC_DIR)/co2Message.pb.h \
		$(SRC_DIR)/parseConfigFile.h $(SRC_DIR)/utils.h
	@printf "\033[1;34mCompiling\033[0m %-35.35s " $$(basename $<)"..."
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/co2Display.o -c $(SRC_DIR)/co2Display.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/co2Screen.o: $(SRC_DIR)/co2Screen.cpp $(SRC_DIR)/co2Screen.h \
		$(SRC_DIR)/co2Message.pb.h \
		$(SRC_DIR)/parseConfigFile.h $(SRC_DIR)/utils.h
	@printf "\033[1;34mCompiling\033[0m %-35.35s " $$(basename $<)"..."
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/co2Screen.o -c $(SRC_DIR)/co2Screen.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/statusScreen.o: $(SRC_DIR)/statusScreen.cpp $(SRC_DIR)/co2Screen.h \
		$(SRC_DIR)/co2Message.pb.h \
		$(SRC_DIR)/parseConfigFile.h $(SRC_DIR)/utils.h
	@printf "\033[1;34mCompiling\033[0m %-35.35s " $$(basename $<)"..."
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/statusScreen.o -c $(SRC_DIR)/statusScreen.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/fanControlScreen.o: $(SRC_DIR)/fanControlScreen.cpp $(SRC_DIR)/co2Screen.h \
		$(SRC_DIR)/co2Message.pb.h \
		$(SRC_DIR)/parseConfigFile.h $(SRC_DIR)/utils.h
	@printf "\033[1;34mCompiling\033[0m %-35.35s " $$(basename $<)"..."
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/fanControlScreen.o -c $(SRC_DIR)/fanControlScreen.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/relHumCo2ThresholdScreen.o: $(SRC_DIR)/relHumCo2ThresholdScreen.cpp $(SRC_DIR)/co2Screen.h \
		$(SRC_DIR)/co2Message.pb.h \
		$(SRC_DIR)/parseConfigFile.h $(SRC_DIR)/utils.h
	@printf "\033[1;34mCompiling\033[0m %-35.35s " $$(basename $<)"..."
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/relHumCo2ThresholdScreen.o -c $(SRC_DIR)/relHumCo2ThresholdScreen.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/shutdownRebootScreen.o: $(SRC_DIR)/shutdownRebootScreen.cpp $(SRC_DIR)/co2Screen.h \
		$(SRC_DIR)/co2Message.pb.h \
		$(SRC_DIR)/parseConfigFile.h $(SRC_DIR)/utils.h
	@printf "\033[1;34mCompiling\033[0m %-35.35s " $$(basename $<)"..."
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/shutdownRebootScreen.o -c $(SRC_DIR)/shutdownRebootScreen.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/confirmCancelScreen.o: $(SRC_DIR)/confirmCancelScreen.cpp $(SRC_DIR)/co2Screen.h \
		$(SRC_DIR)/co2Message.pb.h \
		$(SRC_DIR)/parseConfigFile.h $(SRC_DIR)/utils.h
	@printf "\033[1;34mCompiling\033[0m %-35.35s " $$(basename $<)"..."
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/confirmCancelScreen.o -c $(SRC_DIR)/confirmCancelScreen.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/blankScreen.o: $(SRC_DIR)/blankScreen.cpp $(SRC_DIR)/co2Screen.h \
		$(SRC_DIR)/co2Message.pb.h \
		$(SRC_DIR)/parseConfigFile.h $(SRC_DIR)/utils.h
	@printf "\033[1;34mCompiling\033[0m %-35.35s " $$(basename $<)"..."
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/blankScreen.o -c $(SRC_DIR)/blankScreen.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/splashScreen.o: $(SRC_DIR)/splashScreen.cpp $(SRC_DIR)/co2Screen.h \
		$(SRC_DIR)/co2Message.pb.h \
		$(SRC_DIR)/parseConfigFile.h $(SRC_DIR)/utils.h
	@printf "\033[1;34mCompiling\033[0m %-35.35s " $$(basename $<)"..."
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/splashScreen.o -c $(SRC_DIR)/splashScreen.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/displayElement.o: $(SRC_DIR)/displayElement.cpp $(SRC_DIR)/displayElement.h \
		$(SRC_DIR)/parseConfigFile.h $(SRC_DIR)/utils.h
	@printf "\033[1;34mCompiling\033[0m %-35.35s " $$(basename $<)"..."
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/displayElement.o -c $(SRC_DIR)/displayElement.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/co2TouchScreen.o: $(SRC_DIR)/co2TouchScreen.cpp $(SRC_DIR)/co2TouchScreen.h \
		$(SRC_DIR)/co2Message.pb.h \
		$(SRC_DIR)/parseConfigFile.h $(SRC_DIR)/utils.h
	@printf "\033[1;34mCompiling\033[0m %-35.35s " $$(basename $<)"..."
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/co2TouchScreen.o -c $(SRC_DIR)/co2TouchScreen.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/screenBacklight.o: $(SRC_DIR)/screenBacklight.cpp $(SRC_DIR)/screenBacklight.h \
		$(SRC_DIR)/co2Message.pb.h \
		$(SRC_DIR)/parseConfigFile.h $(SRC_DIR)/utils.h
	@printf "\033[1;34mCompiling\033[0m %-35.35s " $$(basename $<)"..."
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/screenBacklight.o -c $(SRC_DIR)/screenBacklight.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/netMonitor.o: $(SRC_DIR)/netMonitor.cpp $(SRC_DIR)/netMonitor.h \
		$(SRC_DIR)/co2Message.pb.h \
		$(SRC_DIR)/parseConfigFile.h $(SRC_DIR)/utils.h
	@printf "\033[1;34mCompiling\033[0m %-35.35s " $$(basename $<)"..."
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/netMonitor.o -c $(SRC_DIR)/netMonitor.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/ping.o: $(SRC_DIR)/ping.cpp $(SRC_DIR)/ping.h
	@printf "\033[1;34mCompiling\033[0m %-35.35s " $$(basename $<)"..."
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/ping.o -c $(SRC_DIR)/ping.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/parseConfigFile.o: $(SRC_DIR)/parseConfigFile.cpp $(SRC_DIR)/parseConfigFile.h
	@printf "\033[1;34mCompiling\033[0m %-35.35s " $$(basename $<)"..."
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/parseConfigFile.o -c $(SRC_DIR)/parseConfigFile.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/config.o: $(SRC_DIR)/config.cpp $(SRC_DIR)/config.h
	@printf "\033[1;34mCompiling\033[0m %-35.35s " $$(basename $<)"..."
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/config.o -c $(SRC_DIR)/config.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/co2Defaults.o: $(SRC_DIR)/co2Defaults.cpp $(SRC_DIR)/co2Defaults.h
	@printf "\033[1;34mCompiling\033[0m %-35.35s " $$(basename $<)"..."
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/co2Defaults.o -c $(SRC_DIR)/co2Defaults.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/co2Sensor.o: $(SRC_DIR)/co2Sensor.cpp $(SRC_DIR)/co2Sensor.h
	@printf "\033[1;34mCompiling\033[0m %-35.35s " $$(basename $<)"..."
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/co2Sensor.o -c $(SRC_DIR)/co2Sensor.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/co2SensorK30.o: $(SRC_DIR)/co2SensorK30.cpp $(SRC_DIR)/co2SensorK30.h
	@printf "\033[1;34mCompiling\033[0m %-35.35s " $$(basename $<)"..."
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/co2SensorK30.o -c $(SRC_DIR)/co2SensorK30.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/co2SensorSCD30.o: $(SRC_DIR)/co2SensorSCD30.cpp $(SRC_DIR)/co2SensorSCD30.h
	@printf "\033[1;34mCompiling\033[0m %-35.35s " $$(basename $<)"..."
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/co2SensorSCD30.o -c $(SRC_DIR)/co2SensorSCD30.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/co2SensorSim.o: $(SRC_DIR)/co2SensorSim.cpp $(SRC_DIR)/co2SensorSim.h
	@printf "\033[1;34mCompiling\033[0m %-35.35s " $$(basename $<)"..."
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/co2SensorSim.o -c $(SRC_DIR)/co2SensorSim.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/serialPort.o: $(SRC_DIR)/serialPort.cpp $(SRC_DIR)/serialPort.h
	@printf "\033[1;34mCompiling\033[0m %-35.35s " $$(basename $<)"..."
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/serialPort.o -c $(SRC_DIR)/serialPort.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/utils.o: $(SRC_DIR)/utils.cpp $(SRC_DIR)/utils.h
	@printf "\033[1;34mCompiling\033[0m %-35.35s " $$(basename $<)"..."
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/utils.o -c $(SRC_DIR)/utils.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/sysdWatchdog.o: $(SRC_DIR)/sysdWatchdog.cpp $(SRC_DIR)/sysdWatchdog.h
	@printf "\033[1;34mCompiling\033[0m %-35.35s " $$(basename $<)"..."
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/sysdWatchdog.o -c $(SRC_DIR)/sysdWatchdog.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/co2Message.pb.o: $(SRC_DIR)/co2Message.pb.cc
	@printf "\033[1;34mCompiling\033[0m %-35.35s " $$(basename $<)"..."
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/co2Message.pb.o -c $(SRC_DIR)/co2Message.pb.cc
	@printf "\033[1;32mDone\033[0m\n"

$(SRC_DIR)/co2Message.pb.h: $(SRC_DIR)/co2Message.pb.cc

$(SRC_DIR)/co2Message.pb.cc: $(SRC_DIR)/co2Message.proto
	@printf "\033[1;34mCompiling\033[0m %-35.35s " $$(basename $<)"..."
	@protoc $(PROTOCFLAGS) $(SRC_DIR)/co2Message.proto
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR) 2>/dev/null

$(BIN_DIR):
	@mkdir -p $(BIN_DIR) 2>/dev/null

codecheck: $(CODECHECK_DIR) $(CO2MON_SRCS)
	@$(CODECHECK) $(CODECHECKFLAGS) $(CO2MON_SRCS)

$(CODECHECK_DIR):
	@mkdir -p $(CODECHECK_DIR) 2>/dev/null

clean:
	@echo -n 'Removing all temporary binaries... '
	@-rm -f $(BIN_DIR)/* $(OBJ_DIR)/*.o $(CODECHECK_DIR) $(LATEST_DIR) 2>/dev/null
	@echo Done.

cleaner:
	@echo -n 'Removing all temporary binaries and their directories... '
	@-rm -rf  $(SRC_DIR)/*.pb.cc $(SRC_DIR)/*.pb.h $(CODECHECK_DIR) $(LATEST_DIR) 2>/dev/null
	@for D in $(valid_DEV); do rm -fr $$(dirname $(BIN_DIR))/$$D $$(dirname $(OBJ_DIR))/$$D; done 2>/dev/null
	@echo Done.

install_k30 install_scd30 install_sim: install_%:
ifeq ($(shell id -u), 0)
	@install -d $(TARGET_BIN_DIR)
	@install -d $(TARGET_RESOURCE_DIR)
	@install -d $(SDL_BMP_DIR)
	@install -d $(SDL_TTF_DIR)
	@install -m 444 -D $(RESOURCE_DIR)/*.bmp $(SDL_BMP_DIR)
	@install -m 444 -D $(RESOURCE_DIR)/*.ttf $(SDL_TTF_DIR)
	@install -m 755 -D $(BUILD_BIN_DIR)/$(TARGET) $(TARGET_BIN_DIR)
	@install -m 755 -D $(SCRIPT_DIR)/* $(TARGET_BIN_DIR)
	@for S in $(SCRIPTS); do  install -m 755 -D $(SCRIPT_DIR)/$$S $(TARGET_BIN_DIR); done
	@shasum -a 512256 $(TARGET_BIN_DIR)/$(TARGET) $(SDL_BMP_DIR)/* $(SDL_TTF_DIR)/* > $(TARGET_RESOURCE_DIR)/$(TARGET).cksum
	@$(SHELL) -c "$(SYS_DIR)/mksystemd.sh --sensor=$* --loglevel=$(SYSLOGLEVEL)"
else
	$(error "Must be root to run make install")
endif

install install_default: install_k30

uninstall:
ifeq ($(shell id -u), 0)
	@$(SHELL) -c "$(SYS_DIR)/mksystemd.sh -u"
	@-rm -fr $(TARGET_RESOURCE_DIR)
	@for S in $(SCRIPTS); do rm -f $(TARGET_BIN_DIR)/$$S; done
	@-rm -f $(TARGET_BIN_DIR)/$(TARGET)
else
	$(error "Must be root to run make uninstall")
endif

xxx:
	@echo $(CFLAGS)
	@echo
	@echo $(LIBS)
	@$(CC) $(CFLAGS) -o xx.e -E $(SRC_DIR)/xx.cpp


