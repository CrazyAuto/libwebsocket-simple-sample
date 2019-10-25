all: server client

server: server.c
	gcc -g -Wall $< -o $@ `pkg-config libwebsockets --libs --cflags` -lpthread
client: client.c
	gcc -g -Wall $< -o $@ `pkg-config libwebsockets --libs --cflags` -lpthread
	#gcc -g -Wall $< -o $@ -lwebsockets -lpthread 
	#$(CC) -g -Wall $< -o $@ -lwebsockets -lpthread -I/mnt/work/yanzhiyao/broadmobi_sdk/broadmobi_sdk_20190718/local_projects/libwebsockets/include

clean:
	rm -f server
	rm -rf server.dSYM
	rm -f client
	rm -rf client.dSYM
