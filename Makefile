all: TestLiveMedia

TestLiveMedia: TestLiveMedia.o
	g++ -o TestLiveMedia TestLiveMedia.o `pkg-config --libs live555`
    
TestLiveMedia.o: TestLiveMedia.cpp
	g++ -c TestLiveMedia.cpp `pkg-config --cflags live555`
