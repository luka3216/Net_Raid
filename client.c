#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>

int main(int argc, char** argv) {
  int client_socket = socket(AF_INET, SOCK_STREAM, 0);

  struct sockaddr_in server_adress;

  server_adress.sin_family = AF_INET;
  server_adress.sin_port = htons(10001); 
  server_adress.sin_addr.s_addr = inet_addr("127.0.0.1");
  char* str = strdup("hellooooooooooooooo");
  if (!connect(client_socket,(struct sockaddr *) &server_adress, sizeof(server_adress))) {
    send(client_socket, str, strlen(str) + 1, 0);
    printf("success\n");
  }

}