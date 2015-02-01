CC=g++
HEADERS = system.h main.h parser.h xbmcstubs.h LinuxV4l2Sink.h Log.h DVDVideoCodecMFC.h
OBJ = main.o parser.o LinuxV4l2Sink.o Log.o DVDVideoCodecMFC.o
CPPFLAGS= -g

%.o: %.cpp $(HEADERS)
	$(CC) -o $@ -c $< -I/usr/local/include -I/usr/local/include/libdrm $(CPPFLAGS) -g

mymfc: $(OBJ)
	$(CC) -g -o $@ $^ -L/usr/local/lib -ldrm -lrt $(CPPFLAGS)

clean:
	-rm -f $(OBJ)
	-rm -f mymfc
