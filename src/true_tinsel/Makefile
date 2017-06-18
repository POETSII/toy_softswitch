# Tinsel root
POETS_TINSEL_ROOT ?= $(abspath ../../../tinsel)

export POETS_TINSEL_ROOT

TINSEL_ROOT=${POETS_TINSEL_ROOT}
TOY_SOFTSWITCH_ROOT=../..

TINSEL_INC=${POETS_TINSEL_ROOT}

DESIGN=true_tinsel

APPLICATION ?= ../../generated/apps/clocked_izhikevich_fix_80_20_100_100_threads1024
#APPLICATION ?= ../../generated/apps/clock_tree_8_2_threads1

include $(TINSEL_ROOT)/globals.mk

RV_CXX = riscv64-unknown-elf-g++

# Local compiler flags
CFLAGS = $(RV_CFLAGS)  -I $(INC) -Os -g -DNDEBUG=1  -Wno-unused-variable -Wno-unused-local-typedefs
CFLAGS += -fomit-frame-pointer
CFLAGS += -fno-default-inline
#CFLAGS += -DPOETS_DISABLE_LOGGING=1
CFLAGS += -DPOETS_MAX_LOGGING_LEVEL=3
CFLAGS += -fno-zero-initialized-in-bss
#LDFLAGS = -melf32lriscv -G 0 
LDFLAGS = -Wl,-melf32lriscv  -Wl,--gpsize=0  -Wl,-Bstatic

CFLAGS += -I ${TOY_SOFTSWITCH_ROOT}/include

.PHONY: all
all: code.v data.v

code.v: ${DESIGN}.elf
	$(RV_OBJCOPY) -O verilog --only-section=.text ${DESIGN}.elf code.v

data.v: ${DESIGN}.elf
	$(RV_OBJCOPY) -O verilog --remove-section=.text \
                --set-section-flags .bss=alloc,load,contents ${DESIGN}.elf data.v

SOFTSWITCH_SRCS = $(wildcard $(TOY_SOFTSWITCH_ROOT)/src/softswitch/*.cpp)
SOFTSWITCH_OBJS = $(subst .cpp,.riscv.o,$(SOFTSWITCH_SRCS))

APPLICATION_SRCS = $(wildcard $(APPLICATION)/*.cpp) $(wildcard $(APPLICATION)/*.c)
APPLICATION_OBJS = $(subst .cpp,.riscv.o,$(APPLICATION_SRCS))

%.riscv.o : %.cpp $(INC)/config.h $(INC)/tinsel.h
	$(RV_CXX) $(CFLAGS) -Wall -c -o $@ $<


#${DESIGN}.elf: ${DESIGN}.riscv.o  $(SOFTSWITCH_OBJS) $(APPLICATION_OBJS) link.ld $(INC)/config.h $(INC)/tinsel.h
#	$(RV_LD) $(LDFLAGS) -T link.ld -o ${DESIGN}.elf ${DESIGN}.riscv.o  $(SOFTSWITCH_OBJS) $(APPLICATION_OBJS)

${DESIGN}.elf: ${DESIGN}.cpp  $(SOFTSWITCH_SRCS) $(APPLICATION_SRCS) link.ld $(INC)/config.h $(INC)/tinsel.h
	$(RV_CXX) $(CFLAGS) -Wall entry.S ${DESIGN}.cpp  $(SOFTSWITCH_SRCS) $(APPLICATION_SRCS)  $(LDFLAGS) -T link.ld -o ${DESIGN}.elf 

link.ld: genld.sh
	./genld.sh > link.ld

sim.ppm: code.v data.v vis
	hostlink-sim code.v data.v -n 230400 > dump.txt
	./vis dump.txt > sim.ppm

run-jtag: code.v data.v
	LD_LIBRARY_PATH=$(QUARTUS_ROOTDIR)/linux64 hostlink \
    code.v data.v -n 230400 -p -k out.kv

.PHONY: run-sim
run-sim: sim.ppm

$(INC)/config.h: $(TINSEL_ROOT)/config.py
	make -C $(INC)

.PHONY: clean
clean:
	rm -f *.o *.elf link.ld *.v vis dump.txt *.ppm
