/*
---           `huginn' 0.0.0 (c) 1978 by Marcin 'Amok' Konarski            ---

  meta.cxx - this file is integral part of `huginn' project.

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
#include "meta.hxx"

using namespace yaal::hcore;

namespace huginn {

bool meta( HLineRunner& lr_, yaal::hcore::HString const& line_ ) {
	M_PROLOG
	bool isMeta( true );
	if (  line_ == "// source" ) {
		cout << lr_.source();
	} else if (  line_ == "// imports" ) {
		for ( HLineRunner::lines_t::value_type const& l : lr_.imports() ) {
			cout << l << endl;
		}
	} else if (  line_.find( "// doc" ) == 0 ) {
		HString symbol( line_.substr( 7 ) );
		HString doc( lr_.doc( symbol ) );
		cout << ( ! doc.is_empty() ? doc : "symbol `"_ys.append( symbol ).append( "' is unknown or undocumented" ) ) << endl;
	} else if ( line_ == "// reset" ) {
		lr_.reset();
	} else {
		isMeta = false;
	}
	if ( isMeta ) {
		cout << "// ok" << endl;
	}
	return ( isMeta );
	M_EPILOG
}

}

