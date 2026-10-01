# Rainfall

## Introduction

Rainfall est un projet cybersécurité de l'école 42. Il se
présente comme une série de niveaux (`level0`, `level1`, …) à l'intérieur d'une
machine virtuelle. À chaque niveau, on se connecte avec un compte, on cherche une
faille qui donne le mot de passe du niveau suivant, et on progresse ainsi de
niveau en niveau. Le but est d'apprendre à repérer et comprendre différentes
techniques d'exploitation.

---

## Prérequis

- [VirtualBox](https://www.virtualbox.org/) installé sur ta machine (l'hôte).
- Le fichier ISO du projet Snow Crash (fourni par le sujet).
- Un client SSH (déjà présent par défaut sur Linux et macOS).

---

## 1. Créer et configurer la VM

### a. Créer la machine

1. Ouvre VirtualBox → **New**.
2. Donne un nom (ex. `rainfall`).
3. Monter le fichier ISO téléchargé.
4. Type **Linux**, version **Ubuntu (64-bit)**
5. Mémoire : 1024–2048 Mo suffisent.
6. Termine la création.

### b. Réseau : le port forwarding

Dans **Settings** → **Network** → **Adapter 1** →
**Port Forwarding**, ajoute une règle :

| Nom | Protocole | IP hôte     | Port hôte | IP invité | Port invité |
|-----|-----------|-------------|-----------|-----------|-------------|
| ssh | TCP       | `127.0.0.1` | `4242`    | *(vide)*  | `4242`      |

---

## 2. Se connecter en SSH depuis l'hôte

Le premier compte est `level0`, avec le mot de passe `level0`.

Depuis un terminal **sur ta machine hôte** (pas dans la fenêtre de la VM) :

```sh
ssh level0@127.0.0.1 -p 4242
```

À la demande de mot de passe, entre :

```
level0
```

Une fois connecté, l'invite devient quelque chose comme
`level0@RainFall:~$`. Tu es dans la VM, prêt à commencer le premier niveau.

## 3. Approches

Pour sortir un binaire de la VM vers la machine hôte:
```bash
scp -P 4242 levelx@127.0.0.1:/home/user/levelx/levelx ~/Documents/42/rainfall/levelx/levelx.bin
```

Lire l'assembleur de main:
```bash
gdb ./levelx
disas main
```

Inspecter les fonctions cachées :

```bash
info functions
disas <fonction>
```

## 4. Outils

- [dogbolt](https://dogbolt.org/)
