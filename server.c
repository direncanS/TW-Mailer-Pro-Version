/* myserver.c */
// Standardbibliotheken für Netzwerk- und Systemoperationen
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <ldap.h>
#include <errno.h>
#include <stdbool.h>
#include <time.h>
// Benutzerdefinierte Header-Dateien für Passwort- und Blacklist-Verwaltung

#include "mypw.h"
#include "blacklist.h"
// Konstanten für die Puffergröße, den Port und maximale Anzahl von Clients

#define BUF 1024
#define PORT 6543
#define MAXLINE 1500
#define MAX_CLIENTS 100 // Define a maximum number of concurrent clients
#define TRY_AMOUNT 3    // Maximum amount of trials
#define MAIL_DIR "mails"
// Struktur für Thread-Argumente

struct thread_args {
    int socket;
    int index;
    Blacklist* bl;
    char client_ip[INET_ADDRSTRLEN];
};
// Globale Variablen für Client-Threads und ihre Aktivität

pthread_t client_threads[MAX_CLIENTS];
int active_threads[MAX_CLIENTS] = {0}; // 0 indicates available slot

// Mutex for thread list manipulation
pthread_mutex_t thread_list_mutex = PTHREAD_MUTEX_INITIALIZER;
// Mutex for black list manipulation
pthread_mutex_t blacklist_mutex = PTHREAD_MUTEX_INITIALIZER;

// Funktion zur Bereinigung beendeter Threads
void cleanup_finished_threads() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (active_threads[i] == 1) { // If thread is active
            if (pthread_join(client_threads[i], NULL) == 0) { // Check if thread has finished
                pthread_mutex_lock(&thread_list_mutex);
                active_threads[i] = 0; // Mark thread as no longer active
                pthread_mutex_unlock(&thread_list_mutex);
            }
        }
    }
}

// Hilfsfunktionen für das Parsen von Benutzernamen und Passwörtern aus dem Puffer
char* get_user_from_buffer(char* buffer, int size) {
    if (buffer == NULL) {
        return NULL;
    }

    // "LOGIN\n" is 6 characters start after it
    char* start = strstr(buffer, "\n") + 1;
    if (start == NULL || start >= buffer + size) {
        return NULL;
    }

    char* end = strchr(start, '\n');
    if (end == NULL) {
        return NULL;
    }

    int username_length = end - start;
    char* user = malloc((username_length + 1) * sizeof(char));
    if (user == NULL) {
        return NULL; // Memory allocation failed
    }

    strncpy(user, start, username_length);
    user[username_length] = '\0'; // Null-terminate the string
    return user;
}
/**
 * `get_pass_from_buffer` extrahiert das Passwort aus einem Puffer, nachdem der Benutzername und das Kommando "LOGIN\n" übersprungen wurden.
 * @param buffer Der Puffer, der die Eingabe enthält.
 * @param size Die Größe des Puffers.
 * @return Ein Zeiger auf einen neu zugewiesenen Speicherbereich, der das Passwort als String enthält oder NULL, wenn ein Fehler auftritt.
 */
char* get_pass_from_buffer(char* buffer, int size) {
    if (buffer == NULL) {
        printf("Buffer NULL\n");
        return NULL;
    }
    // Überspringt das "LOGIN\n" im Puffer, um den Anfang des Passworts zu finden
    char* start = strstr(buffer, "\n") + 1; // Skip "LOGIN\n"
    if (start == NULL || start >= buffer + size) {
        printf("Cant skip login\n");
        return NULL;
    }
	// Überspringt den Benutzernamen im Puffer, um zum Passwort zu gelangen

    start = strchr(start, '\n') + 1; // Skip username
    if (start == NULL || start >= buffer + size) {
        printf("Cant skip username\n");
        return NULL;
    }
    // Findet das Ende des Passworts im Puffer

    char* end = strchr(start, '\n');
    if (end == NULL) {
        printf("End is NULL\n");
        return NULL;
    }
    // Berechnet die Länge des Passworts und reserviert Speicher dafür
    int password_length = end - start;
    char* pass = malloc((password_length + 1) * sizeof(char));
    if (pass == NULL) {
        printf("Password length: %i\n", password_length);
        return NULL; // Memory allocation failed
    }
    // Kopiert das Passwort in den zugewiesenen Speicher und terminiert es mit Null
    strncpy(pass, start, password_length);
    pass[password_length] = '\0'; // Null-terminate the string

    return pass;
}
/**
 * `get_email_counter` liest die Anzahl der E-Mails aus einer Datei, die den Zähler für einen bestimmten Empfänger enthält.
 * @param receiver Der Empfänger, für den der E-Mail-Zähler abgerufen wird.
 * @return Die Anzahl der E-Mails oder -1, wenn der Empfänger NULL ist oder 0, wenn die Datei nicht gelesen werden kann.
 */
