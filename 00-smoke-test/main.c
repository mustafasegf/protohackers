#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>

void *handle_client(void *socket_desc) {
    int sock = *(int*)socket_desc;
    char buf[1024];
    int read_size;

    printf("new client!\n");

    // Read client data
    while ((read_size = recv(sock, buf, 1024, 0)) > 0) {
        printf("read %d bytes\n", read_size);
        write(sock, buf, read_size); // Echo back the data
    }

    if (read_size == 0) {
        printf("client disconnected\n");
    } else if (read_size == -1) {
        perror("recv failed");
    }

    close(sock);
    free(socket_desc);

    return 0;
}

int main() {
    int socket_desc, client_sock, c;
    struct sockaddr_in server, client;
    pthread_t thread_id;

    printf("Hello, world!\n");

    // Create socket
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_desc == -1) {
        perror("Could not create socket");
        return 1;
    }

    // Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(9999);

    // Bind
    if (bind(socket_desc, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("bind failed");
        return 1;
    }

    // Listen
    listen(socket_desc, 3);

    // Accept an incoming connection
    printf("Waiting for incoming connections...\n");
    c = sizeof(struct sockaddr_in);
    while ((client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c))) {
        printf("Connection accepted\n");

        int *new_sock = malloc(sizeof(int));
        *new_sock = client_sock;

        if (pthread_create(&thread_id, NULL, handle_client, (void*) new_sock) < 0) {
            perror("could not create thread");
            return 1;
        }
    }

    if (client_sock < 0) {
        perror("accept failed");
        return 1;
    }

    return 0;
}

