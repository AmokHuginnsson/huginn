/*
---           `huginn' 0.0.0 (c) 1978 by Marcin 'Amok' Konarski            ---

  oneliner.cxx - this file is integral part of `huginn' project.

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

#include <yaal/tools/hhuginn.hxx>
#include <yaal/tools/hstringstream.hxx>
#include <yaal/hcore/hfile.hxx>
M_VCSID( "$Id: " __ID__ " $" )
M_VCSID( "$Id: " __TID__ " $" )
#include "oneliner.hxx"

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;
using namespace yaal::tools::huginn;

namespace huginn {

int oneliner( yaal::hcore::HString const& program_ ) {
	HStringStream ss;
	HString code;
	ss <<
"import Mathematics as M;\n"
"import Algorithms as A;\n"
"import Text as T;\n"
"\n"
"main() {\n"
"\t" << program_ << ( ! program_.is_empty() && ( program_.back() != ';' ) ? ";" : "" ) << "\n}\n";
	code = ss.string();
	HHuginn h;
	h.load( ss );
	h.preprocess();
	int retVal( 0 );
	bool ok( h.parse() && h.compile( HHuginn::COMPILER::BE_SLOPPY ) && h.execute() );
	if ( ok ) {
		HHuginn::value_t result( h.result() );
		cout << to_string( result ) << endl;
		if ( result->type_id() == HHuginn::TYPE::INTEGER ) {
			retVal = static_cast<int>( static_cast<HHuginn::HInteger*>( result.raw() )->value() );
		}
	} else {
		cerr << code << h.error_message() << endl;
	}
	return ( retVal );
}

}

