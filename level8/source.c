#include <stdlib.h>
#include <string.h>
#include <stdio.h>

char *globale_A;    // 0x8049aac
char *globale_B;    // 0x8049ab0

int main(void)
{
    char buffer[128];

    while (1) {
        printf("%p, %p \n", globale_B, globale_A);

        if (fgets(buffer, 128, stdin) == NULL)
            break;

        if (strncmp(buffer, "auth ", 5) == 0) {
            globale_A = malloc(4);
            *globale_A = 0;
            if (strlen(buffer + 5) <= 30)
                strcpy(globale_A, buffer + 5);
        }

        if (strncmp(buffer, "reset", 5) == 0)
            free(globale_A);          // pas de globale_A = NULL → use-after-free

        if (strncmp(buffer, "service", 6) == 0)
            globale_B = strdup(buffer + 7);

        if (strncmp(buffer, "login", 5) == 0) {
            if (globale_A[0x20] != 0)
                system("/bin/sh");
            else
                fwrite("Password:\n", 1, 10, stdout);
        }
    }

    return 0;
}