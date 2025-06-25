#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib") // Link with ws2_32.lib

#define PORT 8080
#define BUFFER_SIZE 2048

// Structure to hold ride data
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
    printf("Initialising Winsock...\n");
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0)
      {
        printf("Failed. Error Code : %d", WSAGetLastError());
        exit(EXIT_FAILURE);
      }
    printf("Initialised.\n");
}
int register_user(const char *username, const char *password)
{
    FILE *file = fopen("users.txt", "a");
    if (file == NULL) return 0;
    fprintf(file, "%s %s\n", username, password);
    fclose(file);
    return 1;
}

int login_user(const char *username, const char *password)
 {

    FILE *file = fopen("users.txt", "r");
    if (file == NULL) return 0;

    char stored_username[50], stored_password[50];
    int found = 0;
    while (fscanf(file, "%s %s", stored_username, stored_password) == 2)
        {
           if (strcmp(username, stored_username) == 0 && strcmp(password, stored_password) == 0)
           {
             found = 1;
              break;
            }
        }
    fclose(file);
    return found; // 1 for success, 0 for failure
}

void save_ride(Ride *ride)
 {

    FILE *file = fopen("rides.txt", "a");
    if (file == NULL)
        {
          printf("Error opening rides.txt for saving.\n");
          return;
        }
    fprintf(file, "%s %s %s %s %d %d %.2f %.2f %s\n",
            ride->ride_id, ride->driver, ride->origin, ride->destination,
            ride->seats_total, ride->seats_available, ride->distance_km,
            ride->time_minutes, ride->vehicle);
    fclose(file);
}

