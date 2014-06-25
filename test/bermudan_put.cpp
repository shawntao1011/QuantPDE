////////////////////////////////////////////////////////////////////////////////
// bermudan_put.cpp
// ----------------
//
// Computes the price of a Bermudan put with exercise occuring yearly.
//
// Author: Parsiad Azimzadeh
////////////////////////////////////////////////////////////////////////////////

#include <QuantPDE/Core>
#include <QuantPDE/Modules/Payoffs>

// TODO: Change these includes; shouldn't include src directory explicitly
#include <QuantPDE/src/Modules/BlackScholes.hpp>

using namespace QuantPDE;
using namespace QuantPDE::Modules;

///////////////////////////////////////////////////////////////////////////////

#include <iostream>  // cout, cerr
#include <unistd.h>  // getopt

using namespace std;

///////////////////////////////////////////////////////////////////////////////

/**
 * Prints help to stderr.
 */
void help() {
	cerr <<
"unequal_borrowing_lending_rates [OPTIONS]" << endl << endl <<
"Prices a long/short position straddle under the Black-Scholes model assuming" << endl <<
"unequal borrowing/lending rates." << endl <<
endl <<
"-d REAL" << endl <<
endl <<
"    sets the dividend rate (default is 0.)" << endl <<
endl <<
"-e NONNEGATIVE_INTEGER" << endl <<
endl <<
"    sets the number of premature exercises, spread evenly throughout the" << endl <<
"    interval (default is 10)" << endl <<
endl <<
"-K REAL" << endl <<
endl <<
"    sets the strike price (default is 100.)" << endl <<
endl <<
"-N POSITIVE_INTEGER" << endl <<
endl <<
"    sets the number of steps to take in time (default is 100)" << endl <<
endl <<
"-r REAL" << endl <<
endl <<
"    sets the interest rate (default is 0.04)" << endl <<
endl <<
"-R NONNEGATIVE_INTEGER" << endl <<
endl <<
"    controls the coarseness of the grid, with 0 being coarsest (default is 0)" << endl <<
endl <<
"-T POSITIVE_REAL" << endl <<
endl <<
"    sets the expiry time (default is 1.)" << endl <<
endl <<
"-v REAL" << endl <<
endl <<
"    sets the volatility (default is 0.2)" << endl << endl;
}

int main(int argc, char **argv) {

	Real K = 100.;
	Real T = 1.;
	Real r = 0.04;
	Real v = 0.2;
	Real q = 0.;

	int R = 0;
	int e = 10;
	int N = 25;

	// Setting options with getopt
	{ char c;
	while((c = getopt(argc, argv, "d:e:hK:N:r:R:T:v:")) != -1) {
		switch(c) {
			case 'd':
				q = atof(optarg);
				break;
			case 'e':
				e = atoi(optarg);
				if(e < 0) {
					cerr <<
"error: the number of premature exercises must be nonnegative" << endl;
					return 1;
				}
				break;
			case 'h':
				help();
				return 0;
			case 'K':
				K = atof(optarg);
				break;
			case 'N':
				N = atoi(optarg);
				if(N <= 0) {
					cerr <<
"error: the number of steps must be positive" << endl;
					return 1;
				}
				break;
			case 'r':
				r = atof(optarg);
				break;
			case 'R':
				R = atoi(optarg);
				if(R < 0) {
					cerr <<
"error: the maximum level of refinement must be nonnegative" << endl;
					return 1;
				}
				break;
			case 'T':
				if((T = atof(optarg)) <= 0.) {
					cerr <<
"error: expiry time must be positive" << endl;
					return 1;
				}
				break;
			case 'v':
				v = atof(optarg);
				break;
			case ':':
			case '?':
				cerr << endl;
				help();
				return 1;
		}
	} }

	////////////////////////////////////////////////////////////////////////
	// Spatial grid
	////////////////////////////////////////////////////////////////////////

	RectilinearGrid1 grid(
		Axis {
			0., 10., 20., 30., 40., 50., 60., 70.,
			75., 80.,
			84., 88., 92.,
			94., 96., 98., 100., 102., 104., 106., 108., 110.,
			114., 118.,
			123.,
			130., 140., 150.,
			175.,
			225.,
			300.,
			750.,
			2000.,
			10000.
		}
	);

	// Refine grid
	for(int i = 0; i < R; i++) {
		grid.refine( RectilinearGrid1::NewTickBetweenEachPair() );
	}

	////////////////////////////////////////////////////////////////////////
	// Payoff
	// ------
	//
	// Payoffs are lambda functions. The following is just a macro that
	// expands to
	//
	// auto payoff = [K] (Real S) {
	// 	return S < K ? K - S : 0.;
	// };
	////////////////////////////////////////////////////////////////////////

	auto payoff = QUANT_PDE_MODULES_PAYOFFS_PUT_FIXED_STRIKE( K );

	////////////////////////////////////////////////////////////////////////
	// Iteration tree
	// --------------
	//
	// Sets up the loop structure:
	// for(int n = 0; n < N; n++) {
	// 	// Solve a linear system
	// }
	////////////////////////////////////////////////////////////////////////

	ReverseConstantStepper::Factory factory(N);
	ReverseEventIteration1 stepper(
		0., // Initial time
		T,  // Expiry time
		factory
	);

	////////////////////////////////////////////////////////////////////////
	// Exercise events
	////////////////////////////////////////////////////////////////////////

	for(unsigned m = 0; m < e; m++) {
		stepper.add(
			// Time at which the event takes place
			T / e * m,

			// Take the maximum of the continuation and exercise
			// values
			[K] (const Interpolant1 &V, Real S) {
				return max( V(S) , K - S );
			},

			// Spatial grid to interpolate on
			grid
		);
	}

	////////////////////////////////////////////////////////////////////////
	// Linear system tree
	// ------------------
	//
	// Makes the linear system to solve at each iteration
	////////////////////////////////////////////////////////////////////////

	BlackScholes bs(
		grid,

		r, // Interest
		v, // Volatility
		q  // Dividend rate
	);

	ReverseLinearBDFTwo bdf2(grid, bs);
	bdf2.setIteration(stepper);

	////////////////////////////////////////////////////////////////////////
	// Running
	// -------
	//
	// Everything prior to this was setup. Now we run the method.
	////////////////////////////////////////////////////////////////////////

	BiCGSTABSolver solver;

	auto V = stepper.solve(
		grid,   // Domain
		payoff, // Initial condition
		bdf2,   // Root of linear system tree
		solver  // Linear system solver
	);

	////////////////////////////////////////////////////////////////////////
	// Print solution
	////////////////////////////////////////////////////////////////////////

	RectilinearGrid1 printGrid( Axis::range(0., 10., 200.) );
	cout << printGrid.accessor( printGrid.image( V ) ) << endl;

	return 0;

}