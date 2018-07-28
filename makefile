comp:
	gcc -std=gnu99 -Wall lux_client.c `pkg-config fuse --cflags --libs` -o lux_client

client:
	./lux_client ./config

unmount:
	fusermount -u ./mountpoint