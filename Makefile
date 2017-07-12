all:
	g++ -O3 -lcurl main.cpp -Wall -o downloader
clean:
	rm -f downloader