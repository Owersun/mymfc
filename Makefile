CC=g++
HEADERS = common.h main.h parser.h LinuxV4l2Sink.h
OBJ = main_new.o parser.o LinuxV4l2Sink.o

%.o: %.cpp $(HEADERS)
	$(CC) -o $@ -c $< -I/usr/local/include -I/usr/local/include/libdrm $(CFLAGS) -g

mymfc: $(OBJ)
	$(CC) -o $@ $^ -L/usr/local/lib -ldrm -lrt -g $(CFLAGS)

clean:
	-rm -f $(OBJ)
	-rm -f mymfc
