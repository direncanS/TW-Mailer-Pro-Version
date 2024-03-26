#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * Gibt ein Zeichen von der Standardeingabe (stdin) ohne Echo zurück.
 * Diese Funktion wird durch Manipulation der Terminal-Einstellungen erreicht.
 * @return Das gelesene Zeichen.
 */
int my_getch()
{
    int ch;
    // https://man7.org/linux/man-pages/man3/termios.3.html
    struct termios t_old, t_new;

    // https://man7.org/linux/man-pages/man3/termios.3.html
    // tcgetattr() gets the parameters associated with the object referred
    //   by fd and stores them in the termios structure referenced by
    //   termios_p
    tcgetattr(STDIN_FILENO, &t_old);
    
    // copy old to new to have a base for setting c_lflags
    t_new = t_old;

    // https://man7.org/linux/man-pages/man3/termios.3.html
    //
    // ICANON Enable canonical mode (described below).
    //   * Input is made available line by line (max 4096 chars).
    //   * In noncanonical mode input is available immediately.
    //
    // ECHO   Echo input characters.
    t_new.c_lflag &= ~(ICANON | ECHO);
    
    // sets the attributes
    // TCSANOW: the change occurs immediately.
    tcsetattr(STDIN_FILENO, TCSANOW, &t_new);

    ch = getchar();

    // reset stored attributes
    tcsetattr(STDIN_FILENO, TCSANOW, &t_old);

    return ch;
}
/**
 * Liest ein Passwort von der Standardeingabe, ohne es auf der Konsole anzuzeigen.
 * Stattdessen werden Asterisken (*) für die visuelle Rückmeldung gedruckt.
 * @param password Ein Puffer, in den das Passwort geschrieben wird.
 * @param max_size Die maximale Länge des Passworts einschließlich Nullterminierung.
 */
void get_pass(char *password, int max_size)
{
    int show_asterisk = 1;
    const char BACKSPACE = 127;
    const char RETURN = 10;

    unsigned char ch = 0;
    int password_length = 0;
    // Anweisung für den Benutzer
    printf("Password: ");
    fflush(stdout);
    while ((ch = my_getch()) != RETURN && password_length < max_size - 1) {
        if (ch == BACKSPACE) {
            // Behandlung der Rücktaste
            if (password_length != 0) {
                if (show_asterisk) printf("\b \b");
                password_length--;
            }
        } else {
            // Hinzufügen des gelesenen Zeichens zum Passwort und Anzeigen eines Asterisk
            password[password_length++] = ch;
            if (show_asterisk) putchar('*');
        }
    }
    printf("\n");
    password[password_length] = '\0'; // Null-terminate the password
}
