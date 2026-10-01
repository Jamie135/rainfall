#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

struct s {
    int   value;    // offset 0
    char *buffer;   // offset 4
};

char c[68];         // buffer global (0x8049960), taille 0x44 = 68

void m(void)
{
    time_t t = time(NULL);
    printf("%s\n- %d\n", c, t);
}

int main(int argc, char **argv)
{
    struct s *A = malloc(sizeof(struct s));
    A->value  = 1;
    A->buffer = malloc(8);

    struct s *B = malloc(sizeof(struct s));
    B->value  = 2;
    B->buffer = malloc(8);

    strcpy(A->buffer, argv[1]);
    strcpy(B->buffer, argv[2]);

    FILE *f = fopen("/home/user/level8/.pass", "r");
    fgets(c, 68, f);

    puts("~~");

    return 0;
}