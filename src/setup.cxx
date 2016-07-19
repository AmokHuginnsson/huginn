/*
---           `huginn' 0.0.0 (c) 1978 by Marcin 'Amok' Konarski            ---

	setup.cxx - this file is integral part of `huginn' project.

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

#include <cstdio>

#include <yaal/tools/util.hxx>
M_VCSID( "$Id: " __ID__ " $" )
#include <yaal/hcore/hfile.hxx>
#include "setup.hxx"

using namespace yaal::hcore;

namespace huginn {

void OSetup::test_setup( void ) {
	M_PROLOG
	if ( _quiet && _verbose ) {
		yaal::tools::util::failure( 1,
			_( "quiet and verbose options are exclusive\n" )
		);
	}
	if ( _verbose ) {
		clog.reset( make_pointer<HFile>( stdout, HFile::OWNERSHIP::EXTERNAL ) );
	}
	if ( _quiet ) {
		cout.reset();
	}
	if ( _nativeLines && ! _embedded ) {
		yaal::tools::util::failure( 2,
			_( "native lines makes sense only with embedded\n" )
		);
	}
	if ( _lint && _beSloppy ) {
		yaal::tools::util::failure( 3,
			_( "lint and be-sloppy options are mutually exclusive\n" )
		);
	}
	if ( _interactive && ( _lint || _embedded || _nativeLines ) ) {
		yaal::tools::util::failure( 4,
			_( "interactive is mutually axclusive with other switches\n" )
		);
	}
	if ( ! _program.is_empty() && ( _interactive || _lint || _embedded || _nativeLines ) ) {
		yaal::tools::util::failure( 5,
			_( "one-liner code mode is exclusive with other modes of operation\n" )
		);
	}
	if ( _interactive && _beSloppy ) {
		yaal::tools::util::failure( 6,
			_( "sloppy compiler mode is always selected for interactive mode\n" )
		);
	}
	if ( ! _program.is_empty() && _beSloppy ) {
		yaal::tools::util::failure( 6,
			_( "sloppy compiler mode is always selected for one-liner code mode\n" )
		);
	}
	if ( _noDefaultImports && ! _interactive ) {
		yaal::tools::util::failure( 7,
			_( "default imports setting can be only used in interactive mode\n" )
		);
	}
	if ( _noColor && ! _interactive ) {
		yaal::tools::util::failure( 7,
			_( "color setting can be only used in interactive mode\n" )
		);
	}
	return;
	M_EPILOG
}

}

