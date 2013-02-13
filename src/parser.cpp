#include "parser.h"
#include <stdlib.h>
#include <errno.h>

void Parser::ogetc_push(char c) {
  if (ogetc_c != 0)
    error("Internal error: pushback buffer is full in the parser.");
  if (c == '\n')
    linenum--;
  ogetc_c = c;
}

char Parser::ogetc() {
  char c;
  if (ogetc_c != 0) {
    c = ogetc_c;
    ogetc_c = 0;
  }
  else
    c = fgetc(f);
  if (c == '\n')
    linenum++;
  return c;
}

void Parser::consume_line() {
  while (fgetc(f) != '\n');
  linenum++;
}      

void Parser::error(const char *msg) {
  printf("Error on line %d: %s\n",linenum, msg);
  exit(1);
}

int Parser::isspace(char c) {
  return (c == ' ' || c == '\n' || c == '\t'); 
}

int Parser::isdigit(char c) {
 return (c >= '0' && c <= '9'); 
}

void Parser::eatws() {
  char c;
  while (isspace(c = ogetc()));
  ogetc_push(c);
}

int Parser::oint(bool neg, bool eat_follow) {
  errno = 0;
  if (!neg) {
    char c = ogetc();
    neg = (c == '-');
    if (!neg)
      ogetc_push(c);
  }
  char buf[1024];
  int i = 0;
  char c;
  while (isdigit(c = ogetc()) && i < 1024)
    buf[i++] = c;
  buf[i] = 0;
  int t = atoi(buf);
  if (!eat_follow)
    ogetc_push(c);
  if (errno)
    error("Overflow or underflow occurred reading an integer.");
  return neg ? -t : t;
}

int *Parser::clause() {
  int i = 0;
  while(true) {
    eatws();
    if ((buf[i++] = oint()) == 0) {
      int *c = new int[i];
      for (int j = 0; j < i; j++)
	c[j] = buf[j];
      return c;
    }
    if (i == BUFLEN)
      error("Maximum clause length exceeded.");
  }
  return 0; // never reached
}

void Parser::print(FILE *o, int *clause) {
  int i;
  while((i = *clause++))
    fprintf(o," %d", i);
  fprintf(o, " 0\n");
}

void Parser::print(FILE *o, int **bench) {
  int *c;
  while((c = *bench++))
    print(o,c);
}

int **Parser::sat_benchmark(int &num_vars, int &num_cl) {
  char c;
  while ((c = ogetc()) == 'c')
    consume_line();
  if (c != 'p')
    error("Expected 'p' line.");
  eatws();
  if (ogetc() != 'c' || ogetc() != 'n' || ogetc() != 'f')
    error("Expected \"cnf\" in 'p' line.");
  eatws();
  num_vars = oint(); 
  eatws();
  num_cl = oint();
  int **clauses = new int *[num_cl+1];
  int i;
  for (i = 0; i < num_cl; i++)
    clauses[i] = clause();
  clauses[i] = 0;
  return clauses;
}

int* Parser::parse_clause()
{
	eatws();
	return clause();
}