int get_email_counter(const char* receiver) {
    if (receiver == NULL) {
        return -1; // Invalid input
    }
    // Baut den Dateipfad zum Zähler des Empfängers

    char filepath[100] = "counter_";
    strncat(filepath, receiver, sizeof(filepath) - strlen(filepath) - 1);
    // Öffnet die Datei zum Lesen des Zählers

    FILE* fptr = fopen(filepath, "r");
    if (fptr == NULL) {
        return 0; // File doesnt exist returning 0
    }

    int num;
	// Liest den Zähler aus der Datei

    if (fscanf(fptr, "%d", &num) != 1) {
        num = 0; // File read error returning 0
    }
    fclose(fptr);
    printf("Email counter: %d\n",num);
    return num;
}
/**
 * Diese Funktion behandelt das Löschen einer Nachricht für einen bestimmten Benutzer.
 * @param client_socket Der Socket des Clients.
 * @param username Der Benutzername, dessen Nachrichten verwaltet werden.
 * @param buffer Der Puffer, der den Befehl enthält.
 */
void handle_del_command(int client_socket, const char *username, char *buffer) {
    char c_message_number[5];  // Max 9999 messages assumed
    int i_message_number;
    char filepath[512];
    DIR *dir;
    struct dirent *entry;
    int current_message = 0;
    
    // Parse the command for username and message number
    char *ptr = buffer;
    ptr += 4;  // Skip "DEL\n"

    // Extrahieren der Nachrichtennummer aus dem Puffer

    char *next_ptr = strchr(ptr, '\n');
    int len = (next_ptr - ptr < (int)sizeof(c_message_number) - 1) ? next_ptr - ptr : (int)sizeof(c_message_number) - 1;
    strncpy(c_message_number, ptr, len);
    c_message_number[len] = '\0';
    i_message_number = atoi(c_message_number);
    // Fehlerüberprüfung für die Nachrichtennummer
    if (i_message_number <= 0) {
        send(client_socket, "ERR\n", 4, 0);
        return;
    }

    // Erstellen des Dateipfads zum Postfach des Benutzers
    snprintf(filepath, sizeof(filepath), "%s/%s", MAIL_DIR, username);

    // Öffnen des Verzeichnisses und Lokalisieren der nth Nachricht
    dir = opendir(filepath);
    if (!dir) {
        send(client_socket, "ERR\n", 4, 0);
        return;
    }

    char message_filepath[PATH_MAX];
    // Nachrichtennummer im Verzeichnis finden
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            current_message++;
            if (current_message == i_message_number) {
                // Erstellen des Dateipfads für die Nachricht
                snprintf(message_filepath, sizeof(message_filepath), "%s/%s", filepath, entry->d_name);
                break;
            }
        }
    }
    closedir(dir);

    // Löschen der Nachricht, wenn sie gefunden wurde
    if (current_message == i_message_number) {
        if (remove(message_filepath) == 0) {
            send(client_socket, "OK\n", 3, 0);
        } else {
            send(client_socket, "ERR\n", 4, 0);
        }
    } else {
        // Nachrichtennummer liegt außerhalb des gültigen Bereichs
        send(client_socket, "ERR\n", 4, 0);
    }
    printf("Successfully processed DEL.\n");
}
/**
 * Diese Funktion behandelt das Auflisten aller Nachrichten im Posteingang eines Benutzers.
 * @param client_socket Der Socket des Clients.
 * @param sender Der Benutzername, dessen Posteingang aufgelistet wird.
 * @param buffer Der Puffer für die Kommunikation mit dem Client.
 */
