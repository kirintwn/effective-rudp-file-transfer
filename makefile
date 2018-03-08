all: recver1.cpp recver2.cpp recver3.cpp sender1.cpp sender2.cpp sender3.cpp
	g++ recver.cpp -o receiver -std=c++11
	g++ sender.cpp -o sender -std=c++11

clean:
	rm -f receiver
	rm -f sender
	rm -f *.h.gch
