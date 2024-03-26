/* myclient.c */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <curses.h>
#include "mypw.h"
//#include <conio.h>
#define BUF 1024
#define PORT 6543
/**
 * Fordert den Benutzer auf, eine Eingabe zu tätigen und liest diese ein.
 * @param prompt Eine Aufforderung, die vor der Eingabe angezeigt wird.
 * @param out Der Puffer, in den die Eingabe gespeichert wird.
 * @param max_size Die maximale Größe der Eingabe.
 */
void get_input(char *prompt, char *out, int max_size) {
    printf("%s", prompt);
    if (fgets(out, max_size, stdin) == NULL) {
        perror("Error reading input");
        exit(EXIT_FAILURE);
    }

    // Is the input larger than given size?
    // Check for \n and if it is not there, clear stdin with getchar() until we see \n
    if (!strchr(out, '\n')) 
    {
        int c;
        while ((c = getchar()) != '\n' && c != EOF);
    }
}
/**
 * Sammelt eine Nachricht vom Benutzer bis zu einer bestimmten Größe.
 * @param message Der Puffer, in den die Nachricht gespeichert wird.
 * @param maxSize Die maximale Größe der Nachricht.
 */
void collect_message(char *message, int maxSize) 
{
    char buffer[BUF];
    char *message_ptr = message;
    int remaining_size = maxSize - 1;  // One byte for the null terminator

    printf("Message (end with a line containing only a dot): \n");
    while (1) {
        if(fgets(buffer, sizeof(buffer), stdin)==NULL)
        {
            printf("Error reding message.\n");
            break;
        }
        if (strcmp(buffer, ".\n") == 0) {
            break;
        }

        int to_copy = ((int)strlen(buffer) < remaining_size) ? (int)strlen(buffer) : remaining_size;
        strncpy(message_ptr, buffer, to_copy);
        message_ptr += to_copy;
        remaining_size -= to_copy;

        if (remaining_size <= 0) {
            break;
        }
    }
    *message_ptr = 0; // Null-terminate the message string
}
/**
 * Behandelt den Login-Befehl, indem Benutzername und Passwort eingelesen werden.
 * @param buffer Der Puffer, in den der Login-Befehl und die Anmeldeinformationen geschrieben werden.
 */
void handle_login_command(char* buffer)
{
    char temp[BUF];
    char password[BUF];

    memset(&temp,0,sizeof(temp));
    get_input("Username: ", temp, sizeof(temp));
    strncat(buffer,temp, BUF - strlen(buffer) - 1);

    get_pass(password, sizeof(password));
    printf("pass: %s\n", password);
    // Check if the buffer has enough space to concatenate the password
    if (strlen(buffer) + strlen(password) < BUF) {
        strncat(buffer, password, BUF - strlen(buffer) - 1);
        strncat(buffer, "\n", BUF - strlen(buffer) - 1); // Append newline
    } else {
        fprintf(stderr, "Error: Buffer overflow prevented.\n");
    }
}
/**
 * Behandelt den SEND-Befehl, indem Empfänger, Betreff und Nachricht eingelesen werden.
 * @param buffer Der Puffer, in den der SEND-Befehl und die Nachrichtendaten geschrieben werden.
 */
void handle_send_command(char* buffer) {
    char temp[BUF];

    // Get receiver
    get_input("Empfänger: ", temp, sizeof(temp));
    strncat(buffer, temp, BUF - strlen(buffer) - 1);

    // Get subject
    get_input("Betreff: ", temp, sizeof(temp));
    strncat(buffer, temp, BUF - strlen(buffer) - 1);

    // Get message
    printf("Nachricht: ");
    collect_message(temp, sizeof(temp));
    strncat(buffer, temp, BUF - strlen(buffer) - 1);
}
/**
 * Verarbeitet den READ-Befehl, indem die Nachrichtennummer abgefragt wird.
 * @param socket_fd Das Socket-File-Descriptor.
 * @param buffer Der Puffer, in dem die READ-Informationen gespeichert werden.
 */
void handle_read_command(int socket_fd, char* buffer) {
    char message_number[5];

    get_input("Message Number: ", message_number, sizeof(message_number));
    if(atoi(message_number) == 0)
    {
        printf("Invalid message number.\n");
        return;
    }
    // Senden Sie den READ-Befehl und die zugehörigen Informationen an den Server
    memset(buffer, 0, BUF);
    sprintf(buffer, "READ\n%s\n", message_number);
    send(socket_fd, buffer, strlen(buffer), 0);

    memset(buffer, 0, BUF);
    recv(socket_fd, buffer, sizeof(buffer), 0);
    if(strcmp(buffer, "ERR\n") == 0)
    {
        printf("Error reading message.\n");
        return;
    }
    // Collect the message until complete message arrives
    int total_bytes_received = 0;
    char *end_marker_ptr = NULL;
    
    while (!end_marker_ptr) {
        int bytes_received = recv(socket_fd, buffer + total_bytes_received, BUF - 1 - total_bytes_received, 0);
        if (bytes_received > 0) {
            total_bytes_received += bytes_received;

            // "<EndOfMessageMarker>" is the string the server will send when the message is complete
            // Since TCP protocol can send message in multiple packages we read until we see this
            if ((end_marker_ptr = strstr(buffer, "<EndOfMessageMarker>")) != NULL) {
                    // If we received the "<EndOfMessageMarker>", we should remove it from the buffer
                // before printing or processing the message  
                // It will also exits the while loop 
                *end_marker_ptr = '\0'; // Truncate the buffer to remove the end marker
            }
        }
    }

    if (strncmp(buffer, "OK", 2) == 0) 
    {
        printf("Message Content:\n");
        printf("%s", buffer+3);
    } 
    else if (strncmp(buffer, "ERR", 3) == 0) 
    {
        printf("Error reading message.\n");
    }
    memset(buffer, 0, BUF);
}
/**
 * Verarbeitet den DEL-Befehl, indem die Nachrichtennummer abgefragt wird.
 * @param buffer Der Puffer, in dem die DEL-Informationen gespeichert werden.
 */
