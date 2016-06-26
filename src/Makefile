#
# Makefile for netMonitor and co2Monitor.
#
# Type 'make' or 'make netMonitor' or 'make co2Monitor'to create the binary.
# Type 'make clean' or 'make cleaner' to delete all temporaries.
#
DEV = Debug
#DEV = Rel

# build target specs
CC = g++
PROTOC = protoc
PROTOCFLAGS = --cpp_out=.

ifeq ($(DEV),Rel)
	CFLAGS = -Wall -Werror --pedantic -O2 -std=c++11 -D_REENTRANT -D_GNU_SOURCE -Wall -Wno-unused -fno-strict-aliasing -DBASE_THREADSAFE -I. 
else
	CFLAGS = -g -Wall -Werror -DDEBUG -D_DEBUG --pedantic -O0 -std=c++11 -D_REENTRANT -D_GNU_SOURCE -Wno-unused -fno-strict-aliasing -DBASE_THREADSAFE -I.
endif
LIBS =

# SDL headers and libs
# sdl-config, when present, tells us where to find SDL headers and libs.
ifneq (,$(shell which sdl-config 2>/dev/null))
	SDL_CFLAGS =
	SDL_LIBS =
	SDL_CFLAGS := $(shell sdl-config --cflags)
	SDL_LIBS := $(shell sdl-config --libs)
	SDL_LIBS += -l SDL_ttf
	CFLAGS += $(SDL_CFLAGS) -DHAS_SDL
	LIBS += $(SDL_LIBS)
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

OBJ_DIR = ../obj
ifeq ($(DEV),Rel)
	BIN_DIR = ../bin
else
	BIN_DIR = ./Debug
endif

ifeq ($(HAS_SYSD_WDOG),Yes)
	SYSD_WDOG_OBJ = $(OBJ_DIR)/sysdWatchdog.o
endif

# first target entry is the target invoked when typing 'make'
all: $(OBJ_DIR) $(BIN_DIR) co2Monitor
.PHONY:	all co2Monitor clean cleaner

co2Monitor: $(BIN_DIR)/co2Monitor

$(BIN_DIR)/netMonitor: $(OBJ_DIR)/netMonitorMain.o $(OBJ_DIR)/netMonitor.o $(OBJ_DIR)/config.o $(OBJ_DIR)/parseConfigFile.o $(OBJ_DIR)/utils.o $(OBJ_DIR)/ping.o $(OBJ_DIR)/netLink.o $(SYSD_WDOG_OBJ)
	@printf "\033[1;34mLinking  \033[0m netMonitor...\t\t\t"
	@$(CC) $(CFLAGS) -o $(BIN_DIR)/netMonitor $(OBJ_DIR)/netMonitorMain.o $(OBJ_DIR)/netMonitor.o $(OBJ_DIR)/config.o $(OBJ_DIR)/parseConfigFile.o $(OBJ_DIR)/utils.o  $(OBJ_DIR)/ping.o $(OBJ_DIR)/netLink.o $(SYSD_WDOG_OBJ) $(LIBS)
	@printf "\033[1;32mDone\033[0m\n"

$(BIN_DIR)/co2Monitor: $(BIN_DIR) $(OBJ_DIR) $(OBJ_DIR)/config.o $(OBJ_DIR)/co2MonitorMain.o $(OBJ_DIR)/co2Monitor.o $(OBJ_DIR)/parseConfigFile.o $(OBJ_DIR)/utils.o  $(OBJ_DIR)/co2Message.pb.o $(SYSD_WDOG_OBJ)
	@printf "\033[1;34mLinking  \033[0m co2Monitor...\t\t\t"
	@$(CC) $(CFLAGS) -o $(BIN_DIR)/co2Monitor \
		$(OBJ_DIR)/config.o $(OBJ_DIR)/co2MonitorMain.o \
		$(OBJ_DIR)/parseConfigFile.o $(OBJ_DIR)/utils.o  \
		$(OBJ_DIR)/co2Message.pb.o \
		$(SYSD_WDOG_OBJ) $(LIBS)
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/config.o: config.cpp config.h
	@printf "\033[1;34mCompiling\033[0m config.cpp...\t\t\t"
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/config.o -c config.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/netMonitor.o: netMonitor.cpp netMonitor.h \
		parseConfigFile.h utils.h
	@printf "\033[1;34mCompiling\033[0m netMonitor.cpp...\t\t"
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/netMonitor.o -c netMonitor.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/netMonitorMain.o: netMonitorMain.cpp \
		parseConfigFile.h utils.h
	@printf "\033[1;34mCompiling\033[0m netMonitorMain.cpp...\t\t"
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/netMonitorMain.o -c netMonitorMain.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/co2Monitor.o: co2Monitor.cpp co2Monitor.h \
		parseConfigFile.h utils.h
	@printf "\033[1;34mCompiling\033[0m co2Monitor.cpp...\t\t"
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/co2Monitor.o -c co2Monitor.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/co2MonitorMain.o: co2MonitorMain.cpp \
		parseConfigFile.h utils.h
	@printf "\033[1;34mCompiling\033[0m co2MonitorMain.cpp...\t\t"
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/co2MonitorMain.o -c co2MonitorMain.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/parseConfigFile.o: parseConfigFile.cpp parseConfigFile.h
	@printf "\033[1;34mCompiling\033[0m parseConfigFile.cpp...\t"
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/parseConfigFile.o -c parseConfigFile.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/utils.o: utils.cpp utils.h
	@printf "\033[1;34mCompiling\033[0m utils.cpp...\t\t\t"
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/utils.o -c utils.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/ping.o: ping.cpp ping.h
	@printf "\033[1;34mCompiling\033[0m ping.cpp...\t\t\t"
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/ping.o -c ping.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/netLink.o: netLink.cpp netLink.h
	@printf "\033[1;34mCompiling\033[0m netLink.cpp...\t\t"
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/netLink.o -c netLink.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/sysdWatchdog.o: sysdWatchdog.cpp sysdWatchdog.h
	@printf "\033[1;34mCompiling\033[0m sysdWatchdog.cpp...\t\t"
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/sysdWatchdog.o -c sysdWatchdog.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/co2Message.pb.o: co2Message.pb.cc
	@printf "\033[1;34mCompiling\033[0m co2Message.pb.cpp...\t\t"
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/co2Message.pb.o -c co2Message.pb.cc
	@printf "\033[1;32mDone\033[0m\n"

co2Message.pb.cc: co2Message.proto
	@printf "\033[1;34mCompiling\033[0m co2Message.proto...\t\t"
	@protoc --cpp_out=. co2Message.proto
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR) 2>/dev/null

$(BIN_DIR):
	@mkdir -p $(BIN_DIR) 2>/dev/null

clean:
	@echo -n 'Removing all temporary binaries... '
	@rm -f $(BIN_DIR)/co2Monitor $(BIN_DIR)/netMonitor $(OBJ_DIR)/*.o 2>/dev/null
	@echo Done.

cleaner:
	@echo -n 'Removing all temporary binaries and their directories... '
	@rm -rf $(OBJ_DIR) $(BIN_DIR) ./*.pb.cc ./*.pb.h 2>/dev/null
	@echo Done.

