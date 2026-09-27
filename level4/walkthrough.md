# level4 — walkthrough

## Goal

Read the `.pass` file of the next user to obtain its password.

## 1. Recon

```sh
ls -la
cat /var/mail/level4
find / -user <next> 2>/dev/null
find / -perm -4000 2>/dev/null
```

## 2. Analysis

Describe the binary, the vulnerability and why it is exploitable.

## 3. Exploitation

```sh
# The exact commands used to trigger the exploit.
```

## 4. Result

The recovered password for the next level.

```
Password: <fill in>
```
