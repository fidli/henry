build/platformReal.o: sources/platform_api.cpp
	g++ -c -O0 -I baselib sources/platform_api.cpp -o build/platformReal.o


build/platformReal.so: build/platformReal.o
	g++ -ldl build/platformReal.o -shared -o build/platformReal.so