#not finished, not tested
CC:=g++
CFLAGS:=-I/usr/local/include -I../dlib -L/usr/local/lib -L../dlib/examples/build/dlib_duild
LIBRARIES:=-lopencv_core -lopencv_imgproc -lopencv_imgcodecs -lopencv_highgui -lopencv_objdetect -lopencv_video -lopencv_videoio -ldlib

all: face

face:
	$(CC) $(CFLAGS) -o face face.cpp imageface.cpp videoface.cpp $(LIBRARIES)

clean:
	rm -f *.o face