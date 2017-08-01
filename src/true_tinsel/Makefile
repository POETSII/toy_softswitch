# Tinsel root
POETS_TINSEL_ROOT ?= $(abspath ../../../tinsel)
POETS_TOY_SOFTSWITCH_ROOT ?= $(abspath ../..)

BASE=$(abspath $(POETS_TOY_SOFTSWITCH_ROOT)/src/true_tinsel)

export POETS_TINSEL_ROOT

TINSEL_ROOT=${POETS_TINSEL_ROOT}
TINSEL_INC=${POETS_TINSEL_ROOT}

APPLICATION ?= ../../generated/apps/clock_tree_8_3_threads64

DESIGN_ELF ?= true_tinsel.elf

include $(TINSEL_ROOT)/globals.mk

RV_CXX = riscv64-unknown-elf-g++

# Local compiler flags
CFLAGS = $(RV_CFLAGS)  -I $(INC) -Os -g  -Wno-unused-variable -Wno-unused-local-typedefs
CFLAGS += -fomit-frame-pointer
CFLAGS += -fno-default-inline
CFLAGS += -fno-zero-initialized-in-bss
CFLAGS +=  -flto
CFLAGS += -fno-builtin-memcmp -fno-builtin-memcpy

ifeq ($(HARDWARE_ASSERT_ENABLE),0)
CFLAGS += -DNDEBUG=1
endif

ifeq ($(HARDWARE_LOG_LEVEL),0)
CFLAGS += -DPOETS_DISABLE_LOGGING=1
CFLAGS += -DPOETS_MAX_LOGGING_LEVEL=3
else ifneq ($(HARDWARE_LOG_LEVEL),)
CFLAGS += -DPOETS_MAX_LOGGING_LEVEL=$(HARDWARE_LOG_LEVEL)
else
CFLAGS += -DPOETS_MAX_LOGGING_LEVEL=3
endif

ifneq ($(HARDWARE_SOFTSWITCH_LOG_LEVEL),)
CFLAGS += -DPOETS_MAX_SOFTSWITCH_LOGGING_LEVEL=$(HARDWARE_SOFTSWITCH_LOG_LEVEL)
endif

ifneq ($(HARDWARE_HANDLER_LOG_LEVEL),)
CFLAGS += -DPOETS_MAX_HANDLER_LOGGING_LEVEL=$(HARDWARE_HANDLER_LOG_LEVEL)
endif

ifeq ($(HARDWARE_INTRATHREAD_SEND_ENABLE),1)
CFLAGS += -DSOFTSWITCH_ENABLE_INTRATHREAD_SEND=1
endif

ifeq ($(HARDWARE_SEND_OVER_RECV),1)
CFLAGS += -DSOFTSWITCH_ENABLE_SEND_OVER_RECV=1
endif

#LDFLAGS = -melf32lriscv -G 0 
LDFLAGS = -Wl,-melf32lriscv  -Wl,--gpsize=0  -Wl,-Bstatic

CFLAGS += -I ${POETS_TOY_SOFTSWITCH_ROOT}/include
CFLAGS += -I ${POETS_TOY_SOFTSWITCH_ROOT}/src/true_tinsel

.PHONY: all
all: code.v data.v

code.v: ${DESIGN_ELF}
	$(RV_OBJCOPY) -O verilog --only-section=.text ${DESIGN_ELF} code.v

data.v: ${DESIGN_ELF}
	$(RV_OBJCOPY) -O verilog --remove-section=.text \
                --set-section-flags .bss=alloc,load,contents ${DESIGN_ELF} data.v

SOFTSWITCH_SRCS = $(wildcard $(POETS_TOY_SOFTSWITCH_ROOT)/src/softswitch/*.cpp)
SOFTSWITCH_OBJS = $(subst .cpp,.riscv.o,$(SOFTSWITCH_SRCS))

APPLICATION_SRCS = $(wildcard $(APPLICATION)/*.cpp) $(wildcard $(APPLICATION)/*.c)
APPLICATION_OBJS = $(subst .cpp,.riscv.o,$(APPLICATION_SRCS))

TRUE_TINSEL_SRCS = $(POETS_TOY_SOFTSWITCH_ROOT)/src/true_tinsel/true_tinsel.cpp

%.riscv.o : %.cpp $(INC)/config.h $(INC)/tinsel.h
	$(RV_CXX) $(CFLAGS) -Wall -c -o $@ $<


#${DESIGN}.elf: ${DESIGN}.riscv.o  $(SOFTSWITCH_OBJS) $(APPLICATION_OBJS) link.ld $(INC)/config.h $(INC)/tinsel.h
#	$(RV_LD) $(LDFLAGS) -T link.ld -o ${DESIGN}.elf ${DESIGN}.riscv.o  $(SOFTSWITCH_OBJS) $(APPLICATION_OBJS)

${DESIGN_ELF}: ${TRUE_TINSEL_SRCS}  $(SOFTSWITCH_SRCS) $(APPLICATION_SRCS) $(BASE)/link.ld $(INC)/config.h $(INC)/tinsel.h
	time $(RV_CXX) $(CFLAGS) -Wall ${BASE}/entry.S ${TRUE_TINSEL_SRCS}  $(SOFTSWITCH_SRCS) $(APPLICATION_SRCS)  $(LDFLAGS) -T $(BASE)/link.ld -o ${DESIGN_ELF} 

$(BASE)/link.ld: $(BASE)/genld.sh
	$(BASE)/genld.sh > $(BASE)/link.ld

sim.ppm: code.v data.v vis
	hostlink-sim code.v data.v -n 230400 > dump.txt
	./vis dump.txt > sim.ppm

run-jtag: code.v data.v
	LD_LIBRARY_PATH=$(QUARTUS_ROOTDIR)/linux64 hostlink -v \
    code.v data.v -n 230400 -p

.PHONY: run-sim
run-sim: sim.ppm

$(INC)/config.h: $(TINSEL_ROOT)/config.py
	make -C $(INC)

.PHONY: clean
clean:
	rm -f *.o *.elf link.ld *.v vis dump.txt *.ppm
