#include "solver.h"
#include <stdarg.h>
#include <set>
#include <map>
#include <iostream>
#include <list>
#include <stdlib.h>

using namespace std;

int eprintf( const char* fmt, ... )
{
	va_list	pvar;
	int		rval;
	va_start( pvar, fmt );
	rval = vfprintf( stderr, fmt, pvar );
	va_end( pvar );
	return rval;
}

/* Some debug macros
	These macros only affect debug version of clsat.
	The debug version checks ASSERTed expression and if it is evaluated false,
	the program prints an assertion failed message and terminates.
	ASSERTE macro accepts a descriptive message about the asserted expression
	as well.
	And --debug command-line option turns on printing additional
	trace messages.
*/
#ifdef	DEBUG
#define	ASSERT( x )	if( !(x) ) { fprintf( stderr, "assertion faild at %s(%d) : %s\n", __FILE__, __LINE__, #x ); abort(); }
#define	ASSERTE( x, s )	if( !(x) ) { fprintf( stderr, "assertion faild at %s(%d) : %s -> %s\n", __FILE__, __LINE__, #x, s ); abort(); }
#define	TRACE	(!debugMode)? 0: eprintf
#define	TRACE_CLAUSE( c )	((!debugMode)? 0: print_readable_clause( stderr, c ))
//#define	PROGRESS			TRACE
#define	PROGRESS		(!verboseMode || debugMode)? 0: eprintf
//#define	PROGRESS		eprintf
#else
#define	ASSERT( x )
#define	ASSERTE( x, s )
#define	TRACE(...)
#define	TRACE_CLAUSE( c )
#define	PROGRESS		(!verboseMode)? 0: eprintf
//#define	PROGRESS		eprintf
#endif

#ifdef	DEBUG
static int print_readable_clause( FILE *f, int *c )
{
	const int*	it = c;				// iterator for the clause
	bool		is_first = true;
	for( ; *it!=0; it++ )	// for each literal
	{
		if( is_first )
			is_first = false;
		else
			fprintf( f, " " );
		fprintf( f, "%d", *it );
	}
	return 0;
}
#endif


//////////////////////////////////////////////////////////////////////////////
// public interface

Solver::Solver( unsigned _num_vars )
{
	num_vars = _num_vars;

	debugMode = false;
	verboseMode = false;

	pa = new char[num_vars+1];
	fill( pa, pa+num_vars+1, UN );

	why = new Clause[num_vars+1];
	fill( why, why+num_vars+1, (Clause)NULL );

	dls = new int[num_vars+1];
	dl = 0;

	assignHistory = new Literal[num_vars+1];	// including null terminator
	assignHistoryEnd = assignHistory;
	
	numAssignments = 0;
	numConflicts = 0;

	initWatch();
}

Solver::~Solver()
{
}


//////////////////////////////////////////////////////////////////////////////
// watched literals

inline bool isUnassigned( const char* pa, Literal lit ) {
	int	var = abs(lit);
	return pa[var] == UN;
}
inline bool isSatisfied( const char* pa, Literal lit ) {
	int	var = abs(lit);
	return (pa[var] == getSign(lit));
}
inline bool isFalsified( const char* pa, Literal lit ) {
	int	var = abs(lit);
	return (pa[var] == getNegSign(lit));
}

void Solver::_addWatchedLiteral( Literal l, Clause c )
{
	int			var = abs( l );
	WatchList&	wl = l>0? posLitWatches[var]: negLitWatches[var];
	wl.push_back( c );
}

void Solver::_removeWatchedLiteral( Literal l, Clause c )
{
	int			var = abs( l );
	WatchList&	wl = l>0? posLitWatches[var]: negLitWatches[var];
	for( WatchList::iterator it=wl.begin(); it!=wl.end(); it++ )
	{
		if( *it == c ) {
			//wl.erase( it );
			*it = wl.back(), wl.pop_back();	// faster
			return;
		}
	}
	abort();
}

