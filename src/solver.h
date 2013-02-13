#ifndef solver__h
#define solver__h

#include <stdio.h>
#include <vector>

using namespace std;

typedef int			Literal;
typedef Literal*	Clause;

// codes for partial assignment data (stored in a char array)
#define UN ((char)1)	// unassigned
#define TT ((char)0)	// true
#define FF ((char)-1)	// false

inline char getSign( Literal l ) { return l >> 31; }
inline char getNegSign( Literal l ) { return ~getSign(l); }

class Solver
{
public:	// public interface
	Solver( unsigned _num_vars );
	~Solver();

	void setDebugMode( bool flag=true ) { debugMode = flag; }
	void setVerboseMode( bool flag=true ) { verboseMode = flag; }

	void assert( Clause c );
	bool check( Clause c );
	
	void printStat( FILE* o );
	
protected:	// given settings
	unsigned	num_vars;

	// options
	bool	debugMode;
	bool	verboseMode;

protected:	// solver states
	// variable assignment states (be careful that variable zero is not used)
	char*	pa; // current partial assignment (values in UN,TT,FF)
	Clause*	why; /* why[i] = j if i's value was set by unitprop from clause j,
					and NULL if i was a decision var. */
	int*	dls; // dls[i] = l if i's value was set at decision level l.
	int		dl; // current decision level

	// history of current partial assignment
	Literal*	assignHistory;		// array of literals in assigned order
	Literal*	assignHistoryEnd;	// end marker (next to the last item)

	// stats
	unsigned int	numAssignments;
	unsigned int	numConflicts;

protected:	// watched literals
	typedef	vector<Clause>	WatchList;	// an array of clauses
	WatchList*	posLitWatches;	// WatchLists for each positive lit
	WatchList*	negLitWatches;	// WatchLists for each negative lit

	typedef vector< pair<Clause,Literal> >	ImpList;
	ImpList*	posImpLists;
	ImpList*	negImpLists;

	void _addWatchedLiteral( Literal l, Clause c );
	void _removeWatchedLiteral( Literal l, Clause c );
	void _addImpLiterals( Literal one, Literal the_other, Clause c );

	void _addWatchedClause( Clause c );

	void initWatch();
	void addNewWatchedClause( Clause c );
	Clause propagateLiteral( Literal l, Literal* output_it );

protected:	// assertion & backtracking
	// (after backtracking) cancels all assignments set above the current level
	void _cancelAssignments();

	/* if we detect a contradiction, return the falsified clause.
	 * Otherwise return NULL */

	// for original+learned clauses (watched-literal-driven)
	Clause _assertLiteral( Literal l, Clause reason );

	// assert and propagate
	bool assertLiteral( Literal l, Clause reason, Clause& cc );

	// back jumping with a new cluase and its proof
	void backjump( int new_dl );

protected:	// solver loop & utilities
	bool checkSat( Clause c );
	int countFreeLits( Clause c, Literal& lit );
	
	void learn( Clause cc );
	bool hypothesize( Clause c );
};

#endif
