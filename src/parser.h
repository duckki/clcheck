#ifndef clsat__parser_h
#define clsat__parser_h

#include <stdio.h>

class Parser {
protected:
    static const int BUFLEN = 32768;
    FILE *f;
    int linenum;
    char ogetc_c;
    int buf[BUFLEN];
    
    char ogetc(); // read char
    void ogetc_push(char c); // push back a char we read
    int oint(bool neg = false, bool eat_follow = true); // read int
    int *clause();
    
    void eatws(); // consume whitespace
    void consume_line(); // read until end of line
    
    void error(const char *msg);
    
    int isspace(char c);
    int isdigit(char c);
    
    // print a clause
    void print(FILE *o, int *clause);
    
public:
    Parser(FILE *_f) : f(_f), linenum(1), ogetc_c(0), buf() {}
    
    // parse a benchmark
    int **sat_benchmark(int &num_vars, int &num_cl);
    
    // print a SAT benchmark
    void print(FILE *o, int **bench);
	
	// RUP support
    int* parse_clause();
};

#endif
