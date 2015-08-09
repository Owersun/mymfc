CC=g++
HEADERS = system.h main.h parser.h xbmcstubs.h Log.h LinuxC1Codec.h DVDVideoCodecC1.h
OBJ = main.o DVDVideoCodecC1.o LinuxC1Codec.o parser.o Log.o
CPPFLAGS= -g

%.o: %.cpp $(HEADERS)
	$(CC) $(CPPFLAGS) -o $@ -c $<

mymfc: $(OBJ)
	$(CC) $(CPPFLAGS) -o $@ $^ -L/usr/lib/aml_libs -lamcodec -lamadec -lasound -lamavutils

clean:
	-rm -f $(OBJ)
	-rm -f mymfc
