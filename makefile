
CXXFLAGS += -g -std=c++11 -W -Wall -I include -pthread

lib/tinsel_mpi.a : src/tinsel/tinsel_on_mpi.cpp
	mpicxx $(CXXFLAGS) -c -o src/tinsel/tinsel_on_mpi.o src/tinsel/tinsel_on_mpi.cpp
	mkdir -p lib
	ar rcs $@ src/tinsel/tinsel_on_mpi.o
	
lib/tinsel_unix.a : src/tinsel/tinsel_on_unix.o
	mkdir -p lib
	ar rcs $@ $^

lib/softswitch.a : $(patsubst %.cpp,%.o,$(wildcard src/softswitch/*.cpp))
	mkdir -p lib
	ar rcs $@ $^

lib/ring_dev2_threads1.a : $(patsubst %.cpp,%.o,src/applications/ring/ring_vtables.cpp src/applications/ring/ring_dev2_threads1.cpp)
	mkdir -p lib
	ar rcs $@ $^

lib/ring_dev2_threads2.a : $(patsubst %.cpp,%.o,src/applications/ring/ring_vtables.cpp src/applications/ring/ring_dev2_threads2.cpp)
	mkdir -p lib
	ar rcs $@ $^

lib/ring_dev4_threads2.a : $(patsubst %.cpp,%.o,src/applications/ring/ring_vtables.cpp src/applications/ring/ring_dev4_threads2.cpp)
	mkdir -p lib
	ar rcs $@ $^

lib/barrier_dev3_threads2.a : $(patsubst %.cpp,%.o,src/applications/barrier/barrier_vtables.cpp src/applications/barrier/barrier_dev3_threads2.cpp)
	mkdir -p lib
	ar rcs $@ $^

lib/barrier_dev5_threads3.a : $(patsubst %.cpp,%.o,src/applications/barrier/barrier_vtables.cpp src/applications/barrier/barrier_dev5_threads3.cpp)
	mkdir -p lib
	ar rcs $@ $^

lib/edge_props_dev4_threads1.a : $(patsubst %.cpp,%.o,src/applications/edge_props/edge_props_vtables.cpp src/applications/edge_props/edge_props_dev4_threads1.cpp)
	mkdir -p lib
	ar rcs $@ $^


define generated_app_template

lib/$1.a : $(patsubst %.cpp,%.o,$(wildcard generated/apps/$1/*.cpp))
	echo "Gen: $$^"
	mkdir -p lib
	ar rcs $$@ $$^

all_generated_apps : lib/$1.a

endef


GENERATED_APPS := $(patsubst generated/apps/%,%,$(wildcard generated/apps/*))

$(foreach ga,$(GENERATED_APPS),$(eval $(call generated_app_template,$(ga))))


run_unix_% : lib/tinsel_unix.a lib/softswitch.a lib/%.a
	$(CXX) -pthread -std=c++11 -W -Wall -o $@  $^

run_mpi_% : lib/tinsel_mpi.a lib/softswitch.a lib/%.a
	mpicxx -pthread -std=c++11 -W -Wall -o $@  $^ -lmpi


adb_poc : lib/softswitch.a src/adb/main.cpp src/adb/dummies.cpp
	$(CXX) $(CXXFLAGS) -pthread -std=c++11 -W -Wall -o $@  $(filter %.o %.cpp,$^) $(filter %.a,$^)
