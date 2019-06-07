/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <yaal/hcore/hfile.hxx>
#include <yaal/tools/huginn/integer.hxx>
M_VCSID( "$Id: " __ID__ " $" )
M_VCSID( "$Id: " __TID__ " $" )
#include "jupyter.hxx"
#include "linerunner.hxx"
#include "symbolicnames.hxx"
#include "meta.hxx"
#include "setup.hxx"

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;
using namespace yaal::tools::huginn;

namespace huginn {

int jupyter_session( void ) {
	M_PROLOG
	HLineRunner lr( "*huginn jupyter*" );
	int retVal( 0 );
	HString line;
	HString code;
	lr.load_session( setup._sessionDir + "/init", false );
	lr.load_session( setup._sessionDir + "/" + setup._session, true );
	while ( getline( cin, line ).good() ) {
		if ( line.find( "//?" ) == 0 ) {
			line.shift_left( 3 );
			char const* symbol( nullptr );
			if ( ! line.is_empty() && ( line.front() == '\\'_ycp ) ) {
				symbol = symbol_from_name( line );
				if ( symbol ) {
					cout << symbol << endl;
				} else {
					symbolic_names_t sn( symbol_name_completions( line ) );
					for ( yaal::hcore::HString const& n : sn ) {
						cout << n << endl;
					}
				}
			} else {
				HLineRunner::words_t const& words( ! line.is_empty() ? lr.dependent_symbols( line, false ) : lr.words( false ) );
				for ( HString const& w : words ) {
					cout << w << endl;
				}
			}
			cout << "// done" << endl;
		} else if ( meta( lr, line ) ) {
			/* Done in meta(). */
		} else if ( line == "//" ) {
			if ( lr.add_line( code, true ) ) {
				HHuginn::value_t res( lr.execute() );
				if ( !! res && lr.use_result() && ( res->type_id() == HHuginn::TYPE::INTEGER ) ) {
					retVal = static_cast<int>( static_cast<HInteger*>( res.raw() )->value() );
				} else {
					retVal = 0;
				}
				if ( !! res ) {
					if ( lr.use_result() ) {
						cout << to_string( res, lr.huginn() ) << endl;
					}
					cout << "// ok" << endl;
				} else {
					cout << lr.err() << endl;
					cout << "// error" << endl;
				}
			} else {
				cout << lr.err() << endl;
				cout << "// error" << endl;
			}
			code.clear();
		} else {
			if ( ! code.is_empty() ) {
				code.append( "\n" );
			}
			code.append( line );
		}
	}
	filesystem::create_directory( setup._sessionDir, 0700 );
	lr.save_session( setup._sessionDir + "/" + setup._session );
	return ( retVal );
	M_EPILOG
}

}

