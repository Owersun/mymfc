CC=g++
HEADERS = common.h main_new.h parser.h LinuxV4l2Sink.h
OBJ = main_new.o parser.o LinuxV4l2Sink.o
CPPFLAGS= -g

%.o: %.cpp $(HEADERS)
	$(CC) -o $@ -c $< -I/usr/local/include -I/usr/local/include/libdrm $(CFLAGS) -g

mymfc: $(OBJ)
	$(CC) -g -o $@ $^ -L/usr/local/lib -ldrm -lrt $(CFLAGS)

clean:
	-rm -f $(OBJ)
	-rm -f mymfc
