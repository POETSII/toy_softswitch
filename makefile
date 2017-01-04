
softswitch : $(wildcard *.cpp) $(wildcard *.hpp)
	$(CXX) -std=c++11 -W -Wall -o $@ $(wildcard *.cpp)
