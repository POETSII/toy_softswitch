# Tinsel root
TINSEL_ROOT=${POETS_TINSEL_ROOT}
TOY_SOFTSWITCH_ROOT=../..

TINSEL_INC=${POETS_TINSEL_ROOT}

DESIGN=true_tinsel

APPLICATION ?= ../../generated/apps/clock_tree_8_2_threads16

include $(TINSEL_ROOT)/globals.mk

# Local compiler flags
CFLAGS = $(RV_CFLAGS) -Os -I $(INC)  -DNDEBUG=1 -DPOETS_DISABLE_LOGGING=1
LDFLAGS = -melf32lriscv -G 0 

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

APPLICATION_SRCS = $(wildcard $(APPLICATION)/*.cpp)
APPLICATION_OBJS = $(subst .cpp,.riscv.o,$(APPLICATION_SRCS))

%.riscv.o : %.cpp $(INC)/config.h $(INC)/tinsel.h
	$(RV_CXX) $(CFLAGS) -Wall -c -o $@ $<
	

${DESIGN}.elf: ${DESIGN}.riscv.o  $(SOFTSWITCH_OBJS) $(APPLICATION_OBJS) link.ld $(INC)/config.h $(INC)/tinsel.h
	$(RV_LD) $(LDFLAGS) -T link.ld -o ${DESIGN}.elf ${DESIGN}.riscv.o  $(SOFTSWITCH_OBJS) $(APPLICATION_OBJS)

link.ld: genld.sh
	./genld.sh > link.ld

sim.ppm: code.v data.v vis
	hostlink-sim code.v data.v -n 230400 > dump.txt
	./vis dump.txt > sim.ppm

fpga.ppm: code.v data.v vis
	LD_LIBRARY_PATH=$(QUARTUS_ROOTDIR)/linux64 hostlink \
    code.v data.v -n 230400 > dump.txt
	./vis dump.txt > fpga.ppm

.PHONY: run-jtag
run-jtag: fpga.ppm

.PHONY: run-sim
run-sim: sim.ppm

$(INC)/config.h: $(TINSEL_ROOT)/config.py
	make -C $(INC)

.PHONY: clean
clean:
	rm -f *.o *.elf link.ld *.v vis dump.txt *.ppm