void handle_list_command(int client_socket, const char *sender, char *buffer) {
    char filepath[512];
    DIR *dir;
    struct dirent *entry;
    int count = 0;

    // Erstellen des Dateipfads zum Posteingang des Benutzers
    snprintf(filepath, sizeof(filepath), "%s/%s", MAIL_DIR, sender);
    dir = opendir(filepath);
    if (!dir) 
    {
        send(client_socket, "0\n", 2, 0);
        return;
    }

    // Zählen der Nachrichten im Verzeichnis
    while ((entry = readdir(dir)) != NULL) 
    {
        if (entry->d_type == DT_REG) 
        {
            count++;
        }
    }
    snprintf(buffer, BUF, "%d\n", count);
    printf("Message count: %s\n", buffer);
    send(client_socket, buffer, strlen(buffer), 0);
    // Clear buffer
    memset(buffer, 0, BUF);
    // Mesaj konularÄ±nÄ± listele
    rewinddir(dir);
    while ((entry = readdir(dir)) != NULL) 
    {
        if (entry->d_type == DT_REG) 
        {
			// Senden der Nachrichtenzahl an den Client
            snprintf(buffer, BUF, "%.1023s\n", entry->d_name);
            send(client_socket, buffer, strlen(buffer), 0);
            printf("Sending message: %s", buffer);
            // Clear buffer
            memset(buffer, 0, BUF);
        }
    }
    closedir(dir);
    printf("Successfully processed LIST.\n");
}
/**
 * Behandelt das SEND-Kommando, um eine Nachricht zu speichern.
 * @param client_socket Der Socket des Clients, von dem die Anfrage kommt.
 * @param sender Der Absender der Nachricht.
 * @param buffer Der Puffer, der das SEND-Kommando und die Nachrichtendaten enthält.
 */
void handle_send_command(int client_socket, const char* sender, char *buffer)
{
    printf("Handling SEND command...\n");
    char receiver[9];
    char subject[81];
    char message[BUF];
    char filepath[512];
    FILE *file;

    // Initialisierung der Variablen
    memset(receiver, 0, sizeof(receiver));
    memset(subject, 0, sizeof(subject));
    memset(message, 0, sizeof(message));
    // Überspringen des Befehls "SEND\n" im Puffer
    char *ptr = buffer;
    ptr += 5;  // Skip "SEND\n"

    // Extraktion des Empfängers
    char* next_ptr = strchr(ptr, '\n');
    int len = (next_ptr - ptr < (int)sizeof(receiver) - 1) ? next_ptr - ptr : (int)sizeof(receiver) - 1;
    strncpy(receiver, ptr, len);
    printf("Receiver: %s\n", receiver);
    ptr = next_ptr + 1;

    // Extraktion des Betreffs
    next_ptr = strchr(ptr, '\n');
    len = (next_ptr - ptr < (int)sizeof(subject) - 1) ? next_ptr - ptr : (int)sizeof(subject) - 1;
    strncpy(subject, ptr, len);
    printf("Subject: %s\n", subject);
    ptr = next_ptr + 1;

    // Extraktion der Nachricht bis zum ".\n"
    char *end_msg_ptr = strstr(ptr, ".\n");
    if (end_msg_ptr) {
        int msg_len = end_msg_ptr - ptr;
        strncpy(message, ptr, msg_len < BUF ? msg_len : BUF - 1);
    }
    printf("Message: %s\n", message);

    // Überprüfung, ob das Verzeichnis des Benutzers existiert, und Erstellung, falls nicht
    snprintf(filepath, sizeof(filepath), "%s/%s", MAIL_DIR, receiver);
    if (mkdir(filepath, 0755) == -1) 
    {
        if (errno != EEXIST) 
        {
            perror("Directory creation failed");
            send(client_socket, "ERR\n", 4, 0);
            return;
        }
    }

    // Speichern der Nachricht im Posteingang des Empfängers
    snprintf(filepath, sizeof(filepath), "%s/%s/%s", MAIL_DIR, receiver, subject);
    file = fopen(filepath, "w");
    if (!file) 
    {
        printf("Error: Failed to open file for writing.\n");
        send(client_socket, "ERR\n", 4, 0);
        return;
    }
    // Schreiben der Daten in die Datei
    fprintf(file, "Sender: %s\n", sender);
    fprintf(file, "Subject: %s\n", subject);
    fprintf(file, "Message: %s\n", message);
    fclose(file);
    // Bestätigung an den Client senden

    send(client_socket, "OK\n", 3, 0);
    printf("Successfully processed SEND.\n");
}
/**
 * Behandelt das READ-Kommando, um eine bestimmte Nachricht zu lesen.
 * @param client_socket Der Socket des Clients, von dem die Anfrage kommt.
 * @param username Der Benutzername, dessen Nachrichten gelesen werden.
 * @param buffer Der Puffer, der das READ-Kommando und die Nachrichtennummer enthält.
 */
