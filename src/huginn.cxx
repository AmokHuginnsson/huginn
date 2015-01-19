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

#include <yaal/hcore/hlog.hxx>
#include <yaal/hcore/hclock.hxx>
#include <yaal/hcore/hfile.hxx>
#include <yaal/tools/hhuginn.hxx>
M_VCSID( "$Id: " __ID__ " $" )
#include "huginn.hxx"

#include "setup.hxx"

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;

namespace huginn {

int main( int argc_, char** argv_ ) {
	HClock c;
	HHuginn h;
	i64_t huginn( c.get_time_elapsed( HClock::UNIT::MILISECOND ) );
	HString s;
	HPointer<HFile> f;
	if ( argc_ > 0 ) {
		f = make_pointer<HFile>( argv_[0], HFile::OPEN::READING );
		if ( ! *f ) {
			throw HFileException( f->get_error() );
		}
	}
	HStreamInterface* source( argc_ > 0 ? static_cast<HStreamInterface*>( f.raw() ) : &cin );

	for ( int i( 0 ); i < argc_; ++ i ) {
		h.add_argument( argv_[i] );
	}
	c.reset();
	h.load( *source );
	i64_t load( c.get_time_elapsed( HClock::UNIT::MILISECOND ) );
	c.reset();
	h.preprocess();
	i64_t preprocess( c.get_time_elapsed( HClock::UNIT::MILISECOND ) );
	c.reset();
	if ( h.parse() ) {
		i64_t parse( c.get_time_elapsed( HClock::UNIT::MILISECOND ) );
		c.reset();
		if ( h.compile() ) {
			i64_t compile( c.get_time_elapsed( HClock::UNIT::MILISECOND ) );
			c.reset();
			h.execute();
			i64_t execute( c.get_time_elapsed( HClock::UNIT::MILISECOND ) );
			if ( setup._generateLogs ) {
				log( LOG_TYPE::INFO )
					<< "Execution stats: huginn(" << huginn
					<< "), load(" << load
					<< "), preprocess(" << preprocess
					<< "), parse(" << parse
					<< "), compile(" << compile
					<< "), execute(" << execute << ")" << endl;
			}
		} else {
			cerr << h.error_message() << endl;
		}
	} else {
		cerr << h.error_message() << endl;
	}
	return ( 0 );
}

}

