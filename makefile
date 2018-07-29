comp:
	gcc -std=gnu99 -Wall lux_client.c `pkg-config fuse --cflags --libs` -o lux_client
	gcc -std=gnu99 lux_server.c -o lux_server

client:
	./lux_client ./config

server1:
	./lux_server 127.0.0.1 10001 ./server1_dir

server2:
	./lux_server 127.0.0.1 10002 ./server2_dir

unmount:
	fusermount -u ./mountpoint1