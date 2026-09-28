#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    if (atoi(argv[1]) == 423)          /* 0x1a7 = 423 */
    {
        char *args[2];

        args[0] = strdup("/bin/sh");
        args[1] = NULL;

        setresgid(getegid(), getegid(), getegid()); /* gid réel = gid effectif */
        setresuid(geteuid(), geteuid(), geteuid()); /* uid réel = uid effectif */
        execv("/bin/sh", args);        /* shell exécuté en level1 */
    }
    else
        fwrite("No !\n", 1, 5, stderr);
    return (0);
}
