#include <cstring>
#include <cstdlib>
#include <unistd.h>

class N {
private:
    char annotation[100];   // offset 4 (après la vtable à l'offset 0)
    int  number;            // offset 0x68

public:
    N(int n) {number = n;}

    void setAnnotation(char *s) {
        int len = strlen(s);
        memcpy(annotation, s, len);
    }

    virtual int operator+(N &other) {return number + other.number;}

    virtual int operator-(N &other) {return number - other.number;}
};

int main(int argc, char **argv)
{
    if (argc <= 1)
        _exit(1);

    N *a = new N(5);
    N *b = new N(6);

    a->setAnnotation(argv[1]);

    *b + *a;

    return 0;
}