#include <string.h>
#include <stdio.h>
#include <unistd.h>

void p(char *a, char *b)
{
  char *retaddr;
  char buffer[4096];
  
  puts(b);
  read(0, buffer, 4096);
  retaddr = strchr(buffer, '\n');
  *retaddr = '\0';
  strncpy(a, buffer, 20);
  return;
}

void pp(char *buffer)
{
  char buffer1 [20];
  char buffer2 [20];
  unsigned int len;

  p(buffer1, " - ");
  p(buffer2, " - ");
  strcpy(buffer, buffer1);
  len = strlen(buffer);
  strcat(buffer, " ");
  strcat(buffer, buffer2);
  return;
}

int main(void)
{
  char buffer[42];
  
  pp(buffer);
  puts(buffer);
  return 0;
}