void handle_read_command(int client_socket, const char* username, char *buffer) {
    char c_message_number[5]; // Annahme: Die maximale Nachrichtenanzahl ist 9999
    int i_message_number;
    char filepath[512];
    FILE *file;
    // Überspringen des Befehls "READ\n" im Puffer    
    char *ptr = buffer;
    ptr += 5;  // Skip "READ\n"

    // Extraktion der Nachrichtennummer
    char *next_ptr = strchr(ptr, '\n');
    int len = (next_ptr - ptr < (int)sizeof(c_message_number) - 1) ? next_ptr - ptr : (int)sizeof(c_message_number) - 1;
    strncpy(c_message_number, ptr, len);
    c_message_number[len] = '\0';
    printf("Message number: %s\n", c_message_number);

    i_message_number = atoi(c_message_number);

    // Aufbau des Pfads zum Postfach des Benutzers
    snprintf(filepath, sizeof(filepath), "%s/%s", MAIL_DIR, username);

    // Versuch, das Verzeichnis zu öffnen und die Nachrichtennummer zu überprüfen
    DIR *dir = opendir(filepath);
    if (!dir || i_message_number == 0) {
        send(client_socket, "ERR\n", 4, 0);
        return;
    }
    // Iteration über die Einträge im Verzeichnis
    struct dirent *entry;
    int current_message = 0;
    char message_filepath[BUF] = {0};
    // Iterate over directory entries
    while ((entry = readdir(dir)) != NULL && current_message < i_message_number) {
        // Skip "." and ".." entries and only consider regular files
        if (entry->d_type == DT_REG) { // Nur reguläre Dateien betrachten
            current_message++;
            if (current_message == i_message_number) {
                // Aufbau des Pfads zur Nachrichtendatei
                snprintf(message_filepath, sizeof(message_filepath), "%s/%s", filepath, entry->d_name);
                break;
            }
        }
    }
    closedir(dir);
    char file_buffer[BUF] = {0};

    // Wenn die Nachrichtennummer gültig ist und der Pfad zur Datei festgelegt wurde
    if (current_message == i_message_number && message_filepath[0] != '\0') {
        // Open the file to read the message
        file = fopen(message_filepath, "r");
        if (!file) {
            send(client_socket, "ERR\n", 4, 0);
            return;
        }
        // Send OK
        send(client_socket, "OK\n", 3, 0);
        // Read and send the contents of the file        
        while (fgets(file_buffer, BUF, file) != NULL) {
            send(client_socket, file_buffer, strlen(file_buffer), 0);
        }
        fclose(file); // Schließen der Datei nach dem Lesen

        // Send the end-of-message marker
        const char* endOfMessageMarker = "<EndOfMessageMarker>";
        send(client_socket, endOfMessageMarker, strlen(endOfMessageMarker), 0);
    } else {
        // Nachrichtennummer liegt außerhalb des gültigen Bereichs
        send(client_socket, "ERR\n", 4, 0);
    }
    printf("Successfully processed READ.\n");
}
/**
 * Diese Funktion verarbeitet den Login-Vorgang eines Benutzers.
 * @param is_logged Zeiger auf eine Variable, die angibt, ob der Benutzer eingeloggt ist.
 * @param new_socket Der Socket für die Kommunikation mit dem Benutzer.
 * @param buffer Der Puffer, der den Login-Befehl und die Anmeldeinformationen enthält.
 * @param size Die Größe des Puffers.
 * @param username Ein Puffer, in dem der Benutzername gespeichert wird, wenn der Login erfolgreich ist.
 */
