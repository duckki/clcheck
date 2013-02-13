#include <iostream>
#include "parser.h"
#include "solver.h"

int do_rup( FILE* input_file, FILE* proof_file )
{
	Parser in(input_file);
    
	int num_vars, num_cl;
	int **cl;
    
	cl = in.sat_benchmark(num_vars, num_cl);
    
	// Constructing solver
	Solver* 	s = new Solver( num_vars );
    
	for( int** it=cl; *it; it++ )
 		s->assert( *it );

	Parser pf(proof_file);

	bool    success = false;
	
	while( !feof(proof_file) )
	{
		Clause	c;
		c = pf.parse_clause();
        //clog << "check" << endl;
        bool	ok = s->check( c );
        if( !ok )	// check failed
            break;
        if( c[0] == 0 ) {	// checked the empty clause
            success = true;
            break;
		}
	}
	if( success ) {
		cout << "OK" << endl;
		return 0;
	}
	else {
		cout << "FAIL" << endl;
		return 1;
	}
}

int main( int argc, char** argv )
{
    if( argc < 2 || argc > 3 ) {
        clog << "Invalid number of arguments" << endl;
        return 2;
    }
    FILE* pf;
    if( argc == 3 )
        pf = fopen( argv[2], "r" );
    else
        pf = stdin;
	return do_rup( fopen(argv[1],"r"), pf );
}
