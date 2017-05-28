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
#include <yaal/tools/hhuginn.hxx>
#include <yaal/tools/stringalgo.hxx>

M_VCSID( "$Id: " __ID__ " $" )
#include "huginn.hxx"
#include "settings.hxx"
#include "setup.hxx"

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;
using namespace yaal::tools::huginn;

namespace huginn {

int main( int argc_, char** argv_ ) {
	M_PROLOG
	if ( setup._rapidStart ) {
		HHuginn::disable_grammar_verification();
	}
	HClock c;
	HHuginn h;
	i64_t huginn( c.get_time_elapsed( time::UNIT::MILLISECOND ) );
	HPointer<HFile> f;
	bool readFromScript( ( argc_ > 0 ) && ( argv_[0] != "-"_ys ) );
	if ( readFromScript ) {
		f = make_pointer<HFile>( argv_[0], HFile::OPEN::READING );
		if ( ! *f ) {
			throw HFileException( f->get_error() );
		}
	}
	HStreamInterface* source( readFromScript ? static_cast<HStreamInterface*>( f.raw() ) : &cin );

	if ( ! setup._noArgv ) {
		for ( int i( 0 ); i < argc_; ++ i ) {
			h.add_argument( argv_[i] );
		}
	}
	c.reset();
	int lineSkip( 0 );
	if ( setup._embedded ) {
		HString line;
#define LANG_NAME "huginn"
		HRegex r( "^#!.*\\b" LANG_NAME "\\b.*" );
		while ( source->read_until( line ) > 0 ) {
			++ lineSkip;
			if ( r.matches( line ) ) {
				break;
			}
		}
		typedef yaal::hcore::HArray<HString> tokens_t;
		int long settingPos( line.find( LANG_NAME ) );
		if ( settingPos != HString::npos ) {
			settingPos += static_cast<int>( sizeof ( LANG_NAME ) );
			tokens_t settings(
				string::split<tokens_t>(
					line.mid( settingPos ),
					character_class( CHARACTER_CLASS::WHITESPACE ).data(),
					HTokenizer::SKIP_EMPTY | HTokenizer::DELIMITED_BY_ANY_OF
				)
			);
			for ( HString const& s : settings ) {
				apply_setting( h, s );
			}
		}
#undef LANG_NAME

	}
	h.load( *source, setup._nativeLines ? 0 : lineSkip );
	i64_t load( c.get_time_elapsed( time::UNIT::MILLISECOND ) );
	c.reset();
	h.preprocess();
	i64_t preprocess( c.get_time_elapsed( time::UNIT::MILLISECOND ) );
	c.reset();
	bool ok( false );
	int retVal( 0 );
	do {
		if ( ! h.parse() ) {
			retVal = 1;
			break;
		}
		i64_t parse( c.get_time_elapsed( time::UNIT::MILLISECOND ) );
		c.reset();
		HHuginn::compiler_setup_t errorHandling( setup._beSloppy ? HHuginn::COMPILER::BE_SLOPPY : HHuginn::COMPILER::BE_STRICT );
		if ( ! h.compile( setup._modulePath, errorHandling ) ) {
			retVal = 2;
			break;
		}
		i64_t compile( c.get_time_elapsed( time::UNIT::MILLISECOND ) );
		c.reset();
		if ( ! setup._lint ) {
			if ( ! h.execute() ) {
				retVal = 3;
				break;
			}
		}
		if ( setup._dumpState ) {
			h.dump_vm_state( hcore::log );
		}
		i64_t execute( c.get_time_elapsed( time::UNIT::MILLISECOND ) );
		if ( setup._generateLogs ) {
			hcore::log( LOG_LEVEL::NOTICE )
				<< "Execution stats: huginn(" << huginn
				<< "), load(" << load
				<< "), preprocess(" << preprocess
				<< "), parse(" << parse
				<< "), compile(" << compile
				<< "), execute(" << execute << ")" << endl;
		}
		if ( ! setup._lint ) {
			HHuginn::value_t result( h.result() );
			if ( result->type_id() == HHuginn::TYPE::INTEGER ) {
				retVal = static_cast<int>( static_cast<HHuginn::HInteger*>( result.raw() )->value() );
			}
		}
		ok = true;
	} while ( false );
	if ( ! ok ) {
		cerr << h.error_message() << endl;
	}
	return ( retVal );
	M_EPILOG
}


}

