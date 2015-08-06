#
# Makefile for 'co2Monitor'.
#
# Type 'make' or 'make co2Monitor' to create the binary.
# Type 'make clean' or 'make clear' to delete all temporaries.
# Type 'make run' to execute the binary.
# Type 'make debug' to debug the binary using gdb(1).
#

# build target specs
CC = gcc
CFLAGS = -O3 
OUT_DIR = .
LIBS =

# first target entry is the target invoked when typing 'make'
default: co2Monitor

co2Monitor: $(OUT_DIR)/co2io.o $(OUT_DIR)/co2monitor.o $(OUT_DIR)/serial.o
	@echo -n 'Linking co2Monitor... '
	@$(CC) $(CFLAGS) -o co2Monitor $(OUT_DIR)/co2io.o $(OUT_DIR)/co2monitor.o $(OUT_DIR)/serial.o $(LIBS)
	@echo Done.

$(OUT_DIR)/co2io.o: co2io.c co2io.h serial.h
	@echo -n 'Compiling co2io.c... '
	@$(CC) $(CFLAGS) -o $(OUT_DIR)/co2io.o -c co2io.c
	@echo Done.

$(OUT_DIR)/co2monitor.o: co2monitor.c serial.h co2io.h
	@echo -n 'Compiling co2monitor.c... '
	@$(CC) $(CFLAGS) -o $(OUT_DIR)/co2monitor.o -c co2monitor.c
	@echo Done.

$(OUT_DIR)/serial.o: serial.c serial.h
	@echo -n 'Compiling serial.c... '
	@$(CC) $(CFLAGS) -o $(OUT_DIR)/serial.o -c serial.c
	@echo Done.

run:
	./co2Monitor 

debug:
	gdb ./co2Monitor

clean:
	@echo -n 'Removing all temporary binaries... '
	@rm -f co2Monitor $(OUT_DIR)/*.o
	@echo Done.

clear:
	@echo -n 'Removing all temporary binaries... '
	@rm -f co2Monitor $(OUT_DIR)/*.o
	@echo Done.

