server: server.cpp
	g++ -pthread server.cpp -o server.out

clean:
	rm *.out *.o
