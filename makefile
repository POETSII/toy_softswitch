
CXXFLAGS += -g -std=c++11 -W -Wall -I include -pthread

lib/tinsel.a : $(patsubst %.cpp,%.o,$(wildcard src/tinsel/*.cpp))
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

thread_% : lib/tinsel.a lib/softswitch.a lib/%.a
	$(CXX) -pthread -std=c++11 -W -Wall -o $@  $^ -pthread  -Wl,--no-as-needed -lpthread
