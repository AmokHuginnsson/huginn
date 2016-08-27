/*
---           `huginn' 0.0.0 (c) 1978 by Marcin 'Amok' Konarski            ---

  jupyter.cxx - this file is integral part of `huginn' project.

  i.  You may not make any changes in Copyright information.
  ii. You must attach Copyright information to any part of every copy
      of this software.

Copyright:

 You can use this software free of charge and you can redistribute its binary
 package freely but:
  1. You are not allowed to use any part of sources of this software.
  2. You are not allowed to redistribute any part of sources of this software.
  3. You are not allowed to reverse engineer this software.
  4. If you want to distribute a binary package of this software you cannot
     demand any fees for it. You cannot even demand
     a return of cost of the media or distribution (CD for example).
  5. You cannot involve this software in any commercial activity (for example
     as a free add-on to paid software or newspaper).
 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. Use it at your own risk.
*/

#include <yaal/hcore/hfile.hxx>
M_VCSID( "$Id: " __ID__ " $" )
M_VCSID( "$Id: " __TID__ " $" )
#include "jupyter.hxx"
#include "linerunner.hxx"

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;
using namespace yaal::tools::huginn;

namespace huginn {

int jupyter_session( void ) {
	M_PROLOG
	HString line;
	HLineRunner ir( "*huginn jupyter*" );
	int retVal( 0 );
	while ( getline( cin, line ).good() ) {
		if ( line == "//?" ) {
			HLineRunner::words_t const& words( ir.words() );
			for ( HString const& w : words ) {
				cout << w << endl;
			}
			cout << "// done" << endl;
		} else if ( ir.add_line( line ) ) {
			HHuginn::value_t res( ir.execute() );
			if ( !! res && ir.use_result() && ( res->type_id() == HHuginn::TYPE::INTEGER ) ) {
				retVal = static_cast<int>( static_cast<HHuginn::HInteger*>( res.raw() )->value() );
			} else {
				retVal = 0;
			}
			if ( !! res ) {
				if ( ir.use_result() ) {
					cout << to_string( res ) << endl;
				}
				cout << "// ok" << endl;
			} else {
				cout << ir.err() << endl;
				cout << "// error" << endl;
			}
		} else {
			cout << ir.err() << endl;
			cout << "// error" << endl;
		}
	}
	return ( retVal );
	M_EPILOG
}

}
