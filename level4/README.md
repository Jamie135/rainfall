# level4

## Objectif

Lire le fichier `.pass` de l'utilisateur suivant pour obtenir son mot de passe.

## 1. Reconnaissance

```sh
ls -la
cat /var/mail/level4
find / -user <next> 2>/dev/null
find / -perm -4000 2>/dev/null
```

## 2. Analyse

Décrire le binaire, la vulnérabilité et pourquoi elle est exploitable.

## 3. Exploitation

```sh
# Les commandes exactes qui déclenchent l'exploit.
```

Le mot de passe récupéré est à reporter dans le fichier `flag`.