void handle_login(bool* is_logged, int new_socket, char* buffer,int size, char* username){
    // Analysieren des Benutzernamens und Passworts aus dem Puffer

    // Parse the username and password from the buffer
    char* user = get_user_from_buffer(buffer, size);
    if (user == NULL) {
        send(new_socket, "Invalid username format\n", 25, 0);
        return;
    }
    // read username (bash: export ldapuser=<yourUsername>)
    char ldapBindUser[256];
    sprintf(ldapBindUser, "uid=%s,ou=people,dc=technikum-wien,dc=at", user);
    printf("user set to: %s\n", ldapBindUser);


    char* pass = get_pass_from_buffer(buffer, size);
    if (pass == NULL) {
        free(user); // Free user before returning
        send(new_socket, "Invalid password format\n", 25, 0);
        return;
    }

    // LDAP config
    const char *ldapUri = "ldap://ldap.technikum-wien.at:389";
    const int ldapVersion = LDAP_VERSION3;
    // Initialize LDAP connection
    LDAP *ldapHandle;
    int rc = ldap_initialize(&ldapHandle, ldapUri);
    if (rc != LDAP_SUCCESS) {
        free(user);
        free(pass);
        send(new_socket, "Failed to initialize LDAP\n", 27, 0);
        return;
    }

    // Set LDAP version
    rc = ldap_set_option(ldapHandle, LDAP_OPT_PROTOCOL_VERSION, &ldapVersion);
    if (rc != LDAP_OPT_SUCCESS) {
        free(user);
        free(pass);
        ldap_unbind_ext_s(ldapHandle, NULL, NULL);
        send(new_socket, "Failed to set LDAP version\n", 28, 0);
        return;
    }

    // Start TLS
    rc = ldap_start_tls_s(ldapHandle, NULL, NULL);
    if (rc != LDAP_SUCCESS) {
        free(user);
        free(pass);
        ldap_unbind_ext_s(ldapHandle, NULL, NULL);
        send(new_socket, "Failed to start TLS\n", 21, 0);
        return;
    }

    // Bind credentials
    BerValue bindCredentials;
    bindCredentials.bv_val = pass;
    bindCredentials.bv_len = strlen(pass);
    rc = ldap_sasl_bind_s(ldapHandle, ldapBindUser, LDAP_SASL_SIMPLE, &bindCredentials, NULL, NULL, NULL);
    if (rc != LDAP_SUCCESS) {
        free(user);
        free(pass);
        ldap_unbind_ext_s(ldapHandle, NULL, NULL);
        printf("LDAP bind error\n");
        send(new_socket, "ERR\n", 5, 0);
        return;
    }

    // Successful login
    *is_logged = true;
    strcpy(username, user);
    printf("Successful Login. User: %s\n", username);
    send(new_socket, "OK\n", 18, 0);

    // Cleanup
    free(user);
    free(pass);
    ldap_unbind_ext_s(ldapHandle, NULL, NULL);
}
/**
 * Diese Funktion repräsentiert einen Client-Thread. 
 * Sie wird aufgerufen, sobald ein neuer Client sich verbindet und ist zuständig für die Kommunikation 
 * mit dem Client sowie die Ausführung der gegebenen Kommandos.
 * @param arg Ein Zeiger auf eine Struktur, die die Argumente für den Thread enthält.
 * @return Ein Nullzeiger, da diese Funktion keinen Wert zurückgibt.
 */
