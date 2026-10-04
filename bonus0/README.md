# bonus0

## Objectif

Lire le fichier `.pass` de l'utilisateur suivant pour obtenir son mot de passe.

## 1. Reconnaissance

Le programme s'articule en trois fonctions : `p` (lit une saisie), `pp`
(enchaîne deux lectures et les concatène) et `main` (fournit le buffer final).

### `p` — la lecture et le `strncpy` fautif

```asm
080484b4 <p>:
 80484da:  movl   $0x0,(%esp)
 80484e1:  call   8048380 <read@plt>      ; read(0, buffer, 4104)
 80484e6:  movl   $0xa,0x4(%esp)          ; 0xa = '\n'
 80484ee:  lea    -0x1008(%ebp),%eax
 80484f7:  call   80483d0 <strchr@plt>    ; strchr(buffer, '\n')
 80484fc:  movb   $0x0,(%eax)             ; *resultat = '\0'   (remplace le '\n')
 8048505:  movl   $0x14,0x8(%esp)         ; 0x14 = 20  (taille du strncpy)
 8048517:  call   80483f0 <strncpy@plt>   ; strncpy(dest, buffer, 20)
```

En C (voir `source.c`) :

```c
read(0, buffer, 4104);
retaddr = strchr(buffer, 10);   // cherche le '\n'
*retaddr = '\0';                // le remplace par une fin de chaine
strncpy(a, buffer, 20);         // copie 20 octets dans a
```

### Que fait `strncpy` exactement ?

`strncpy(dest, src, n)` copie **au plus `n` octets** de `src` vers `dest` :

- si `src` (jusqu'à son `\0`) fait **moins de `n`** caractères → il copie la chaîne
  **et complète avec des `\0`** jusqu'à `n` (dest est bien terminé) ;
- si `src` fait **`n` caractères ou plus** → il copie **exactement `n` octets et
  s'arrête, SANS ajouter de `\0`**. ⚠️ `dest` n'est alors **pas** une chaîne
  terminée.

Ici `n = 20`. Donc **dès que la saisie fait ≥ 20 caractères, le buffer de
destination ne reçoit aucun `\0` final.**

### Le rôle du `\n` et le contournement du `\0`

Le `\n` est ce qui termine ta ligne au clavier. Le programme le transforme en
`\0` (via `strchr` + `movb $0x0`) pour faire une vraie chaîne **dans `buffer`**
(le tampon local de 4104 octets). **Mais** ce `\0` se trouve à la position du
`\n`, c'est-à-dire **après** tes 20 premiers caractères si tu en envoies 20+.

Or `strncpy` ne recopie que **les 20 premiers octets** : le `\0` (placé plus
loin) **n'est jamais copié** dans la destination. En envoyant exactement 20
caractères (sans `\n` dans ces 20), **on « contourne » le `\0`** : le buffer de
destination contient 20 octets **non terminés**.

### Pourquoi ça déborde : `pp`

```asm
0804851e <pp>:
 804852e:  lea    -0x30(%ebp),%eax        ; buffer1 = ebp-0x30
 8048534:  call   80484b4 <p>             ; p(buffer1, " - ")
 8048541:  lea    -0x1c(%ebp),%eax        ; buffer2 = ebp-0x1c
 8048547:  call   80484b4 <p>             ; p(buffer2, " - ")
 8048559:  call   80483a0 <strcpy@plt>    ; strcpy(dest, buffer1)
 8048598:  call   8048390 <strcat@plt>    ; strcat(dest, buffer2)
```

`buffer1` (`ebp-0x30`) et `buffer2` (`ebp-0x1c`) sont distants de
`0x30 - 0x1c = 0x14 = 20` octets : **ils sont collés** (20 octets chacun).

Conséquence de l'absence de `\0` :

```c
strcpy(buffer, buffer1);   // copie buffer1 jusqu'au '\0'...
```

Si `buffer1` fait 20 octets **sans `\0`**, `strcpy` **ne trouve pas de fin** et
**continue de lire dans `buffer2`** (juste derrière) → il copie **jusqu'à 40
octets** d'un coup. Puis `strcat(buffer, buffer2)` rajoute encore `buffer2`.

### La conséquence dans `main`

```c
int main(void)
{
  char buffer[54];   // petit buffer
  pp(buffer);        // pp peut y ecrire BIEN plus que 54 octets
  puts(buffer);
}
```

`main` ne réserve qu'un **petit buffer** pour recevoir `buffer1 + " " + buffer2`.
Amplifié par le `\0` manquant, `pp` y écrit plus que sa capacité → **débordement
de la pile de `main`** jusqu'à son **adresse de retour**.

**En une phrase :** `strncpy(…, 20)` oublie le `\0` dès 20 caractères, donc
`strcpy` recopie `buffer1` **et** `buffer2` collés (≈ 40 octets) dans le petit
`buffer[54]` de `main` → la pile déborde et on réécrit l'**adresse de retour**.
Comme la place est réduite, on logera le **shellcode dans une variable
d'environnement** et on fera pointer l'adresse de retour dessus.

## 3. Exploitation

```sh
# Les commandes exactes qui déclenchent l'exploit.
```

Le mot de passe récupéré est à reporter dans le fichier `flag`.
