CXX = g++
HEADERS = system.h main.h xbmcstubs.h LinuxV4l2Sink.h Log.h BitstreamConverter.h DVDVideoCodecMFC.h
OBJ = main.o LinuxV4l2Sink.o Log.o BitstreamConverter.o DVDVideoCodecMFC.o
CXXFLAGS = -g -Wall
LIBS = -lavformat -lavcodec -lavutil

%.o: %.cpp $(HEADERS)
	$(CXX) -o $@ -c $< $(CXXFLAGS)

mymfc: $(OBJ)
	$(CXX) -o $@ $^ $(LIBS)

clean:
	-rm -f $(OBJ)
	-rm -f mymfc