void* client_thread(void* arg){
    // Extrahieren der Thread-Argumente aus der Struktur
    struct thread_args *args = (struct thread_args*)arg;
    int new_socket = args->socket;
    int thread_index = args->index;
    char client_ip[INET_ADDRSTRLEN];
    strcpy(client_ip, args->client_ip);
    Blacklist* bl = args->bl;
	// Die Speicherfreigabe für die Argumente, da sie nun kopiert wurden
    free(args);
    // Initialisierung der Variablen für den Login-Status und den Kommunikationspuffer
    bool is_logged=false;
    char buffer[BUF];
    int size;
    int try = 0;
    char user[100];
	// Startmeldung für den neuen Thread
    printf("Starting new thread\n");
	// Hauptschleife für die Kommunikation mit dem Client
    while(true){
        // Puffer zurücksetzen und Nachrichten vom Client empfangen
        memset(&buffer,0, sizeof(buffer));
        size = recv(new_socket, buffer, BUF, 0);
        // Empfangene Nachricht ausgeben		
        printf("Received: %s\n",buffer);
        // Prüfen, ob der Client die Verbindung getrennt hat
        if (size <= 0) {
            if (size == 0) {
                // Client disconnected
                printf("Client disconnected\n");
            } else {
                // recv error
                perror("recv error");
            }
            break;
        }
        // Verarbeitung der empfangenen Befehle
        if(strncmp(buffer,"LOGIN",5)==0 || strncmp(buffer,"login",5)==0){
            printf("Received login.");
            try++;
            printf("Try: %i for IP %s\n",try, client_ip);
            if(Blacklist_isBlacklisted(bl, client_ip))
            {
                printf("IP in blacklist: %s\n", client_ip);
                try = 0;
                send(new_socket, "ERR\n", 4, 0);
            }
            else if(try >= TRY_AMOUNT)
            {
                printf("Tried 3 times. IP now in blacklist: %s\n", client_ip);
                pthread_mutex_lock(&blacklist_mutex);
                Blacklist_addToBlacklist(bl, client_ip);
                pthread_mutex_unlock(&blacklist_mutex);
                send(new_socket, "ERR\n", 4, 0);

            }
            else if(is_logged){
                printf("Already logged in..\n");
                send(new_socket, "ERR\n", 4, 0);
            }
            else
            {
                handle_login(&is_logged, new_socket, buffer, size, user);
            }
            continue;
        }
        if((strncmp(buffer,"SEND",4)==0 || strncmp(buffer,"send",4)==0) && is_logged){
            handle_send_command(new_socket, user, buffer);
        }
        else if((strncmp(buffer,"LIST",4)==0 || strncmp(buffer,"list",4)==0) && is_logged){
            handle_list_command(new_socket, user, buffer);
        }
        else if((strncmp(buffer,"READ",4)==0 || strncmp(buffer,"read",4)==0) && is_logged){
            handle_read_command(new_socket, user, buffer);
        }
        else if ((strncmp(buffer,"DEL",3)==0 || strncmp(buffer,"del",3)==0) && is_logged){
            handle_del_command(new_socket, user, buffer);
        }
        else if(strncmp (buffer, "quit\n", 6)== 0 ||  strncmp (buffer, "QUIT\n", 6) == 0){
            send(new_socket, &("Closing connection\n") , strlen("Closing connection\n"),0);
            printf("Closed connection\n");
            break;
        }
        else{
            send(new_socket, "ERR\n" , 4,0);
            printf("Not logged in or incorrect Command\n");
        }
     }
    // Bereinigung und Schließen des Sockets, wenn der Thread beendet wird
    close(new_socket);
    pthread_mutex_lock(&thread_list_mutex);
    active_threads[thread_index] = 0; // Mark this thread as no longer active
    pthread_mutex_unlock(&thread_list_mutex);
    return NULL;
}
/**
 * Die Hauptfunktion des Servers, die den Netzwerksocket initialisiert, 
 * auf eingehende Verbindungen wartet und für jede Verbindung einen neuen Thread startet.
 * @return EXIT_SUCCESS, wenn der Server ordnungsgemäß heruntergefahren wird, 
 *         sonst ein Fehlercode.
 */
