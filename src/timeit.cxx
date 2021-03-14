/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <yaal/hcore/hfile.hxx>
M_VCSID( "$Id: " __ID__ " $" )
M_VCSID( "$Id: " __TID__ " $" )
#include "timeit.hxx"
#include "setup.hxx"

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;

namespace huginn {

bool timeit( HHuginn& huginn_, time::duration_t& preciseTime_, int& runs_ ) {
	bool ok( true );
	if ( ! setup._timeitRepeats ) {
		ok = huginn_.execute();
	} else {
		int repeats( *setup._timeitRepeats );
		while ( ok && ( runs_ < repeats ) ) {
			ok = huginn_.execute();
			preciseTime_ += huginn_.execution_time();
			++ runs_;
		}
		if ( ! ok ) {
			-- runs_;
		}
	}
	return ok;
}

void report_timeit(
	yaal::hcore::time::duration_t const& huginn_,
	yaal::hcore::time::duration_t const& load_,
	yaal::hcore::time::duration_t const& preprocess_,
	yaal::hcore::time::duration_t const& parse_,
	yaal::hcore::time::duration_t const& compile_,
	yaal::hcore::time::duration_t const& execute_,
	yaal::hcore::time::duration_t const& preciseTime_,
	int runs_
) {
	if ( !! setup._timeitRepeats && ( *setup._timeitRepeats > 0 ) ) {
		cerr << "Huginn time statistics:";
		if ( setup._verbose ) {
			cerr
				<< "\ninit:             " << lexical_cast<HString>( huginn_ )
				<< "\nload:             " << lexical_cast<HString>( load_ )
				<< "\npreprocess:       " << lexical_cast<HString>( preprocess_ )
				<< "\nparse:            " << lexical_cast<HString>( parse_ )
				<< "\ncompile:          " << lexical_cast<HString>( compile_ )
				<< "\nexecute overhead: " << lexical_cast<HString>( execute_ - preciseTime_ )
				<< "\nexecute:          " << lexical_cast<HString>( execute_ )
			;
		}
		if ( setup._quiet ) {
			cerr
				<< " " << *setup._timeitRepeats << " " << ( runs_ > 0 ? lexical_cast<HString>( ( preciseTime_ / runs_ ).get() ) : "not-executed" )
				<< " " << ( runs_ > 0 ? lexical_cast<HString>( preciseTime_.get() ) : "not-executed" ) << endl;
		} else {
			cerr
				<< "\nrepetitions:      " << *setup._timeitRepeats
				<< "\niteration time:   " << ( runs_ > 0 ? lexical_cast<HString>( preciseTime_ / runs_ ) : "not executed" )
				<< "\ncumulative time:  " << ( runs_ > 0 ? lexical_cast<HString>( preciseTime_ ) : "not executed" )
				<< endl;
		}
	}
}

}

