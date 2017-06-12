
CXXFLAGS += -g -std=c++11 -W -Wall -I include -pthread -Wno-unused-parameter

#CXXFLAGS += -fsanitize=undefined -fno-sanitize-recover

lib/tinsel_mpi.a : src/tinsel/tinsel_on_mpi.cpp src/tinsel/tinsel_mbox.hpp
	mpicxx $(CXXFLAGS) -c -o src/tinsel/tinsel_on_mpi.o src/tinsel/tinsel_on_mpi.cpp
	mkdir -p lib
	ar rcs $@ src/tinsel/tinsel_on_mpi.o
	
lib/tinsel_unix.a : src/tinsel/tinsel_on_unix.cpp src/tinsel/tinsel_mbox.hpp
	g++ $(CXXFLAGS) -c -o src/tinsel/tinsel_on_unix.o src/tinsel/tinsel_on_unix.cpp
	mkdir -p lib
	ar rcs $@ src/tinsel/tinsel_on_unix.o

lib/softswitch.a : $(patsubst %.cpp,%.o,$(wildcard src/softswitch/*.cpp)) $
	mkdir -p lib
	ar rcs $@ $^

	
define manual_app_template
# $1 = app
# $2 = config

lib/$1_$2.a : $(patsubst %.cpp,%.o,src/applications/$1/$1_vtables.cpp src/applications/$1/$1_$2.cpp)
	mkdir -p lib
	ar rcs $$@ $$^
	
ALL_MANUAL_LIBS := $(ALL_MANUAL_LIBS) $1_$2

all_manual_libs : lib/$1_$2.a

endef



$(eval $(call manual_app_template,ring,dev2_threads1))
$(eval $(call manual_app_template,ring,dev2_threads2))
$(eval $(call manual_app_template,ring,dev4_threads2))

$(eval $(call manual_app_template,barrier,dev3_threads2))
$(eval $(call manual_app_template,barrier,dev5_threads3))

$(eval $(call manual_app_template,edge_props,dev4_threads1))



define generated_app_template

lib/$1.a : $(patsubst %.cpp,%.o,$(wildcard generated/apps/$1/*.cpp))
	echo "Gen: $$^"
	mkdir -p lib
	ar rcs $$@ $$^

ALL_GENERATED_LIBS := $(ALL_GENERATED_LIBS) $1

all_generated_libs : lib/$1.a

endef


GENERATED_APPS := $(patsubst generated/apps/%,%,$(wildcard generated/apps/*))

$(foreach ga,$(GENERATED_APPS),$(eval $(call generated_app_template,$(ga))))

ALL_LIBS := $(ALL_GENERATED_LIBS) $(ALL_MANUAL_LIBS)

bin/run_unix_% : lib/tinsel_unix.a lib/softswitch.a lib/%.a
	mkdir -p bin
	$(CXX) -pthread -std=c++11 -W -Wall -o $@  $^ -fsanitize=undefined -fno-sanitize-recover

bin/run_mpi_% : lib/tinsel_mpi.a lib/softswitch.a lib/%.a
	mkdir -p bin
	mpicxx -pthread -std=c++11 -W -Wall -o $@  $^ -lmpi


all_unix : $(foreach l,$(ALL_LIBS),bin/run_unix_$(l))

all_mpi : $(foreach l,$(ALL_LIBS),bin/run_mpi_$(l))


###################################################################
## Initial proof-of-concept data-structures from ADB. Deprecated?

adb_poc : lib/softswitch.a src/adb/main.cpp src/adb/dummies.cpp
	$(CXX) $(CXXFLAGS) -pthread -std=c++11 -W -Wall -o $@  $(filter %.o %.cpp,$^) $(filter %.a,$^)
