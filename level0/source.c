/*
** level0 — reconstruction lisible du binaire exploité.
**
** Ce n'est pas le code source original (indisponible) mais une reconstruction
** fidèle à partir du désassemblage de `main` (objdump / gdb).
**
** Le binaire est setuid level1 : lancé par level0, son uid EFFECTIF est level1
** mais son uid REEL reste level0.
**
** Faille : si argv[1] vaut la valeur magique 423, le programme aligne l'uid
** réel sur l'uid effectif (setresuid) PUIS exécute /bin/sh. Le shell obtenu
** tourne donc réellement en level1, ce qui donne accès à /home/user/level1/.pass.
*/

#include <stdlib.h>     /* atoi                                   */
#include <string.h>     /* strdup                                 */
#include <unistd.h>     /* setresgid, setresuid, execv, getegid, geteuid */
#include <stdio.h>      /* fwrite, stderr                         */

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
