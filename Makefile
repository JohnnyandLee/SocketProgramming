all: serverM serverA serverB client

serverM:
	g++ -o serverM -g serverM.cpp

serverA:
	g++ -o serverA -g serverA.cpp

serverB:
	g++ -o serverB -g serverB.cpp

client:
	g++ -o client -g client.cpp

clean:
	rm -f *.o serverM serverA serverB client

