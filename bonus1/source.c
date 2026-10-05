#include <string.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    int a = atoi(argv[1]);   // esp+0x3c
    char buffer[40];         // esp+0x14
    if (a <= 9)
    {
        memcpy(buffer, argv[2], a * 4);
        if (a == 0x574f4c46)   // "FLOW"
            execl("/bin/sh", "/bin/sh", NULL);
    }
    else
        return 1;
    return 0;
}