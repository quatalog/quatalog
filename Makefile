CXX = clang++
CXXFLAGS = -O2 -march=native -I 3rdparty/jsoncpp -Wall -std=c++17

CourseOfferingsScraper: CourseOfferingsScraper.cpp

%.o: %.cpp %.hpp
	$(CXX) $(CXXFLAGS) -o $@

jsoncpp/3rdparty/dist:
	git submodule update --init
	cd 3rdparty/jsoncpp; ./amalgamate.py
