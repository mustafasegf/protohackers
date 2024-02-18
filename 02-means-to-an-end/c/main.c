#include <arpa/inet.h>
#include <errno.h>
#include <math.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

ssize_t read_bufsize_from_socket(int sock, uint8_t *buf, size_t bufsize) {
  if (bufsize == 0)
    return -1;

  size_t bytesRead = 0;
  ssize_t result;

  while (bytesRead < bufsize) {
    result = recv(sock, buf + bytesRead, bufsize - bytesRead, 0);
    if (result < 0) {
      return -1;
    } else if (result == 0) {
      break;
    }
    bytesRead += result;
  }

  return bytesRead;
}

void *handler(void *socket_desc) {
  puts("Handler started");

  int sock = *(int *)socket_desc;

  uint8_t buf[9] = {0};
  size_t read_size = 0;
  size_t *db = malloc(sizeof(size_t) * 0x7FFFFFFF);

  if (db == NULL) {
    perror("Could not allocate memory");
    exit(EXIT_FAILURE);
  }

  while ((read_size = read_bufsize_from_socket(sock, buf, 9)) > 0) {
    printf("buf: ");
    for (int i = 0; i < 9; i++) {
      printf("%d ", buf[i]);
    }
    printf("\n");

    if (buf[0] == 'I') {
      int32_t time = buf[4] | (buf[3] << 8) | (buf[2] << 16) | (buf[1] << 24);
      int32_t price = buf[8] | (buf[7] << 8) | (buf[6] << 16) | (buf[5] << 24);
      printf("time %d, price %d\n", time, price);

      db[time] = price;
    } else if (buf[0] == 'Q') {
      int32_t mintime =
          buf[4] | (buf[3] << 8) | (buf[2] << 16) | (buf[1] << 24);
      int32_t maxtime =
          buf[8] | (buf[7] << 8) | (buf[6] << 16) | (buf[5] << 24);
      if (mintime < 0) {
        mintime = 0;
      }
      if (maxtime < 0) {
        maxtime = 0;
      }
      uint8_t answer[4] = {0};
      if (mintime > maxtime) {
        write(sock, answer, 4);
      }

      int32_t count = 0;
      for (int32_t i = mintime; i <= maxtime; i++) {
        if (db[i] != 0) {
          count++;
        }
      }

      int64_t sum = 0;
      for (int32_t i = mintime; i <= maxtime; i++) {
        sum += db[i];
      }


      int32_t avg = 0;
      if (count > 0) {
        avg = sum / count;
      }

      printf("count %d, sum %ld, avg %d\n", count, sum, avg);

      answer[0] = avg >> 24;
      answer[1] = avg >> 16;
      answer[2] = avg >> 8;
      answer[3] = avg;
      printf("answer: ");
      for (int i = 0; i < 4; i++) {
        printf("%d ", answer[i]);
      }
      printf("\n");


      write(sock, answer, 4);
    } else {
      perror("Invalid command");
      write(sock, "Invalid command\n", 16);
      break;
    }
  }

  if (read_size < 0) {
    perror("Receive failed");
  } else {
    puts("Client disconnected");
  }

  close(sock);
  free(socket_desc);
  free(db);

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
