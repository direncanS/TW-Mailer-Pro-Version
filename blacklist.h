// Zeitbibliothek für Zeitoperationen und Netzwerk-Bibliothek für Netzwerkoperationen inkludieren
#include <time.h>
#include <netinet/in.h>
#include <stdbool.h>

#define MAX_BLACKLISTED_IPS 100 // Maximale Anzahl von IP-Adressen in der Blacklist
#define INET_ADDRSTRLEN 16// Länge einer IPv4-Adresse in Zeichen
#define BLACKLIST_FILE "blacklist.txt"// Dateiname der Blacklist
#define BLACKLIST_TIME 60   // Zeit in Sekunden, wie lange eine IP auf der Blacklist bleibt

// Struktur zur Nachverfolgung von IP-Adressen und ihren fehlgeschlagenen Login-Versuchen
typedef struct {
    char ip[INET_ADDRSTRLEN]; // Speichert die IP-Adresse
    time_t blacklist_time; // Zeitpunkt, wann die IP von der Blacklist entfernt werden soll
} IpAttempt;

// Structure for the blacklist
typedef struct {
    IpAttempt blacklisted_ips[MAX_BLACKLISTED_IPS]; // Array von IP-Adressen mit Blacklist-Zeit
} Blacklist;


// Function declarations
void Blacklist_init(Blacklist *bl); // Initialisiert die Blacklist-Struktur
void Blacklist_persist(const Blacklist *bl); // Speichert die Blacklist in eine Datei
void Blacklist_load(Blacklist *bl); // Lädt die Blacklist aus einer Datei
bool Blacklist_isBlacklisted(Blacklist *bl, const char* ip); // Überprüft, ob eine IP auf der Blacklist ist
void Blacklist_addToBlacklist(Blacklist *bl, const char* ip); // Fügt eine IP zur Blacklist hinzu
bool is_field_empty(const IpAttempt* attempt);  // Überprüft, ob ein Eintrag leer ist

// Hier beginnen die Implementierungen der oben deklarierten Funktionen

void Blacklist_init(Blacklist *bl) {
// Setze den gesamten Speicherbereich der Blacklist-Struktur auf Null
    memset(bl, 0, sizeof(Blacklist));
}
/**
 * Überprüft, ob ein Feld in der Blacklist leer ist oder die Blacklist-Zeit abgelaufen ist.
 * @param attempt Zeiger auf die IpAttempt-Struktur, die überprüft werden soll.
 * @return True, wenn das Feld leer ist oder die Blacklist-Zeit abgelaufen ist, andernfalls False.
 */
bool is_field_empty(const IpAttempt* attempt) {
    // Check if IP is empty
    if (attempt->ip[0] == '\0') {
        return true;
    }

    // Check if blacklist time has passed
    time_t now = time(NULL);
    if (difftime(now, attempt->blacklist_time) > 0) {
        return true;
    }

    return false;
}
/**
 * Überprüft, ob eine IP-Adresse in der Blacklist enthalten ist.
 * @param bl Zeiger auf die Blacklist-Struktur.
 * @param ip Die zu überprüfende IP-Adresse als String.
 * @return True, wenn die IP auf der Blacklist ist, andernfalls False.
 */
bool Blacklist_isBlacklisted(Blacklist *bl, const char* ip) {
    time_t now = time(NULL);
    for (int i = 0; i < MAX_BLACKLISTED_IPS; i++) {
        // Vergleiche die IP-Adresse mit den Einträgen in der Blacklist
        if (strcmp(bl->blacklisted_ips[i].ip, ip) == 0) {
            // Überprüfe, ob die Blacklist-Zeit noch nicht abgelaufen ist
            if (difftime(now, bl->blacklisted_ips[i].blacklist_time) < 0) {
                return true;
            } else {
                // Wenn die Blacklist-Zeit abgelaufen ist, setze den Eintrag zurück und aktualisiere die Blacklist-Datei
                memset(&bl->blacklisted_ips[i], 0, sizeof(IpAttempt));
                Blacklist_persist(bl);
                return false;
            }
        }
    }
    return false;
}
/**
 * Lädt die Blacklist-Einträge aus einer Datei.
 * @param bl Zeiger auf die Blacklist-Struktur.
 */
void Blacklist_load(Blacklist *bl) {
    FILE *file = fopen(BLACKLIST_FILE, "r");
    // Fehlerbehandlung beim Öffnen der Datei
    if (file == NULL) {
        if (errno != ENOENT) {
            perror("Error opening blacklist file for reading");
        }
        return;
    }
    // Lese die Einträge aus der Datei und speichere sie in der Struktur
    char ip[INET_ADDRSTRLEN];
    time_t blacklist_time;
    int idx = 0;
    // Lese IP-Adresse und Blacklist-Zeit aus der Datei
    while (fscanf(file, "%s %ld\n", ip, &blacklist_time) == 2 && idx < MAX_BLACKLISTED_IPS) {
        strncpy(bl->blacklisted_ips[idx].ip, ip, INET_ADDRSTRLEN);
        bl->blacklisted_ips[idx].blacklist_time = blacklist_time;
        idx++;
    }

    fclose(file);
}
/**
 * Speichert die aktuellen Blacklist-Einträge in eine Datei.
 * @param bl Zeiger auf die Blacklist-Struktur.
 */
void Blacklist_persist(const Blacklist *bl) {
    FILE *file = fopen(BLACKLIST_FILE, "w");
    if (file == NULL) {
        perror("Error opening blacklist file for writing");
        return;
    }
    // Schreibe alle nicht-leeren Blacklist-Einträge in die Datei
    for (int i = 0; i < MAX_BLACKLISTED_IPS; i++) {
        if(!is_field_empty(&bl->blacklisted_ips[i])) {
            printf("Writing blacklist to file.\n");
            fprintf(file, "%s %ld\n", bl->blacklisted_ips[i].ip, bl->blacklisted_ips[i].blacklist_time);
        }
    }

    fclose(file);
}
/**
 * Fügt eine neue IP-Adresse zur Blacklist hinzu.
 * @param bl Zeiger auf die Blacklist-Struktur.
 * @param ip Die IP-Adresse, die zur Blacklist hinzugefügt werden soll.
 */
void Blacklist_addToBlacklist(Blacklist *bl, const char* ip) {
    // Füge die IP zur Blacklist hinzu und setze die Blacklist-Zeit
    printf("Adding to blacklist: %s\n", ip);
    for(int i = 0; i < MAX_BLACKLISTED_IPS; i++)
    {
        if (is_field_empty(&bl->blacklisted_ips[i])) {
            strncpy(bl->blacklisted_ips[i].ip, ip, INET_ADDRSTRLEN);
            bl->blacklisted_ips[i].blacklist_time = time(NULL) + BLACKLIST_TIME;
            break;
        }
    }
    // Speichere die aktualisierte Blacklist in die Datei
    Blacklist_persist(bl);
}