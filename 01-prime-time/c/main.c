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

void *handler2(void *socket_desc) {
  printf("Handler started\n");
  int fd = *(int *)socket_desc;

  char inbuf[1024 * 1024];
  size_t inbuf_used = 0;

  size_t inbuf_remain = sizeof(inbuf) - inbuf_used;
  if (inbuf_remain == 0) {
    perror("Buffer full");
    return 0;
  }

  ssize_t rv = recv(fd, (void *)&inbuf[inbuf_used], inbuf_remain, 0);
  if (rv == 0) {
    perror("Connection closed");
    return 0;
  }

  // if (rv < 0 && errno == EAGAIN) {
  //   /* no data for now, call back when the socket is readable */
  //   printf("No data for now\n");
  //   return 0;
  // }
  
  if (rv < 0) {
    perror("Connection error");
  }
  inbuf_used += rv;

  printf("inbuf: %s\n", inbuf);

  /* Scan for newlines in the line buffer; we're careful here to deal with
   * embedded \0s an evil server may send, as well as only processing lines that
   * are complete.
   */
  char *line_start = inbuf;
  char *line_end;
  while ((line_end = (char *)memchr((void *)line_start, '\n',
                                    inbuf_used - (line_start - inbuf)))) {
    *line_end = 0;
    JSON_Value *root_value = create_response(line_start);
    if (root_value == NULL) {
      printf("Failed to create response\n");
      write(fd, "Error\n", 6);
      continue;
    }
    char *serialized_string = json_serialize_to_string(root_value);
    write(fd, serialized_string, strlen(serialized_string));
    write(fd, "\n", 1);

    printf("Response: %s\n", serialized_string);

    line_start = line_end + 1;
  }
  /* Shift buffer down so the unprocessed data is at the start */
  inbuf_used -= (line_start - inbuf);
  memmove(inbuf, line_start, inbuf_used);

  return 0;
}

void *handler(void *socket_desc) {
  puts("Handler started");

  int sock = *(int *)socket_desc;

  char buf[1024 * 1024] = {0};

  int read_size = 0;
  size_t bufsize = 1024 * 1024;

  char *start = buf;
  char *p = buf;
  char c;
  size_t i = 0;
  size_t offset = 0;
  // size_t offset = 0;

  // while ((read_size = recv(sock, buf + offset, bufsize - offset, 0)) > 0) {
  //   while (i < read_size) {
  //     for (*p != '\n', i = 0; i < bufsize; ++p, ++i)
  //       ;
  //     c = *p;
  //
  //     // check sampe akhir ketemu newline apa engga
  //     if (c != '\n') {
  //       offset = i;
  //       continue;
  //       // recv(sock, buf + i, bufsize - i, 0);
  //     }
  //
  //     if (c == '\n') {
  //       *p = '\0';
  //     }
  //
  //     JSON_Value *root_value = create_response(buf);
  //     if (root_value == NULL) {
  //       printf("Failed to create response\n");
  //       write(sock, "Error\n", 6);
  //       continue;
  //     }
  //
  //     char *serialized_string = json_serialize_to_string(root_value);
  //
  //     write(sock, serialized_string, strlen(serialized_string));
  //     write(sock, "\n", 1);
  //   }
  // }

  // change logic to read until newline
  while ((read_size = recv(sock, buf, bufsize, 0)) > 0) {

    printf("buf %s", buf);
    printf("read_size %d\n", read_size);

    // before we parse, we need to split by new line
    char *new_line;
    size_t old_offset = 0;

    while ((new_line = strchr(buf + old_offset, '\n')) != NULL) {
      size_t line_length = new_line - (buf + old_offset);
      printf("line_length %zu\n", line_length);

      if (line_length == 0) {
        printf("empty line\n");
        old_offset++;
        continue;
      }

      buf[line_length + old_offset] = '\0';

      JSON_Value *root_value = create_response(buf + old_offset);
      old_offset = new_line - buf + 1;
      printf("old_offset %zu\n", old_offset);
      if (root_value == NULL) {
        printf("Failed to create response\n");
        write(sock, "Error\n", 6);
        continue;
      }

      char *serialized_string = json_serialize_to_string(root_value);

      write(sock, serialized_string, strlen(serialized_string));
      write(sock, "\n", 1);

      // json_value_free(root_value);
      // memset(temp_buf, 0, 1024 * 1024);
    }

    printf("old_offset %zu\n", old_offset);
    printf("read_size %d\n", read_size);

    // if (old_offset != read_size) {
    //   printf("there's no new line\n");
    //   write(sock, "Error\n", 6);
    // }
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

    if (pthread_create(&thread_id, NULL, handler2, (void *)new_sock) < 0) {
      perror("Could not create thread");
      exit(EXIT_FAILURE);
    }
  }
}
