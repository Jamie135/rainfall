#include <unistd.h>
#include <stdio.h>
#include <string.h>

char *p()
{
  char buffer[64];
  const void *v2;
  unsigned int retaddr;

  fflush(stdout);
  gets(buffer);
  v2 = (const void *)retaddr;
  if ( (retaddr & 0xb0000000) == 0xb0000000 )
  {
    printf("(%p)\n", v2);
    _exit(1);
  }
  puts(buffer);
  return strdup(buffer);
}

void main(void)
{
  p();
  return;
}