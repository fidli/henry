build/platformReal.o: sources/platform_api.cpp baselib build/data
	g++ `pkg-config --cflags gtk+-2.0` -std=c++11 -c -O0 -I baselib sources/platform_api.cpp -o build/platformReal.o


build/platformReal.so: build/platformReal.o
	g++ -ldl  build/platformReal.o -shared -o build/platformReal.so `pkg-config --libs gtk+-2.0`