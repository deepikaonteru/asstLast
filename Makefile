all: clientSide/client.c serverSide/server.c
	gcc -o clientSide/WTF clientSide/client.c -lssl -lcrypto -lz
	gcc -o serverSide/WTFserver serverSide/server.c -lz

test: WTFtest.c
	gcc -o WTFtest WTFtest.c

clean:
	rm -rf clientSide/WTF
	rm -rf serverSide/WTFserver
	rm -rf WTFtest
