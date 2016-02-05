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
ifeq ($(DEV),Rel)
	CFLAGS = -Wall -Werror --pedantic -O2 -std=c++11
else
	CFLAGS = -g -Wall -Werror -DDEBUG -D_DEBUG --pedantic -O0 -std=c++11
endif
LIBS =

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
all: $(OBJ_DIR) $(BIN_DIR) netMonitor co2Monitor
.PHONY:	all netMonitor co2Monitor clean cleaner

netMonitor: $(BIN_DIR)/netMonitor

co2Monitor: $(BIN_DIR)/co2Monitor

$(BIN_DIR)/netMonitor: $(OBJ_DIR)/netMonitorMain.o $(OBJ_DIR)/netMonitor.o $(OBJ_DIR)/config.o $(OBJ_DIR)/parseConfigFile.o $(OBJ_DIR)/utils.o $(SYSD_WDOG_OBJ)
	@printf "\033[1;34mLinking  \033[0m netMonitor...\t\t\t"
	@$(CC) $(CFLAGS) -o $(BIN_DIR)/netMonitor $(OBJ_DIR)/netMonitorMain.o $(OBJ_DIR)/netMonitor.o $(OBJ_DIR)/config.o $(OBJ_DIR)/parseConfigFile.o $(OBJ_DIR)/utils.o  $(SYSD_WDOG_OBJ) $(LIBS)
	@printf "\033[1;32mDone\033[0m\n"

$(BIN_DIR)/co2Monitor: $(OBJ_DIR)/config.o $(OBJ_DIR)/netMonitorMain.o $(OBJ_DIR)/parseConfigFile.o $(OBJ_DIR)/utils.o  $(SYSD_WDOG_OBJ)
	@printf "\033[1;34mLinking  \033[0m co2Monitor...\t\t\t"
	@$(CC) $(CFLAGS) -o $(BIN_DIR)/co2Monitor $(OBJ_DIR)/config.o $(OBJ_DIR)/netMonitorMain.o $(OBJ_DIR)/parseConfigFile.o $(OBJ_DIR)/utils.o  $(SYSD_WDOG_OBJ) $(LIBS)
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

$(OBJ_DIR)/parseConfigFile.o: parseConfigFile.cpp parseConfigFile.h
	@printf "\033[1;34mCompiling\033[0m parseConfigFile.cpp...\t"
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/parseConfigFile.o -c parseConfigFile.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/utils.o: utils.cpp utils.h
	@printf "\033[1;34mCompiling\033[0m utils.cpp...\t\t\t"
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/utils.o -c utils.cpp
	@printf "\033[1;32mDone\033[0m\n"

$(OBJ_DIR)/sysdWatchdog.o: sysdWatchdog.cpp sysdWatchdog.h
	@printf "\033[1;34mCompiling\033[0m sysdWatchdog.cpp...\t\t\t"
	@$(CC) $(CFLAGS) -o $(OBJ_DIR)/sysdWatchdog.o -c sysdWatchdog.cpp
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
	@rm -rf $(OBJ_DIR) $(BIN_DIR) 2>/dev/null
	@echo Done.

