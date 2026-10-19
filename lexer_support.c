#include "lexer.h"

char parse_char_escape(const char *str) {
  if (str[1] != '\\') {
    return str[1];
  }
  switch (str[2]) {
  case 'n': return '\n';
  case 't': return '\t';
  case 'r': return '\r';
  case '\\': return '\\';
  case '\'': return '\'';
  case '0': return '\0';
  default: return str[2];
  }
}
