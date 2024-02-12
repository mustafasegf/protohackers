#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

void *handler(void *socket_desc) {
  puts("Handler started");

  int sock = *(int *)socket_desc;
  char buf[1024];
  int read_size = 0;

  while ((read_size = recv(sock, buf, 1024, 0)) > 0) {
    printf("read %d bytes\n", read_size);
    write(sock, buf, read_size); // Echo back the data
  }

  if (read_size < 0) {
    perror("Receive failed");
  } else {
    puts("Client disconnected");
  }

  close(sock);
  free(socket_desc);

  return 0;
}

int main() {
  int socket_desc = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_desc == -1) {
    perror("Could not create socket");
    exit(EXIT_FAILURE);
  }

  struct sockaddr_in server;
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port = htons(9999);

  if (bind(socket_desc, (struct sockaddr *)&server, sizeof(server)) < 0) {
    perror("Bind failed");
    exit(EXIT_FAILURE);
  }

  if (listen(socket_desc, 5) < 0) {
    perror("Listen failed");
    exit(EXIT_FAILURE);
  }

  struct sockaddr_in client;
  int client_sock = 0;

  pthread_t thread_id;

  puts("Starting server");

  int len = sizeof(struct sockaddr_in);

  int true = 1;

  setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, &true, sizeof(int));

  while ((client_sock = accept(socket_desc, (struct sockaddr *)&client,
                               (socklen_t *)&len))) {

    printf("Connection accepted\n");
    printf("Client address: %s\n", inet_ntoa(client.sin_addr));
    printf("Client port: %d\n", ntohs(client.sin_port));

    int *new_sock = malloc(sizeof(int));
    *new_sock = client_sock;

    if (pthread_create(&thread_id, NULL, handler, (void *)new_sock) < 0) {
      perror("Could not create thread");
      exit(EXIT_FAILURE);
    }
  }
}