int main (void) {

    // Überprüfen, ob das Verzeichnis für E-Mails existiert, sonst erstellen
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s", MAIL_DIR);
    if (mkdir(filepath, 0755) == -1) 
    {
        if (errno != EEXIST) 
        {
            perror("Directory creation failed");
            return -1;
        }
    }
    // Initialisierung und Laden der Blacklist
    Blacklist bl;
    Blacklist_init(&bl);
    Blacklist_load(&bl);
    // Erstellung des Netzwerksockets
    int create_socket;
    socklen_t addrlen;
    char buffer[BUF];
    struct sockaddr_in address, cliaddress;
    int option=1;
    create_socket = socket (AF_INET, SOCK_STREAM, 0);
    setsockopt(create_socket, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
    //setsockopt(create_socket,AF_INET,SO_REUSEADDR);
	// Vorbereitung der Serveradresse und Bindung an den Port
    memset(&address,0,sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    // Binden des Sockets an die Serveradresse
    if (bind (create_socket, (struct sockaddr *) &address, sizeof (address)) != 0) {
        perror("bind error");
        return EXIT_FAILURE;
    }
    // Der Server beginnt, auf Verbindungen zu warten
    listen (create_socket, 5);
    addrlen = sizeof (struct sockaddr_in);
    // Warten auf Verbindungen in einer Endlosschleife
    printf("Waiting for connections...\n");
    while (1) {
        int new_socket;
        new_socket = accept(create_socket, (struct sockaddr*) &cliaddress, &addrlen );
        // Verarbeitung von Verbindungsfehlern
        if(new_socket <= 0){
            printf ("Connection Error\n");
            strcpy(buffer,"Connection Error\n");
            continue;
        }
        // Erfolgreiche Verbindung, Beginn der Threaderstellung
        if (new_socket > 0) {
            printf("Client connected from %s:%d...\n", inet_ntoa(cliaddress.sin_addr), ntohs(cliaddress.sin_port));
            // Sperren des Mutex, um die Thread-Liste zu bearbeiten            
            pthread_mutex_lock(&thread_list_mutex);
            // Suchen nach einem freien Slot für den neuen Thread
            int thread_index = -1;
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (active_threads[i] == 0) {
                    active_threads[i] = 1;
                    thread_index = i;
                    break;
                }
            }
            // Wenn ein Thread-Index gefunden wurde, erstelle einen neuen Thread			
            if (thread_index != -1) {
                struct thread_args *args = malloc(sizeof(struct thread_args));
                args->socket = new_socket;
                args->index = thread_index;
                args->bl = &bl;
                inet_ntop(AF_INET, &cliaddress.sin_addr, args->client_ip, INET_ADDRSTRLEN);
                // Erstellung des Client-Threads
                int ret = pthread_create(&client_threads[thread_index], NULL, client_thread, args);
                if (ret != 0) {
                    // Handle the error based on the value of ret, e.g., print an error message
                    fprintf(stderr, "Error - pthread_create() return code: %d\n", ret);
                    exit(EXIT_FAILURE);
                }
                // Der Thread wird sofort detachiert, um Ressourcen freizugeben, sobald er fertig ist
                pthread_detach(client_threads[thread_index]);
            }
            else
            {
                // Server hat maximale Kapazität erreicht und lehnt die Verbindung ab
                strcpy(buffer, "Server is at capacity, please try again later.\n");
                send(new_socket, buffer, strlen(buffer), 0);
                close(new_socket);
                printf("Rejected connection from %s:%d, server at capacity.\n", inet_ntoa(cliaddress.sin_addr), ntohs(cliaddress.sin_port));
            }
            // Freigeben des Mutex für die Thread-Liste
            pthread_mutex_unlock(&thread_list_mutex);
        }
        cleanup_finished_threads(); // Clean up any finished threads
    }
    close (create_socket);
    return EXIT_SUCCESS;
}
