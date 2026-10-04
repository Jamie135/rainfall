#include <string.h>
#include <stdio.h>
#include <unistd.h>

const char sep[] = " - ";

void p(char *a, char *b)
{
  char *retaddr;
  char buffer[4104];
  
  puts(b);
  read(0, buffer, 4104);
  retaddr = strchr(buffer,10);
  *retaddr = '\0';
  strncpy(a, buffer, 20);
  return;
}

void pp(char *buffer)
{
  char buffer1 [20];
  char buffer2 [20];
  unsigned int len;

  p(buffer1, sep);
  p(buffer2, sep);
  strcpy(buffer, buffer1);
  len = strlen(buffer);
  buffer[len] = " ";
  buffer[len + 1] = 0;
  strcat(buffer, buffer2);
  return;
}

int main(void)
{
  char buffer[54];
  
  pp(buffer);
  puts(buffer);
  return 0;
}