void handle_del_command(char* buffer) {
    char temp[BUF];
    memset(temp, 0, sizeof(temp));
    printf("Nachrichten-Nummer: ");
    if(NULL == fgets(temp, BUF, stdin))
    {
        perror("Error reading Nachrichten-Nummer");
        return;
    }
    strncat(buffer, temp, BUF - strlen(buffer) - 1);
}
/**
 * Sendet eine Nachricht über das Socket an den Server und empfängt die Antwort.
 * @param socket_fd Das Socket-File-Descriptor.
 * @param buffer Der Puffer, der die zu sendende Nachricht enthält.
 */
void send_message(int socket_fd, char* buffer)
{
    int size;
    char temp[BUF];
    send(socket_fd, buffer, strlen(buffer), 0);
    size = recv(socket_fd, temp, BUF, 0);
    if (size > 0) {
        temp[size] = '\0';
        printf("Server response: %s", temp);
    }
    else
    {
        printf("Server didn't respond.");
    }
}
/**
 * Stellt eine Verbindung zum Server mit der angegebenen IP-Adresse her.
 * @param server_ip Die IP-Adresse des Servers als String.
 * @param socket_fd Zeiger auf eine Integer-Variable, in der das Socket-File-Descriptor gespeichert wird.
 * @param address Zeiger auf eine sockaddr_in-Struktur, die die Server-Adresse enthält.
 * @return Gibt 0 zurück, wenn die Verbindung erfolgreich hergestellt wurde, sonst -1.
 */
int establish_connection(const char* server_ip, int* socket_fd, struct sockaddr_in* address) {
	// Erstelle ein neues Socket vom Typ STREAM im Internetprotokollfamilie.
    *socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (*socket_fd == -1) {
        perror("Socket error");
        return -1;
    }
    // Vorbereiten der Serveradresse für den Verbindungsaufbau
    memset(address, 0, sizeof(*address));
    address->sin_family = AF_INET;
    address->sin_port = htons(PORT);
    // Umwandlung der IP-Adresse vom String in eine Netzwerkadresse
    if (!inet_pton(AF_INET, server_ip, &address->sin_addr)) {
        perror("Invalid address");
        close(*socket_fd);
        return -1;
    }
    // Versuch, eine Verbindung zum Server herzustellen
    if (connect(*socket_fd, (struct sockaddr *) address, sizeof(*address)) == -1) {
        perror("Connect error - no server available");
        close(*socket_fd);
        return -1;
    }

    printf("Connection with server (%s) established\n", inet_ntoa(address->sin_addr));
    return 0;
}
/**
 * Die Hauptfunktion des Clients, die die Verbindung zum Server aufbaut und auf Benutzereingaben wartet.
 */
int main (int argc, char **argv) {
    int create_socket;
    char buffer[BUF];
    struct sockaddr_in address;

    if( argc < 2 ){
        printf("Usage: %s ServerAdresse\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Try to connect to server
    if (establish_connection(argv[1], &create_socket, &address) == -1) {
        return EXIT_FAILURE;
    }

    while(1){
        memset(&buffer,0,BUF);
        printf ("Enter command: ");
        if(NULL == fgets(buffer, BUF, stdin))
        {
            perror("Error reading command");
            continue;
        }
		// Hauptschleife des Clients, um Befehle zu empfangen und zu verarbeiten.
        // LOGIN command
        if(strcmp (buffer, "login\n") == 0 || strcmp(buffer, "LOGIN\n") == 0){
            handle_login_command(buffer);
            send_message(create_socket, buffer);
        }
        // SEND command
        if (strcmp(buffer, "send\n") == 0 || strcmp(buffer, "SEND\n") == 0) {
            handle_send_command(buffer);
            send_message(create_socket, buffer);
        }
        if(strcmp (buffer, "list\n") == 0 || strcmp(buffer, "LIST\n") == 0){
            send_message(create_socket, buffer);
        }
        if(strcmp (buffer, "read\n") == 0 || strcmp(buffer, "READ\n") == 0){
            handle_read_command(create_socket, buffer);
        }
        if(strcmp (buffer, "del\n") == 0 || strcmp(buffer, "DEL\n") == 0){
            handle_del_command(buffer);
            send_message(create_socket, buffer);
        }
        if(strcmp (buffer, "quit\n") == 0 || strcmp (buffer, "QUIT\n") == 0){
            send_message(create_socket, buffer);
            break;
        }
    }
	// Schließt die Socket-Verbindung und beendet das Programm.
    close(create_socket);
    return EXIT_SUCCESS;
}
