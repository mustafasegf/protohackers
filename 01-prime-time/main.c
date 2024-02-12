#include "parson.h"
#include <arpa/inet.h>
#include <math.h>
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

  JSON_Value *root_value;
  JSON_Object *root_object;

  // need to read until new line. If we read max 1024, read more to new buffer
  while ((read_size = recv(sock, buf, 1024, 0)) > 0) {
    printf("read %s\n", buf);

    root_value = json_parse_string(buf);
    if (root_value == NULL) {
      printf("Failed to parse JSON\n");
      write(sock, "Error", 5);
      close(sock);
      free(socket_desc);
      return 0;
    }

    root_object = json_value_get_object(root_value);
    if (root_object == NULL) {
      printf("Failed to parse JSON\n");
      write(sock, "Error", 5);
      close(sock);
      free(socket_desc);
      return 0;
    }

    const char *method = json_object_get_string(root_object, "method");

    if (method == NULL) {
      printf("Failed to parse JSON\n");
      write(sock, "Error", 5);
      close(sock);
      free(socket_desc);
      return 0;
    }

    if (strncmp(method, "isPrime", 7) != 0) {
      printf("Method not supported\n");
      write(sock, "Error", 5);
      close(sock);
      free(socket_desc);
      return 0;
    }

    // get number in JSON_VALUE and check if it's number
    JSON_Value *number_value = json_object_get_value(root_object, "number");
    if (json_value_get_type(number_value) != JSONNumber) {
      printf("Failed to parse JSON\n");
      write(sock, "Error", 5);
      close(sock);
      free(socket_desc);
      return 0;
    }

    int number = json_value_get_number(number_value);
    printf("Number: %d\n", number);

    // check if prime
    int is_prime = 1;
    if (number < 2) {
      is_prime = 0;
    }

    for (int i = 2; i < number; i++) {
      if (number % i == 0) {
        is_prime = 0;
        break;
      }
    }

    root_value = json_value_init_object();
    root_object = json_value_get_object(root_value);

    json_object_set_boolean(root_object, "prime", is_prime);
    json_object_set_string(root_object, "method", "isPrime");

    char *serialized_string = json_serialize_to_string(root_value);
    printf("Response: %s\n", serialized_string);

    // write(sock, serialized_string, strlen(serialized_string));
    send(sock, serialized_string, strlen(serialized_string), 0);
    send(sock, "\n", 1, 0);
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

  int true = 1;
  setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, &true, sizeof(int));

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
