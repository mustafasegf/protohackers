#include "parson.h"
#include <arpa/inet.h>
#include <errno.h>
#include <math.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

long is_prime(long number) {
  if (number < 2) {
    return 0;
  }

  for (long i = 2; i <= sqrt(number); i++) {
    if (number % i == 0) {
      return 0;
    }
  }

  return 1;
}

JSON_Value *create_response(char *input) {
  printf("Input: %s\n", input);
  JSON_Value *root_value;
  JSON_Object *root_object;

  root_value = json_parse_string(input);
  if (root_value == NULL) {
    printf("Failed to parse string to JSON\n");
    return NULL;
  }

  root_object = json_value_get_object(root_value);
  if (root_object == NULL) {
    printf("Failed to get object JSON\n");
    return NULL;
  }

  const char *method = json_object_get_string(root_object, "method");

  if (method == NULL) {
    printf("Failed to get method\n");
    return NULL;
  }

  if (strncmp(method, "isPrime", 7) != 0) {
    printf("Method not supported\n");
    return NULL;
  }

  // get number in JSON_VALUE and check if it's number
  JSON_Value *number_value = json_object_get_value(root_object, "number");
  if (number_value == NULL) {
    printf("Failed to get number\n");
    return NULL;
  }

  if (json_value_get_type(number_value) != JSONNumber) {
    printf("number type is not number\n");
    return NULL;
  }

  long long number = json_value_get_number(number_value);
  printf("Number: %lld\n", number);

  // check if prime
  root_value = json_value_init_object();
  root_object = json_value_get_object(root_value);

  json_object_set_boolean(root_object, "prime", is_prime(number));
  json_object_set_string(root_object, "method", "isPrime");

  char *serialized_string = json_serialize_to_string(root_value);
  printf("Response: %s\n", serialized_string);

  return root_value;
}

ssize_t read_line_from_socket(int sock, char *buf, size_t bufsize) {
  if (bufsize == 0)
    return -1;

  size_t bytesRead = 0;
  ssize_t result;
  char c;

  while (bytesRead < bufsize - 1) {
    result = recv(sock, &c, 1, 0);
    if (result < 0) {
      return -1;
    } else if (result == 0) {
      break;
    }

    buf[bytesRead++] = c;
    if (c == '\n') {
      break;
    }
  }

  buf[bytesRead] = '\0';
  return bytesRead;
}

void *handler(void *socket_desc) {
  puts("Handler started");

  int sock = *(int *)socket_desc;

  char buf[1024 * 1024] = {0};

  size_t read_size = 0;
  size_t bufsize = 1024 * 1024;

  while ((read_size = read_line_from_socket(sock, buf, bufsize)) > 0) {
    printf("buf %s", buf);
    printf("read_size %zu\n", read_size);

    JSON_Value *root_value = create_response(buf);
    if (root_value == NULL) {
      printf("Failed to create response\n");
      write(sock, "Error\n", 6);
      continue;
    }

    char *serialized_string = json_serialize_to_string(root_value);

    write(sock, serialized_string, strlen(serialized_string));
    write(sock, "\n", 1);

    // copy the rest of the buffer to the beginning
    memmove(buf, buf + read_size, bufsize - read_size);
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