void clientHandler(SOCKET sock)
{
    char buffer[BUFFER_SIZE];
    int len;
    char response[BUFFER_SIZE];

    char client_message_copy[BUFFER_SIZE];

    while ((len = recv(sock, buffer, BUFFER_SIZE - 1, 0)) > 0)
        {
            buffer[len] = '\0';
            printf("Received from client: %s\n", buffer);
            strcpy(client_message_copy, buffer);
            char *cmd = strtok(client_message_copy, "|");

        if (cmd == NULL)
            {
              sprintf(response, "ERROR|Invalid command format\n");
              send(sock, response, strlen(response), 0);
              continue;
            }

        if (strcmp(cmd, "REGISTER") == 0)
            {
              char *username = strtok(NULL, "|");
              char *password = strtok(NULL, "|");
              if (username && password && register_user(username, password))
               {
                  sprintf(response, "SUCCESS|Registered successfully\n");
            }     else {
                sprintf(response, "ERROR|Registration failed\n");
            }
            send(sock, response, strlen(response), 0);
        } else if (strcmp(cmd, "LOGIN") == 0) {
            char *username = strtok(NULL, "|");
            char *password = strtok(NULL, "|");
            if (username && password && login_user(username, password)) {
                sprintf(response, "SUCCESS|Logged in successfully\n");
            } else {
                sprintf(response, "ERROR|Invalid username or password\n");
            }
            send(sock, response, strlen(response), 0);
        } else if (strcmp(cmd, "CREATE_RIDE") == 0) {
            Ride new_ride;
            // Parse all ride details from the buffer
            char *ride_id_str = strtok(NULL, "|");
            char *driver_str = strtok(NULL, "|");
            char *origin_str = strtok(NULL, "|");
            char *destination_str = strtok(NULL, "|");
            char *seats_str = strtok(NULL, "|");
            char *distance_str = strtok(NULL, "|");
            char *time_str = strtok(NULL, "|");
            char *vehicle_str = strtok(NULL, "|");

            if (ride_id_str && driver_str && origin_str && destination_str &&
                seats_str && distance_str && time_str && vehicle_str) {

                strcpy(new_ride.ride_id, ride_id_str);
                strcpy(new_ride.driver, driver_str);
                strcpy(new_ride.origin, origin_str);
                strcpy(new_ride.destination, destination_str);
                new_ride.seats_total = atoi(seats_str);
                new_ride.seats_available = new_ride.seats_total;
                new_ride.distance_km = atof(distance_str);
                new_ride.time_minutes = atof(time_str);
                strcpy(new_ride.vehicle, vehicle_str);

                save_ride(&new_ride); // Save to rides.txt
                sprintf(response, "SUCCESS|Ride created: %s\n", new_ride.ride_id);
            } else {
                sprintf(response, "ERROR|Invalid CREATE_RIDE format\n");
            }
            send(sock, response, strlen(response), 0);
        } else if (strcmp(cmd, "GET_TOTAL_SEATS") == 0) {
            char *ride_id_req = strtok(NULL, "|");
            if (ride_id_req == NULL) {
                sprintf(response, "ERROR|Invalid GET_TOTAL_SEATS format\n");
                send(sock, response, strlen(response), 0);
                continue;
            }

            FILE *file = fopen("rides.txt", "r");
            if (!file) {
                sprintf(response, "ERROR|Server error reading rides data\n");
                send(sock, response, strlen(response), 0);
                continue;
            }

            Ride current_ride;
            int found_ride = 0;
            while (fscanf(file, "%s %s %s %s %d %d %f %f %s", current_ride.ride_id, current_ride.driver, current_ride.origin, current_ride.destination, &current_ride.seats_total, &current_ride.seats_available, &current_ride.distance_km, &current_ride.time_minutes, current_ride.vehicle) == 9) {
                if (strcmp(current_ride.ride_id, ride_id_req) == 0) {
                    sprintf(response, "TOTAL_SEATS|%d\n", current_ride.seats_available);
                    found_ride = 1;
                    break;
                }
            }
            fclose(file);

            if (!found_ride) {
                sprintf(response, "ERROR|Ride not found\n");
            }
            send(sock, response, strlen(response), 0);

        } else if (strcmp(cmd, "BOOK") == 0) {
            char *ride_id = strtok(NULL, "|");
            char *username = strtok(NULL, "|");
            int seats = atoi(strtok(NULL, "|"));

            if (ride_id == NULL || username == NULL || seats <= 0) {
                sprintf(response, "ERROR|Invalid BOOK command format\n");
                send(sock, response, strlen(response), 0);
                continue;
            }

            FILE *file = fopen("rides.txt", "r");
            FILE *temp = fopen("temp_rides.txt", "w");
            FILE *log = fopen("history.txt", "a");
            if (!file || !temp || !log) {
                sprintf(response, "ERROR|Server error (file operation)\n");
                send(sock, response, strlen(response), 0);
                if (file) fclose(file);
                if (temp) fclose(temp);
                if (log) fclose(log);
                continue;
            }

            Ride ride;
            int found_and_booked = 0;
            while (fscanf(file, "%s %s %s %s %d %d %f %f %s", ride.ride_id, ride.driver, ride.origin, ride.destination, &ride.seats_total, &ride.seats_available, &ride.distance_km, &ride.time_minutes, ride.vehicle) == 9) {
                if (strcmp(ride.ride_id, ride_id) == 0) {
                    if (ride.seats_available >= seats) {
                        ride.seats_available -= seats;
                        found_and_booked = 1;

                        fprintf(log, "%s %s %s %s %s %d %.2f %.2f %s\n", username, ride.ride_id, ride.driver, ride.origin, ride.destination, seats, ride.distance_km, ride.time_minutes, ride.vehicle);
                        sprintf(response, "SUCCESS|Seats booked. Remaining: %d\n", ride.seats_available);
                    } else {
                        sprintf(response, "ERROR|Not enough seats available. Available: %d\n", ride.seats_available);
                    }
                }

                fprintf(temp, "%s %s %s %s %d %d %.2f %.2f %s\n", ride.ride_id, ride.driver, ride.origin, ride.destination, ride.seats_total, ride.seats_available, ride.distance_km, ride.time_minutes, ride.vehicle);
            }
            fclose(file);
            fclose(temp);
            fclose(log);

            remove("rides.txt");
            rename("temp_rides.txt", "rides.txt");

            if (!found_and_booked && strlen(response) == 0) {
                sprintf(response, "ERROR|Ride not found or booking failed\n");
            }
            send(sock, response, strlen(response), 0);
        } else if (strcmp(cmd, "SEARCH") == 0) {
            char *keyword = strtok(NULL, "|");
            if (keyword == NULL) {
                sprintf(response, "ERROR|Invalid SEARCH format\n");
                send(sock, response, strlen(response), 0);
                continue;
            }

            FILE *file = fopen("rides.txt", "r");
            if (!file) {
                sprintf(response, "ERROR|Server error reading rides data\n");
                send(sock, response, strlen(response), 0);
                continue;
            }

            Ride ride;
            char search_results[BUFFER_SIZE] = "SEARCH_RESULTS|\n";
            int found_any = 0;
            while (fscanf(file, "%s %s %s %s %d %d %f %f %s", ride.ride_id, ride.driver, ride.origin, ride.destination, &ride.seats_total, &ride.seats_available, &ride.distance_km, &ride.time_minutes, ride.vehicle) == 9) {
                if (strstr(ride.ride_id, keyword) || strstr(ride.origin, keyword) || strstr(ride.destination, keyword)) {
                    found_any = 1;
                    sprintf(search_results + strlen(search_results), "Ride ID: %s, Driver: %s, From: %s, To: %s, Avail Seats: %d, Dist: %.2fkm, Time: %.2fmins, Vehicle: %s\n",
                            ride.ride_id, ride.driver, ride.origin, ride.destination, ride.seats_available, ride.distance_km, ride.time_minutes, ride.vehicle);
                }
            }
            fclose(file);

            if (found_any) {
                send(sock, search_results, strlen(search_results), 0);
            } else {
                sprintf(response, "NO_RESULTS|No rides found for '%s'\n", keyword);
                send(sock, response, strlen(response), 0);
            }
        } else if (strcmp(cmd, "DELETE") == 0) {
            char *ride_id = strtok(NULL, "|");
            char *username = strtok(NULL, "|"); // Driver's username
            if (ride_id == NULL || username == NULL) {
                sprintf(response, "ERROR|Invalid DELETE format\n");
                send(sock, response, strlen(response), 0);
                continue;
            }

            FILE *file = fopen("rides.txt", "r");
            FILE *temp = fopen("temp_rides.txt", "w");
            if (!file || !temp) {
                sprintf(response, "ERROR|Server error (file operation)\n");
                send(sock, response, strlen(response), 0);
                if (file) fclose(file);
                if (temp) fclose(temp);
                continue;
            }

            Ride ride;
            int deleted = 0;
            while (fscanf(file, "%s %s %s %s %d %d %f %f %s", ride.ride_id, ride.driver, ride.origin, ride.destination, &ride.seats_total, &ride.seats_available, &ride.distance_km, &ride.time_minutes, ride.vehicle) == 9) {
                // Only delete if ride_id matches AND the deleter is the driver
                if (strcmp(ride.ride_id, ride_id) == 0 && strcmp(ride.driver, username) == 0) {
                    deleted = 1;
                    // Do not write this ride to temp file (effectively deletes it)
                } else {
                    fprintf(temp, "%s %s %s %s %d %d %.2f %.2f %s\n", ride.ride_id, ride.driver, ride.origin, ride.destination, ride.seats_total, ride.seats_available, ride.distance_km, ride.time_minutes, ride.vehicle);
                }
            }
            fclose(file);
            fclose(temp);
            remove("rides.txt");
            rename("temp_rides.txt", "rides.txt");

            if (deleted) {
                sprintf(response, "SUCCESS|Ride '%s' deleted\n", ride_id);
            } else {
                sprintf(response, "ERROR|Ride '%s' not found or you are not the driver\n", ride_id);
            }
            send(sock, response, strlen(response), 0);
        } else if (strcmp(cmd, "HISTORY") == 0) {
            char *username = strtok(NULL, "|");
            if (username == NULL) {
                sprintf(response, "ERROR|Invalid HISTORY format\n");
                send(sock, response, strlen(response), 0);
                continue;
            }

            FILE *file = fopen("history.txt", "r");
            if (!file) {
                sprintf(response, "NO_HISTORY|No ride history found.\n");
                send(sock, response, strlen(response), 0);
                continue;
            }

            char line[BUFFER_SIZE];
            char history_output[BUFFER_SIZE] = "HISTORY_RESULTS|\n";
            int found_history = 0;
            // Format in history.txt: passenger_username ride_id driver origin destination seats_booked distance time vehicle
            while (fgets(line, sizeof(line), file) != NULL) {
                char hist_user[50], hist_ride_id[20], hist_driver[50], hist_origin[50], hist_destination[50], hist_vehicle[50];
                int hist_seats_booked;
                float hist_distance, hist_time;

                // Parse the line according to your history.txt format
                // This assumes your history.txt line looks like:
                // "username ride_id driver origin destination seats_booked distance time vehicle\n"
                // Adjust sscanf format string if your history.txt format is different
                if (sscanf(line, "%s %s %s %s %s %d %f %f %s",
                           hist_user, hist_ride_id, hist_driver, hist_origin, hist_destination,
                           &hist_seats_booked, &hist_distance, &hist_time, hist_vehicle) == 9) {
                    if (strcmp(hist_user, username) == 0) {
                        found_history = 1;
                        sprintf(history_output + strlen(history_output),
                                "Ride ID: %s, Driver: %s, From: %s, To: %s, Seats Booked: %d, Dist: %.2fkm, Time: %.2fmins, Vehicle: %s\n",
                                hist_ride_id, hist_driver, hist_origin, hist_destination,
                                hist_seats_booked, hist_distance, hist_time, hist_vehicle);
                    }
                }
            }
            fclose(file);

            if (found_history) {
                send(sock, history_output, strlen(history_output), 0);
            } else {
                sprintf(response, "NO_HISTORY|No ride history found for %s.\n", username);
                send(sock, response, strlen(response), 0);
            }
        } else if (strcmp(cmd, "GET_BOOKED_RIDES_COUNT") == 0) { // NEW: Handle GET_BOOKED_RIDES_COUNT
            char *username_for_count = strtok(NULL, "|");
            if (username_for_count == NULL) {
                sprintf(response, "ERROR|Invalid GET_BOOKED_RIDES_COUNT format\n");
                send(sock, response, strlen(response), 0);
                continue;
            }

            FILE *file = fopen("history.txt", "r");
            if (!file) {
                // If history.txt doesn't exist, count is 0
                sprintf(response, "BOOKED_RIDES_COUNT|0\n");
                send(sock, response, strlen(response), 0);
                continue;
            }

            int booked_count = 0;
            char line[BUFFER_SIZE];
            char hist_user[50];

            while (fgets(line, sizeof(line), file) != NULL) {

                if (sscanf(line, "%s", hist_user) == 1) { // Read only the first word
                    if (strcmp(hist_user, username_for_count) == 0) {
                        booked_count++;
                    }
                }
            }
            fclose(file);

            sprintf(response, "BOOKED_RIDES_COUNT|%d\n", booked_count);
            send(sock, response, strlen(response), 0);
            printf("Sent booked rides count for user %s: %d\n", username_for_count, booked_count);

        } else {
            sprintf(response, "ERROR|Unknown command: %s\n", cmd);
            send(sock, response, strlen(response), 0);
        }
    }
    printf("Client disconnected.\n");
    closesocket(sock);
}

int main() {
    init_winsock();

    SOCKET server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    int c;

    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET) {
        printf("Could not create socket : %d", WSAGetLastError());
        return 1;
    }
    printf("Socket created.\n");


    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("Bind failed with error code : %d", WSAGetLastError());
        return 1;
    }
    printf("Bind done.\n");


    listen(server_socket, 3);

    printf("Waiting for incoming connections on port %d...\n", PORT);

    c = sizeof(struct sockaddr_in);
    while ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &c)) != INVALID_SOCKET) {
        printf("Connection accepted from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));


        clientHandler(client_socket);
        printf("Client handling finished. Waiting for new connections...\n");
    }

    if (client_socket == INVALID_SOCKET) {
        printf("accept failed with error code : %d", WSAGetLastError());
        return 1;
    }

    closesocket(server_socket);
    WSACleanup();

    return 0;
}
