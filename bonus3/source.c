#include <stdio.h>

int main(int argc, char *argv[])
{
    char buff[132];
    FILE *f = fopen("/home/user/end/.pass", "r");

    if (f == NULL || argc != 2)
        return -1;

    fread(buff, 1, 66, f);        // lit 66 octets du .pass dans buff
    buff[65] = 0;
    buff[atoi(argv[1])] = 0;      // buff[num] = 0  ← position contrôlée par l'user
    fread(buff + 66, 1, 65, f);   // 2e lecture, 65 octets à buff+66
    fclose(f);

    if (strcmp(buff, argv[1]) == 0)
        execl("/bin/sh", "/bin/sh", NULL);
    else
        puts(buff + 66);
    return 0;
}
