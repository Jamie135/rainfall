#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void n(void)
{
    system("/bin/cat /home/user/level7/.pass");
}

void m(void)
{
    puts("Nope");
}

int main(int argc, char **argv)
{
    char *buffer = malloc(64);
    void (**ptr)(void) = malloc(4);

    *ptr = m;                    /* par défaut, ptr pointe sur m */

    strcpy(buffer, argv[1]);     /* copie sans borne → overflow */

    (*ptr)();                    /* appel indirect de la fonction pointée */

    return 0;
}