void Solver::_addImpLiterals( Literal one, Literal the_other, Clause c )
{
	int			v1 = abs( one );
	int			v2 = abs( the_other );
	ImpList&	l1 = one>0? posImpLists[v1]: negImpLists[v1];
	ImpList&	l2 = the_other>0? posImpLists[v2]: negImpLists[v2];
	l1.push_back( make_pair(c,the_other) );
	l2.push_back( make_pair(c,one) );
}

void Solver::_addWatchedClause( Clause c )
{
	// assume the clause is not a empty or unit clause
	// assume the clause has no duplicated lits

	// watched lits should be the first two of the clause
	// 1. to find fast the other lit for given lit
	// 2. to find fast the watch lists for given clause
	// this scheme may be replaced with a better one

	// rearrange the clause to make unassigned lits come first
	Literal*	it_unassigned = NULL;
	int			len = 0;
	bool		have_two_unassigned = false;
	for( Literal* it=c; *it; it++ )
	{
		len++;
		if( isUnassigned(pa,*it) )
		{
			if( it_unassigned != NULL ) {	// we have found two unassigned lits
				swap( c[1], *it );
				have_two_unassigned = true;
				break;
			}
			swap( c[0], *it );
			it_unassigned = it;
		}
	}

	ASSERTE( len > 1, "the clause shoulde be longer than 1" );
	ASSERTE( it_unassigned != NULL,
			"the clause shoudle be non-empty at the current level" );

	// if one of the watched literals is falsified, we prefer highter level.
	// find the literal set at the maximum level
	if( !have_two_unassigned )
	{
		// we can assume the rest of the clause is falsified
		int			max_level = -1;
		Literal*	max_it = NULL;
		for( Literal* it=c+1; *it; it++ )
		{
			int	var = abs( *it );
			if( dls[var] > max_level ) {
				max_level = dls[var];
				max_it = it;
			}
		}
		ASSERT( max_it != NULL );
		swap( c[1], *max_it );
	}

	if( c[2] == 0 )	// see if it is a binary clause
	{
		_addImpLiterals( c[0], c[1], c );
	}
	else
	{
		_addWatchedLiteral( c[0], c );	// should be unassiged
		_addWatchedLiteral( c[1], c );	// unassigned or falsified
	}
}

bool _removeDuplictedLiterals( Clause c )
{
	set<Literal>	lits;
	size_t			n = 0;
	for( Literal* it=c; *it; it++, n++ ) {
		if( lits.find(-(*it)) != lits.end() )	// see if tautology
			return false;
		lits.insert( *it );
	}
	copy( lits.begin(), lits.end(), c );
	c[lits.size()] = 0;	// null-terminate
	return true;
}

void Solver::initWatch()
{
	posLitWatches = new WatchList[num_vars+1];	// [0] is not used
	negLitWatches = new WatchList[num_vars+1];
	posImpLists = new ImpList[num_vars+1];	// [0] is not used
	negImpLists = new ImpList[num_vars+1];
}

void Solver::addNewWatchedClause( Clause c )
{
	_addWatchedClause( c );
}

