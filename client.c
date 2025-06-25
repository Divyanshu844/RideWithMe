#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib")

#define PORT 8080
#define SERVER_ADDR "127.0.0.1"
#define BUFFER_SIZE 2048

void send_request(SOCKET sock, const char *request) {
    send(sock, request, strlen(request), 0);
}

void receive_response(SOCKET sock) {
    char buffer[BUFFER_SIZE];
    int bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        printf("\nSERVER RESPONSE:\n%s\n", buffer);
    }
}

void menu(SOCKET sock, char *username) {
    int choice;
    char buffer[BUFFER_SIZE];

    while (1) {
        printf("\n--- RideWithMe Carpooling Menu ---\n");
        printf("1. Create Ride\n");
        printf("2. Search Ride\n");
        printf("3. Book Seat\n");
        printf("4. Delete My Ride\n");
        printf("5. View My Ride History\n");
        printf("6. View Number of Rides Booked\n"); // New option added
        printf("7. Exit\n"); // Exit option moved
        printf("Enter choice: ");
        scanf("%d", &choice);
        getchar();

        switch (choice) {
            case 1: {
                char ride_id[20], origin[50], destination[50], vehicle[50];
                int seats;
                float distance, time;

                printf("Enter Ride ID: ");
                gets(ride_id);
                printf("Origin: ");
                gets(origin);
                printf("Destination: ");
                gets(destination);
                printf("Seats: ");
                scanf("%d", &seats); getchar();
                printf("Distance (km): ");
                scanf("%f", &distance); getchar();
                printf("Time (mins): ");
                scanf("%f", &time); getchar();
                printf("Vehicle Type: ");
                gets(vehicle);

                sprintf(buffer, "CREATE_RIDE|%s|%s|%s|%s|%d|%.2f|%.2f|%s",
                        ride_id, username, origin, destination, seats, distance, time, vehicle);
                send_request(sock, buffer);
                receive_response(sock);
                break;
            }
            case 2: {
                char keyword[50];
                printf("Enter keyword (origin, destination, or ride ID): ");
                gets(keyword);
                sprintf(buffer, "SEARCH|%s", keyword);
                send_request(sock, buffer);
                receive_response(sock);
                break;
            }
            case 3: {
                char ride_id[20];
                int seats, total_seats;

                printf("Enter Ride ID to book: ");
                fgets(ride_id, sizeof(ride_id), stdin);
                ride_id[strcspn(ride_id, "\n")] = 0;


                sprintf(buffer, "GET_TOTAL_SEATS|%s", ride_id);
                send_request(sock, buffer);
                // Receive the total seats response here
                char total_seats_response[BUFFER_SIZE];
                int bytes_received = recv(sock, total_seats_response, sizeof(total_seats_response) - 1, 0);
                if (bytes_received > 0) {
                    total_seats_response[bytes_received] = '\0';
                    sscanf(total_seats_response, "TOTAL_SEATS|%d", &total_seats);
                    printf("Total seats available: %d\n", total_seats);
                } else {
                    printf("Could not get total seats for ride.\n");
                    break;
                }

                printf("Enter number of seats to book: ");
                scanf("%d", &seats);
                getchar();

                if (seats <= 0 || seats > total_seats) {
                    printf("Invalid number of seats.\n");
                } else {
                    sprintf(buffer, "BOOK|%s|%s|%d", ride_id, username, seats);
                    send_request(sock, buffer);
                    receive_response(sock);

                }
                break;
            }
            case 4: {
                char ride_id[20];
                printf("Enter Ride ID to delete: ");
                gets(ride_id);
                sprintf(buffer, "DELETE|%s|%s", ride_id, username);
                send_request(sock, buffer);
                receive_response(sock);
                break;
            }
            case 5:
                sprintf(buffer, "HISTORY|%s", username);
                send_request(sock, buffer);
                receive_response(sock);
                break;
            case 6: {
                sprintf(buffer, "GET_BOOKED_RIDES_COUNT|%s", username);
                send_request(sock, buffer);
                receive_response(sock);
                break;
            }
            case 7: // Exit option
                closesocket(sock);
                WSACleanup();
                printf("Disconnected.\n");
                return;
            default:
                printf("Invalid choice.\n");
        }
    }
}

int main() {
    WSADATA wsa;
    SOCKET sock;
    struct sockaddr_in server;

    WSAStartup(MAKEWORD(2, 2), &wsa);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    server.sin_addr.s_addr = inet_addr(SERVER_ADDR);
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        printf("Connection failed.\n");
        return 1;
    }

    printf("Connected to server at %s:%d\n", SERVER_ADDR, PORT);

    char username[50], password[50], buffer[BUFFER_SIZE];
    int option;

    while (1) {
        printf("\n1. Register\n2. Login\nChoose option: ");
        scanf("%d", &option); getchar();

        printf("Username: ");
        gets(username);
        printf("Password: ");
        gets(password);

        if (option == 1)
            sprintf(buffer, "REGISTER|%s|%s", username, password);
        else
            sprintf(buffer, "LOGIN|%s|%s", username, password);

        send_request(sock, buffer);
        recv(sock, buffer, BUFFER_SIZE - 1, 0);
        buffer[BUFFER_SIZE - 1] = '\0';

        if (strstr(buffer, "SUCCESS")) {
            printf("%s", buffer);
            break;
        } else {
            printf("ERROR: %s", buffer);
        }
    }

    menu(sock, username);
    return 0;
}
