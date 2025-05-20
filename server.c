// server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib")

#define PORT 8080
#define BUFFER_SIZE 2048

typedef struct {
    char ride_id[10];
    char driver[50];
    char origin[50];
    char destination[50];
    int seats_total;
    int seats_available;
    float distance_km;
    float time_minutes;
    char vehicle[50];
} Ride;

void init_winsock() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);
}

int register_user(const char *username, const char *password) {
    FILE *file = fopen("users.txt", "a+");
    if (!file) return 0;

    char line[100], user[50], pass[50];
    while (fgets(line, sizeof(line), file)) {
        sscanf(line, "%s %s", user, pass);
        if (strcmp(user, username) == 0) {
            fclose(file);
            return 0; // User exists
        }
    }

    fprintf(file, "%s %s\n", username, password);
    fclose(file);
    return 1;
}

int login_user(const char *username, const char *password) {
    FILE *file = fopen("users.txt", "r");
    if (!file) return 0;

    char line[100], user[50], pass[50];
    while (fgets(line, sizeof(line), file)) {
        sscanf(line, "%s %s", user, pass);
        if (strcmp(user, username) == 0 && strcmp(pass, password) == 0) {
            fclose(file);
            return 1;
        }
    }

    fclose(file);
    return 0;
}

void save_ride(Ride *ride) {
    FILE *file = fopen("rides.txt", "a");
    if (!file) return;

    fprintf(file, "%s %s %s %s %d %d %.2f %.2f %s\n",
            ride->ride_id, ride->driver, ride->origin, ride->destination,
            ride->seats_total, ride->seats_available,
            ride->distance_km, ride->time_minutes, ride->vehicle);
    fclose(file);
}

int book_seat(const char *ride_id, const char *username) {
    FILE *file = fopen("rides.txt", "r");
    FILE *temp = fopen("temp.txt", "w");
    FILE *log = fopen("history.txt", "a");

    if (!file || !temp || !log) return 0;

    Ride ride;
    int found = 0;

    while (fscanf(file, "%s %s %s %s %d %d %f %f %s",
                  ride.ride_id, ride.driver, ride.origin, ride.destination,
                  &ride.seats_total, &ride.seats_available, &ride.distance_km,
                  &ride.time_minutes, ride.vehicle) == 9) {

        if (strcmp(ride.ride_id, ride_id) == 0 && ride.seats_available > 0) {
            ride.seats_available--;
            found = 1;

            // Log the booking
            fprintf(log, "%s %s %.2f %.2f %s\n", username,
                    ride.ride_id, ride.distance_km, ride.time_minutes, ride.vehicle);
        }

        fprintf(temp, "%s %s %s %s %d %d %.2f %.2f %s\n",
                ride.ride_id, ride.driver, ride.origin, ride.destination,
                ride.seats_total, ride.seats_available,
                ride.distance_km, ride.time_minutes, ride.vehicle);
    }

    fclose(file);
    fclose(temp);
    fclose(log);

    remove("rides.txt");
    rename("temp.txt", "rides.txt");

    return found;
}

void get_user_history(const char *username, SOCKET sock) {
    FILE *file = fopen("history.txt", "r");
    if (!file) {
        send(sock, "ERROR|No history found\n", strlen("ERROR|No history found\n"), 0);
        return;
    }

    char line[256], ride_id[10], user[50], vehicle[50];
    float dist, time;
    float total_dist = 0.0, total_time = 0.0;
    char vehicles[500] = "";
    int found = 0;

    while (fgets(line, sizeof(line), file)) {
        sscanf(line, "%s %s %f %f %s", user, ride_id, &dist, &time, vehicle);
        if (strcmp(user, username) == 0) {
            found = 1;
            total_dist += dist;
            total_time += time;

            strcat(vehicles, vehicle);
            strcat(vehicles, ", ");
        }
    }

    fclose(file);

    if (found) {
        char response[1024];
        sprintf(response, "HISTORY|Total Distance: %.2f km\nTotal Time: %.2f mins\nVehicles: %s\n",
                total_dist, total_time, vehicles);
        send(sock, response, strlen(response), 0);
    } else {
        send(sock, "ERROR|No rides booked yet\n", strlen("ERROR|No rides booked yet\n"), 0);
    }
}

void search_rides(const char *keyword, SOCKET sock) {
    FILE *file = fopen("rides.txt", "r");
    if (!file) {
        send(sock, "ERROR|No rides found\n", strlen("ERROR|No rides found\n"), 0);
        return;
    }

    Ride ride;
    char response[2048] = "RIDES|";
    int found = 0;

    while (fscanf(file, "%s %s %s %s %d %d %f %f %s",
                  ride.ride_id, ride.driver, ride.origin, ride.destination,
                  &ride.seats_total, &ride.seats_available,
                  &ride.distance_km, &ride.time_minutes, ride.vehicle) == 9) {

        if (strstr(ride.origin, keyword) || strstr(ride.destination, keyword) || strstr(ride.ride_id, keyword)) {
            char line[256];
            sprintf(line, "[%s] %s -> %s | Seats: %d/%d | %.2f km, %.2f min | %s\n",
                    ride.ride_id, ride.origin, ride.destination,
                    ride.seats_available, ride.seats_total,
                    ride.distance_km, ride.time_minutes, ride.vehicle);
            strcat(response, line);
            found = 1;
        }
    }

    fclose(file);

    if (found) {
        send(sock, response, strlen(response), 0);
    } else {
        send(sock, "ERROR|No matching rides\n", strlen("ERROR|No matching rides\n"), 0);
    }
}