Clause Solver::propagateLiteral( Literal l, Literal* output_it )
{
	Literal	falsified = -l;
	int		var = abs( falsified );

	// for implication list
	ImpList&	il = (falsified > 0)? posImpLists[var]: negImpLists[var];
	for( unsigned index=0; index<il.size(); index++ )
	{
		Clause	c = il[index].first;
		Literal	the_other = il[index].second;
		char	val = pa[abs(the_other)];
		if( val == getSign(the_other) )	// SAT -> ignore
			continue;
		if( val == UN )	// the other is free
		{
			// we got an unit clause
			int	unit_var = abs( the_other );
			if( why[unit_var] == NULL ) {	// see if it's not in the pipeline
				why[unit_var] = c;
				*output_it++ = the_other;
			}
			continue;
		}
		else	// the other is falsified
		{
			// no unassigned lits -> conflict
			*output_it = 0;
			return c;
		}
	}

	// for general watched list
	WatchList&	wl = (falsified > 0)? posLitWatches[var]: negLitWatches[var];
	for( unsigned index=0; index<wl.size(); index++ )
	{
		Clause	c = wl[index];
		Literal	the_other = (c[0] == falsified)? c[1]: c[0];
		// case of the_other: UN / SAT / FAL
		char	val = pa[abs(the_other)];
		if( val == getSign(the_other) )	// SAT -> ignore
			continue;
		if( val == UN )	// the other is free
		{
			// let's find another unassinged lit in the clause
			Literal		unassigned = 0;
			Literal*	it2 = c + 2;
			for( int l; (l=*it2); it2++ )
			{
				char	val = pa[abs(l)];
				if( val == getSign(l) )
					break;
				if( val == UN ) {
					unassigned = *it2;
					break;
				}
			}
			if( *it2 == 0 )	// if we got a unit clause
			{
				int	unit_var = abs( the_other );
				if( why[unit_var] == NULL ) {	// see if it's not in the pipeline
					why[unit_var] = c;
					*output_it++ = the_other;
				}
				continue;
			}
			if( unassigned == 0 )	// we got a satisfied clause
				continue;

			// found another unassigned -> undetermined
			// update watch lists & rearrange the clause
			//_removeWatchedLiteral( falsified, cn );	// unsafe
			//wl.erase( wl.begin()+index ); index--;	// slow
			wl[index--] = wl.back(), wl.pop_back();		// fast remove

			_addWatchedLiteral( *it2, c );
			if( c[0] == falsified )
				swap( c[0], *it2 );
			else
				swap( c[1], *it2 );
		}
		else	// the other is falsified
		{
			// let's find an unassinged lit in the clause
			Literal*	it2 = c + 2;
			for( ; *it2; it2++ ) {
				if( isSatisfied(pa,*it2) )
					break;
				if( isUnassigned(pa,*it2) )
					break;
			}
			if( *it2 == 0 ) {	// no unassigned lits -> conflict
				*output_it = 0;
				return c;
			}
			if( isSatisfied(pa,*it2) )	// we got a satisfied clause
				continue;

			// yes, we got one
			//_removeWatchedLiteral( falsified, cn );	// unsafe
			//wl.erase( wl.begin()+index ); index--;	// slow
			wl[index--] = wl.back(), wl.pop_back();		// fast remove
			_addWatchedLiteral( *it2, c );
			if( c[0] == falsified )
				swap( c[0], *it2 );
			else
				swap( c[1], *it2 );

			// try to find one more
			for( ; *it2; it2++ ) {
				if( isSatisfied(pa,*it2) )
					break;
				if( isUnassigned(pa,*it2) )
					break;
			}
			if( *it2 == 0 )	// if we got a unit clause
			{
				int	unit_var = abs( c[0] );
				if( why[unit_var] == NULL ) {	// see if it's not in the pipeline
					why[unit_var] = c;
					*output_it++ = c[0];
				}
				continue;
			}
			if( isSatisfied(pa,*it2) )	// we got a satisfied clause
				continue;

			// two unassigned lits -> undetermined
			_removeWatchedLiteral( the_other, c );
			_addWatchedLiteral( *it2, c );
			if( c[0] == the_other )
				swap( c[0], *it2 );
			else
				swap( c[1], *it2 );
		}
	}
	*output_it = 0;
	return NULL;	// no conflict
}


//////////////////////////////////////////////////////////////////////////////
// assertion & backtracking

void Solver::_cancelAssignments()
{
	TRACE( "  cancel assignment: " );
	int*	it = assignHistoryEnd;
	for( ; it!=assignHistory; it-- )
	{
		int	l = *(it-1);	// look ahead
		int	v = abs( l );
		if( dls[v] <= dl )	// see if it's time to stop
			break;
		pa[v] = UN;
		why[v] = NULL;
		TRACE( "%d ", l );
	}
	assignHistoryEnd = it;	// reset stack top
	TRACE( "\n" );
}

