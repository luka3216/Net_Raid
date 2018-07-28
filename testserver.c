#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>

int main(int argc, char** argv) {
  int server_socket = socket(AF_INET, SOCK_STREAM, 0);

  struct sockaddr_in server_adress;

  server_adress.sin_family = AF_INET;
  server_adress.sin_port = htons(10001); 
  server_adress.sin_addr.s_addr = inet_addr("127.0.0.1");

  bind(server_socket,(struct sockaddr *) &server_adress, sizeof(server_adress));

  listen(server_socket, 10);

  int client_socket = accept(server_socket, NULL, NULL);

  char buff[256];

  recv(client_socket, &buff[0], 256, 0);

  printf("%s", &buff[0]);

  close(server_socket);

  

}