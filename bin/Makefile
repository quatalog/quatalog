CXX = clang++
CXXFLAGS = -O2 -march=native -Wall -std=c++17 -I../3rdparty/jsoncpp/dist/
vpath %.cpp ../src

CourseOfferingsScraper: CourseOfferingsScraper.cpp json.o

json.o: ../3rdparty/jsoncpp/dist/jsoncpp.cpp
	if ! test -f "$@"; then $(CXX) -c $(CXXFLAGS) $? -o $@; fi

../3rdparty/jsoncpp/dist/jsoncpp.cpp:
	git submodule update --init
	cd ../3rdparty/jsoncpp; ./amalgamate.py

%.o: %.cpp %.hpp
	$(CXX) $(CXXFLAGS) $? -o $@
