CXX = clang++
CXXFLAGS = -O2 -march=native -Wall -std=c++17 -I../3rdparty/jsoncpp/dist/
vpath %.cpp ../courseinfo_scraper

.PHONY: all
all: CourseOfferingsScraper GenerateHtml

CourseOfferingsScraper: CourseOfferingsScraper.cpp json.o

GenerateHtml: GenerateHtml.cpp json.o

json.o: ../3rdparty/jsoncpp/dist/jsoncpp.cpp
	$(CXX) -c $(CXXFLAGS) $? -o $@

../3rdparty/jsoncpp/dist/jsoncpp.cpp:
	git submodule update --init
	cd ../3rdparty/jsoncpp; ./amalgamate.py

%.o: %.cpp %.hpp
	$(CXX) $(CXXFLAGS) $? -o $@