int delete_ride(const char *ride_id, const char *username) {
    FILE *file = fopen("rides.txt", "r");
    FILE *temp = fopen("temp.txt", "w");
    if (!file || !temp) return 0;

    Ride ride;
    int deleted = 0;

    while (fscanf(file, "%s %s %s %s %d %d %f %f %s",
                  ride.ride_id, ride.driver, ride.origin, ride.destination,
                  &ride.seats_total, &ride.seats_available,
                  &ride.distance_km, &ride.time_minutes, ride.vehicle) == 9) {
        
        if (strcmp(ride.ride_id, ride_id) == 0 && strcmp(ride.driver, username) == 0) {
            deleted = 1; // Don't write this ride (delete it)
            continue;
        }
        // Keep ride
        fprintf(temp, "%s %s %s %s %d %d %.2f %.2f %s\n",
                ride.ride_id, ride.driver, ride.origin, ride.destination,
                ride.seats_total, ride.seats_available,
                ride.distance_km, ride.time_minutes, ride.vehicle);
    }

    fclose(file);
    fclose(temp);
    remove("rides.txt");
    rename("temp.txt", "rides.txt");

    return deleted;
}

void clientHandler(SOCKET sock) {
    char buffer[BUFFER_SIZE];
    int len;

    while ((len = recv(sock, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[len] = '\0';
        char *cmd = strtok(buffer, "|");

        if (strcmp(cmd, "REGISTER") == 0) {
            char *username = strtok(NULL, "|");
            char *password = strtok(NULL, "|");
            if (register_user(username, password))
                send(sock, "SUCCESS|Registered\n", strlen("SUCCESS|Registered\n"), 0);
            else
                send(sock, "ERROR|Username exists\n", strlen("ERROR|Username exists\n"), 0);
        }
        else if (strcmp(cmd, "LOGIN") == 0) {
            char *username = strtok(NULL, "|");
            char *password = strtok(NULL, "|");
            if (login_user(username, password))
                send(sock, "SUCCESS|Logged in\n", strlen("SUCCESS|Logged in\n"), 0);
            else
                send(sock, "ERROR|Invalid credentials\n", strlen("ERROR|Invalid credentials\n"), 0);
        }
        else if (strcmp(cmd, "CREATE_RIDE") == 0) {
            Ride ride;
            strcpy(ride.ride_id, strtok(NULL, "|"));
            strcpy(ride.driver, strtok(NULL, "|"));
            strcpy(ride.origin, strtok(NULL, "|"));
            strcpy(ride.destination, strtok(NULL, "|"));
            ride.seats_total = atoi(strtok(NULL, "|"));
            ride.seats_available = ride.seats_total;
            ride.distance_km = atof(strtok(NULL, "|"));
            ride.time_minutes = atof(strtok(NULL, "|"));
            strcpy(ride.vehicle, strtok(NULL, "|"));
            save_ride(&ride);
            send(sock, "SUCCESS|Ride created\n", strlen("SUCCESS|Ride created\n"), 0);
        }
        else if (strcmp(cmd, "BOOK") == 0) {
            char *ride_id = strtok(NULL, "|");
            char *username = strtok(NULL, "|");
            if (book_seat(ride_id, username))
                send(sock, "SUCCESS|Seat booked\n", strlen("SUCCESS|Seat booked\n"), 0);
            else
                send(sock, "ERROR|Booking failed\n", strlen("ERROR|Booking failed\n"), 0);
        }
        else if (strcmp(cmd, "SEARCH") == 0) {
            char *keyword = strtok(NULL, "|");
            search_rides(keyword, sock);
        }
        else if (strcmp(cmd, "DELETE") == 0) {
            char *ride_id = strtok(NULL, "|");
            char *username = strtok(NULL, "|");
            if (delete_ride(ride_id, username))
                send(sock, "SUCCESS|Ride deleted\n", strlen("SUCCESS|Ride deleted\n"), 0);
            else
                send(sock, "ERROR|Delete failed\n", strlen("ERROR|Delete failed\n"), 0);
        }
        else if (strcmp(cmd, "HISTORY") == 0) {
            char *username = strtok(NULL, "|");
            get_user_history(username, sock);
        }
        else {
            send(sock, "ERROR|Unknown command\n", strlen("ERROR|Unknown command\n"), 0);
        }
    }

    closesocket(sock);
}

int main() {
    init_winsock();

    SOCKET server_socket, client_socket;
    struct sockaddr_in server, client;
    int client_len = sizeof(client);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET) {
        printf("Socket creation failed\n");
        return 1;
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server, sizeof(server)) == SOCKET_ERROR) {
        printf("Bind failed\n");
        return 1;
    }

    listen(server_socket, 5);
    printf("Server started on port %d...\n", PORT);

    while ((client_socket = accept(server_socket, (struct sockaddr *)&client, &client_len)) != INVALID_SOCKET) {
        printf("Client connected\n");
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)clientHandler, (void *)client_socket, 0, NULL);
    }

    closesocket(server_socket);
    WSACleanup();
    return 0;
}

