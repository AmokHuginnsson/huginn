/*
---           `huginn' 0.0.0 (c) 1978 by Marcin 'Amok' Konarski            ---

	huginn.cxx - this file is integral part of `huginn' project.

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
#include <yaal/tools/hhuginn.hxx>
M_VCSID( "$Id: " __ID__ " $" )
#include "huginn.hxx"

using namespace yaal::hcore;
using namespace yaal::tools;

namespace huginn {

int main( int argc_, char** argv_ ) {
	HHuginn h;
	HString s;
	HPointer<HFile> file;
	if ( argc_ > 0 ) {
		file = make_pointer<HFile>( argv_[0], HFile::OPEN::READING );
		if ( ! *file ) {
			throw HFileException( file->get_error() );
		}
	}
	HStreamInterface* source( argc_ > 0 ? static_cast<HStreamInterface*>( file.raw() ) : &cin );

	for ( int i( 0 ); i < argc_; ++ i ) {
		h.add_argument( argv_[i] );
	}
	h.load( *source );
	h.preprocess();
	if ( ! h.parse() ) {
		cerr << h.error_message() << endl;
	} else {
		h.compile();
		h.execute();
	}
	return ( 0 );
}

}

