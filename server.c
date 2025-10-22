#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <conio.h>

#pragma comment(lib, "Ws2_32.lib")

#define UDP_PORT 5005
#define BUF_SIZE 4096
#define AUTO_TIMEOUT 30  // seconds (to send auto response after 30s)
#define MAX_FILE_SIZE 65000  // single UDP packet limit

// Function to read file content
char* read_file(const char* filepath, long* out_size) {
    FILE* f = fopen(filepath, "rb"); // open file in binary mode
    if (!f) { // file not found
        printf("Error: File '%s' not found\n", filepath);
        return NULL;
    }

    fseek(f, 0, SEEK_END); // jump to end
    long size = ftell(f); // get position = size
    rewind(f); // jump back to start

    if (size > MAX_FILE_SIZE) { // reject if file size exceeds limit
        printf("Error: File too large (max %d bytes)\n", MAX_FILE_SIZE);
        fclose(f);
        return NULL;
    }

    char* buffer = (char*)malloc(size); // allocate memory buffer
    if (!buffer) {
        printf("Error: Memory allocation failed\n");
        fclose(f);
        return NULL;
    }

    size_t read_size = fread(buffer, 1, size, f); // read file into buffer
    fclose(f);

    if (read_size != size) {
        printf("Error: Could not read entire file\n");
        free(buffer);
        return NULL;
    }

    *out_size = size;
    return buffer;
}

// Function to send file to pico (1 = success, 0 = fail)
int send_file(SOCKET sock, struct sockaddr_in* client, const char* filepath) {

    long size = 0;
    char* content = read_file(filepath, &size); // read file
    if (!content) return 0;

    const char* filename = strrchr(filepath, '\\'); // extract filename
    if (!filename) {
        filename = strrchr(filepath, '/');
        if (!filename) filename = filepath; 
        else filename++;
    } else {
        filename++;
    }

    // Protocol: "FILE:" + filename_length(4 bytes) + filename + content
    int filename_len = strlen(filename);
    int header_size = 5 + 4 + filename_len; // "FILE:" + length + filename
    int total_len = header_size + size;

    char* message = (char*)malloc(total_len); // allocate message buffer
    if (!message) {
        printf("Error: Memory allocation failed\n");
        free(content);
        return 0;
    }

    // Build message: FILE: + 4-byte length + filename + content
    memcpy(message, "FILE:", 5); // header
    memcpy(message + 5, &filename_len, 4); // filename length
    memcpy(message + 9, filename, filename_len); // filename
    memcpy(message + 9 + filename_len, content, size); // file content

    printf("\nSending file '%s'\n", filename);
    printf("File size: %ld bytes\n", size);

    // send file via udp
    int sent = sendto(sock, message, total_len, 0,
                      (struct sockaddr*)client, sizeof(*client));
                      
    // free allocated memory
    free(content);
    free(message);

    if (sent == SOCKET_ERROR) {
        printf("Error sending file: %d\n", WSAGetLastError());
        return 0;
    }

    // Wait for confirmation with timeout
    fd_set readfds;
    struct timeval tv;
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);
    tv.tv_sec = 5;
    tv.tv_usec = 0;

    int select_result = select(0, &readfds, NULL, NULL, &tv);
    if (select_result > 0) {
        char buf[BUF_SIZE];
        struct sockaddr_in from;
        int fromlen = sizeof(from);
        int recv_len = recvfrom(sock, buf, BUF_SIZE, 0, (struct sockaddr*)&from, &fromlen);
        if (recv_len > 0) {
            buf[recv_len] = '\0';
            if (strcmp(buf, "FILE_RECEIVED_OK") == 0) {
                printf("File sent and received successfully!\n");
                printf("--------------------------------------------------\n");
                return 1;
            } else {
                printf("Pico reported error: %s\n", buf);
                return 0;
            }
        }
    }
    
    printf("[WARNING] No confirmation from Pico\n");
    return 1; // assume success if no response
}

