CC=g++
HEADERS = common.h main_new.h parser.h LinuxV4l2Sink.h Log.h
OBJ = main_new.o parser.o LinuxV4l2Sink.o Log.o
CPPFLAGS= -g

%.o: %.cpp $(HEADERS)
	$(CC) -o $@ -c $< -I/usr/local/include -I/usr/local/include/libdrm $(CPPFLAGS) -g

mymfc: $(OBJ)
	$(CC) -g -o $@ $^ -L/usr/local/lib -ldrm -lrt $(CPPFLAGS)

clean:
	-rm -f $(OBJ)
	-rm -f mymfc
