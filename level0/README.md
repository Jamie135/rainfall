# level0

## Objectif

Obtenir un shell en tant que `level1` et lire `/home/user/level1/.pass`.

## 1. Reconnaissance

```sh
level0@RainFall:~$ ls -l
-rwsr-x---+ 1 level1 users 747441 ... level0
```

Le binaire `level0` est **setuid `level1`** (bit `s`). Lancé par `level0`, il
s'exécute avec l'uid *effectif* `level1` mais l'uid *réel* `level0`.

## 2. Analyse

Désassemblage de `main` (`objdump -d ./level0` / `gdb disas main`) :

```asm
mov    eax, [ebp+0xc]        ; argv
add    eax, 0x4              ; &argv[1]
mov    eax, [eax]            ; argv[1]
call   atoi                  ; atoi(argv[1])
cmp    eax, 0x1a7            ; 0x1a7 = 423
jne    <else>                ; si != 423 -> "No !"
...
call   __setresgid           ; setresgid(egid, egid, egid)
call   __setresuid           ; setresuid(euid, euid, euid)
lea    eax, [esp+0x10]        ; args = { "/bin/sh", NULL }
call   execv                 ; execv("/bin/sh", args)
```

Chaînes référencées :
- `0x80c5348` → `"/bin/sh"`
- `0x80c5350` → `"No !\n"`

Logique reconstruite (voir `source`) :

```c
if (atoi(argv[1]) == 423) {
    char *args[2];
    args[0] = strdup("/bin/sh");
    args[1] = NULL;
    setresgid(getegid(), getegid(), getegid());
    setresuid(geteuid(), geteuid(), geteuid());
    execv("/bin/sh", args);
} else
    fwrite("No !\n", 1, 5, stderr);
```

### Explication ligne par ligne

- **`atoi(argv[1]) == 423`**
  `argv[1]` est le premier argument passé au programme. `atoi` le convertit en
  entier, comparé à `423` (constante `0x1a7` vue dans le désassemblage). C'est le
  seul « mot de passe » attendu : tout le bloc privilégié est derrière cette
  condition. Passer autre chose (ou rien) tombe dans le `else`.

- **`args[0] = strdup("/bin/sh"); args[1] = NULL;`**
  Construction du tableau d'arguments pour `execv`. Convention : `args[0]` = nom
  du programme lancé, et le tableau doit se terminer par `NULL`. On prépare donc
  le lancement de `/bin/sh`.

- **`setresgid(getegid(), getegid(), getegid())`**
  `getegid()` = gid *effectif* (celui hérité du setuid, côté groupe). `setresgid`
  fixe les trois gids (réel, effectif, sauvegardé) à cette valeur : le gid réel
  est aligné sur l'effectif.

- **`setresuid(geteuid(), geteuid(), geteuid())`** — **la ligne décisive**
  `geteuid()` = uid *effectif* = `level1` (grâce au bit setuid). `setresuid` met
  les trois uids (réel, effectif, sauvegardé) à `level1`. Sans cet appel, un
  shell lancé garderait souvent l'uid réel `level0` et perdrait le privilège
  (bash/dash abaissent l'euid s'il diffère de l'uid réel). En égalisant les deux,
  le programme **verrouille** l'identité `level1` pour tout ce qui suit.

- **`execv("/bin/sh", args)`**
  Remplace le processus courant par `/bin/sh`. Comme les uids valent maintenant
  tous `level1`, ce shell tourne **réellement en `level1`** — d'où l'accès à
  `/home/user/level1/.pass`.

- **`fwrite("No !\n", 1, 5, stderr)`** (branche `else`)
  Si la condition n'est pas remplie, le programme écrit simplement `No !` sur la
  sortie d'erreur et se termine. `1` = taille d'un élément, `5` = nombre
  d'octets (`N`, `o`, ` `, `!`, `\n`).

**En résumé :** la seule condition à satisfaire est `argv[1] == "423"` ; le
binaire fait lui-même le travail de conservation de privilège (`setresuid`) puis
nous ouvre un shell `level1`. Pas de vraie « faille mémoire » ici — c'est une
mauvaise logique de contrôle d'accès (un shell privilégié caché derrière une
valeur magique).

## 3. Exploitation

```sh
level0@RainFall:~$ ./level0 423
$ id
uid=2021(level1) gid=100(users) ...
$ cat /home/user/level1/.pass
<mot de passe de level1>
```

Le mot de passe récupéré est à reporter dans le fichier `flag`.