int main() {
    WSADATA wsa;
    SOCKET sock;
    struct sockaddr_in server, client;
    int client_len = sizeof(client);

    char buf[BUF_SIZE];

    // Init Winsock
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        printf("WSAStartup failed\n");
        return 1;
    }

    // Create UDP socket
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) {
        printf("Socket creation failed\n");
        WSACleanup();
        return 1;
    }

    // Set socket to non-blocking for select() to work properly
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);

    // Bind
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY; // listen on all interfaces
    server.sin_port = htons(UDP_PORT); // port 5005

    if (bind(sock, (struct sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
        printf("Bind failed\n");
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    printf("UDP Server listening on port %d\n", UDP_PORT);
    printf("File size limit: %d bytes\n", MAX_FILE_SIZE);

    while (1) { // main communication loop
        printf("\n\033[1;32mWaiting for next message from Pico...\033[0m\n");
        
        // Wait for incoming message using select, with 3 second timeout
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        
        struct timeval tv;
        tv.tv_sec = 3;
        tv.tv_usec = 0;
        
        int select_result = select(0, &readfds, NULL, NULL, &tv); // 3-seccond timeout
        if (select_result <= 0) {
            continue; // No data, loop back
        }

        int recv_len = recvfrom(sock, buf, BUF_SIZE - 1, 0,
                                (struct sockaddr*)&client, &client_len); // receive udp packet
        if (recv_len == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err != WSAEWOULDBLOCK) {
                printf("recvfrom() failed: %d\n", err);
            }
            continue;
        }

        // display received message
        buf[recv_len] = '\0';
        printf("\n==================================================\n");
        printf("Received from Pico (%s:%d)\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));
        printf("Message: %s\n", buf);
        printf("==================================================\n");

        // Prompt user for response with timeout
        time_t start = time(NULL);
        char input[BUF_SIZE] = {0};
        int input_ready = 0;

        printf("\nYou have %d seconds to send a response...\n", AUTO_TIMEOUT);
        printf("Type message OR 'file:<filepath>' to send a file\nMessage: ");
        fflush(stdout);

        // get user input with timeout
        while (difftime(time(NULL), start) < AUTO_TIMEOUT) {
            if (_kbhit()) { // check if key pressed
                fgets(input, sizeof(input), stdin);
                input[strcspn(input, "\n")] = '\0'; // remove newline
                input_ready = 1;
                break;
            }
            Sleep(100); // check every 100ms
        }

        // Handle empty input - reprompt
        while (input_ready && strlen(input) == 0) {
            printf("Empty input detected. Please enter a message or 'file:<filepath>'\nMessage: ");
            fflush(stdout);
            fgets(input, sizeof(input), stdin);
            input[strcspn(input, "\n")] = '\0';
        }

        // Handle file transfer
        if (input_ready && strlen(input) > 0) {
            if (strncmp(input, "file:", 5) == 0) {
                int success = 0;
                char filepath[BUF_SIZE];
                strcpy(filepath, input + 5);

                while (!success) {
                    success = send_file(sock, &client, filepath);
                    if (!success) {
                        printf("\nRetry? Type message or 'file:<filepath>' to send a file\nMessage: ");
                        fflush(stdout);
                        fgets(input, sizeof(input), stdin);
                        input[strcspn(input, "\n")] = '\0';
                        if (strncmp(input, "file:", 5) == 0) {
                            strcpy(filepath, input + 5);
                        } else if (strlen(input) > 0) {
                            // User switched to text
                            sendto(sock, input, strlen(input), 0,
                                (struct sockaddr*)&client, client_len);
                            printf("Sent to Pico: %s\n", input);
                            printf("--------------------------------------------------\n");
                            break;
                        } else {
                            break; // Empty input, exit retry loop
                        }
                    }
                }
            } else {
                int sent = sendto(sock, input, strlen(input), 0,
                                (struct sockaddr*)&client, client_len);
                if (sent == SOCKET_ERROR) {
                    printf("Error sending response: %d\n", WSAGetLastError());
                } else {
                    printf("Sent to Pico: %s\n", input);
                    printf("--------------------------------------------------\n");
                }
            }
        } else if (!input_ready) {
            // Only send auto-message if timeout occurred
            const char* auto_msg = "Sorry for the wait, this is an autogenerated message.";
            sendto(sock, auto_msg, strlen(auto_msg), 0,
                   (struct sockaddr*)&client, client_len);
            printf("\n[SYSTEM] Auto-response sent (timeout)\n");
            printf("Message: %s\n", auto_msg);
            printf("--------------------------------------------------\n");
        }
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}