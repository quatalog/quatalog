CXX = clang++
CXXFLAGS = -O2 -march=native -std=c++17

CourseOfferingsScraper: CourseOfferingsScraper.cpp json.o

json.o: 3rdparty/jsoncpp/dist/jsoncpp.cpp
	$(CXX) -c $(CXXFLAGS) $? -o $@

3rdparty/jsoncpp/dist/jsoncpp.cpp:
	git submodule update --init
	cd 3rdparty/jsoncpp; ./amalgamate.py

%.o: %.cpp %.hpp
	$(CXX) $(CXXFLAGS) $? -o $@
