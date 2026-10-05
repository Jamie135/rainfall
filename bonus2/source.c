#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int language = 0;

void greetuser(char *param)
{
  char buffer[72];

  switch (language)
  {
    case 1:
      strcpy(buffer, "Hyvää päivää ");
      break;
    case 2:
      strcpy(buffer, "Goedemiddag! ");
      break;
    case 0:
      strcpy(buffer, "Hello ");
  }
  strcat(buffer, param);
  puts(buffer);
}

int main(int argc, char **argv)
{
  char buffer[76];
  char *lang;

  if (argc == 3)
  {
    memset(buffer, 0, 76);
    strncpy(buffer, argv[1], 40);
    strncpy(buffer + 40, argv[2], 32);
    lang = getenv("LANG");
    if (lang != NULL)
    {
      if (memcmp(lang, "fi", 2) == 0)
        language = 1;
      else if (memcmp(lang, "nl", 2) == 0)
        language = 2;
    }
    greetuser(buffer);
  }
  else
    return 1;
  return 0;
}