Clause Solver::_assertLiteral( Literal l, Clause reason )
{
   	int	v = abs( l );
	pa[v] = getSign( l );
	why[v] = reason;
	dls[v] = dl;
	*assignHistoryEnd++ = l;	// put on top of the stack
	numAssignments++;

	Literal*	it = assignHistoryEnd-1;	// propagation start
	Literal*	end = assignHistoryEnd;		// propagation end
	for( int pass=1; ; pass++ )
	{
		TRACE( "  unit prop pass %d\n", pass );
		// propagate all of last assignments
		for( ; it!=assignHistoryEnd; it++ )
		{
			Clause conflict = propagateLiteral( *it, end );
			if( conflict != NULL )
			{
				// cancel why assignments
				for( Literal* it=assignHistoryEnd; *it; it++ ) {
					int	l = *it;
					int	v = abs( l );
					why[v] = NULL;
				}
				return conflict;
			}
			for( ; *end; end++ ) ;	// seek end ptr
		}
		if( *assignHistoryEnd == 0 ) {	// no more unit clauses
			//TRACE( "  no unit clauses\n" );
			break;
		}

		for( int l; (l=*assignHistoryEnd); assignHistoryEnd++ ) {
			int	v = abs( l );
			pa[v] = getSign( l );
			dls[v] = dl;
			numAssignments++;
			TRACE( "   assign by UP: %d for #%d\n", l, why[v] );
		}
	}
	return NULL;
}

bool Solver::assertLiteral( Literal l, Clause reason, Clause& cc )
{
	if( reason == NULL )
		TRACE( "  assign by hypothesis: %d\n", l );
	else
		TRACE( "  assign by assertion/check: %d\n", l );
	Clause result = _assertLiteral( l, reason );
	if( result != NULL ) // if conflicting
	{
		TRACE( "    detected a conflict with #%d\n", result );
		numConflicts++;
		cc = result;
		return false;
	}
	return true;
}

void Solver::backjump( int new_dl )
{
	TRACE( "backtrack to level %d\n", new_dl );
	dl = new_dl;
	_cancelAssignments();
}


//////////////////////////////////////////////////////////////////////////////
// the main interface

bool Solver::checkSat( Clause c ) {
	for( int* it=c; *it!=0; it++ )
	{
		if( pa[abs(*it)] == getSign(*it) )
			return true;
	}
	return false;
}

int Solver::countFreeLits( Clause c, Literal& lit ) {
	lit = 0;
	int	num_free = 0;
	for( int* it=c; *it!=0; it++ )
	{
		int	v = abs(*it);
		if( pa[v] == UN ) {
			if( num_free == 0 )	// first free lit
				lit = *it;
			num_free++;
		}
	}
	return num_free;
}

void Solver::learn( Clause c )
{
	// assume: c is not satisfied under the current assignment
	TRACE( "  learn new clause: " );
	TRACE_CLAUSE( c );
	TRACE( "\n" );
	
	Literal	lit;
	int		num_free;
	num_free = countFreeLits( c, lit );
	
	if( num_free == 0 ) {	// if empty
		TRACE( "  observed contradiction\n" );
	}
	else if( num_free == 1 )	// if unit
	{
		Clause	cc;
		bool	ok = assertLiteral( lit, c, cc );
		if( !ok ) {
			TRACE( "  observed contradiction\n" );
		}
	}
	else {
		// add the new clause into watched lists
		addNewWatchedClause( c );
	}
}

bool Solver::hypothesize( Clause c )
{
	TRACE( "DECISION LEVEL: hypothesis\n" );

	for( Literal lit; (lit=*c); c++ )	// for each literal in c
	{
		if( pa[abs(lit)] == UN ) {
			Clause	cc;	// Conflict clause
			bool	ok = assertLiteral( -lit, NULL, cc );
			if( !ok )	// conflicting
				return false;
		}
		else if( pa[abs(lit)] == getSign(lit) )	// hypothesis would conflict
			return false;
	}
	return true;
}

void Solver::assert( Clause c )
{
	if( checkSat( c ) )
		return;
	learn( c );
}

bool Solver::check( Clause c )
{
	dl = 1;

	bool	ok = hypothesize( c );
	if( ok )	// if SAT, c is not provable by UIP
		return false;
	
	backjump( 0 );
	learn( c );
	return true;
}


//////////////////////////////////////////////////////////////////////////////
// printing stuff

void Solver::printStat( FILE* o )
{
	fprintf( o, "%d assignments, %d conflicts\n",
			 numAssignments, numConflicts );
}
