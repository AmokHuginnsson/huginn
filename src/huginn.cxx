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
#include <yaal/tools/hstringstream.hxx>
#include <yaal/tools/hhuginn.hxx>
M_VCSID( "$Id: " __ID__ " $" )
#include "huginn.hxx"

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
	i64_t huginn( c.get_time_elapsed( time::UNIT::MILISECOND ) );
	HPointer<HFile> f;
	bool readFromScript( argc_ > 0 && ( argv_[0] != "-"_ys ) );
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
		HString s;
		HRegex r( "^#!.*" YAAL_REGEX_WORD_START "huginn" YAAL_REGEX_WORD_END ".*$" );
		while ( source->read_until( s ) > 0 ) {
			++ lineSkip;
			if ( r.matches( s ) ) {
				break;
			}
		}
	}
	h.load( *source, setup._nativeLines ? 0 : lineSkip );
	i64_t load( c.get_time_elapsed( time::UNIT::MILISECOND ) );
	c.reset();
	h.preprocess();
	i64_t preprocess( c.get_time_elapsed( time::UNIT::MILISECOND ) );
	c.reset();
	bool ok( false );
	int retVal( 0 );
	do {
		if ( ! h.parse() ) {
			retVal = 1;
			break;
		}
		i64_t parse( c.get_time_elapsed( time::UNIT::MILISECOND ) );
		c.reset();
		HHuginn::compiler_setup_t errorHandling( setup._beSloppy ? HHuginn::COMPILER::BE_SLOPPY : HHuginn::COMPILER::BE_STRICT );
		if ( ! h.compile( errorHandling ) ) {
			retVal = 2;
			break;
		}
		i64_t compile( c.get_time_elapsed( time::UNIT::MILISECOND ) );
		c.reset();
		if ( ! setup._lint ) {
			if ( ! h.execute() ) {
				retVal = 3;
				break;
			}
		}
		if ( setup._dumpState ) {
			h.dump_vm_state( log );
		}
		i64_t execute( c.get_time_elapsed( time::UNIT::MILISECOND ) );
		if ( setup._generateLogs ) {
			log( LOG_LEVEL::NOTICE )
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

class HInteractiveRunner {
public:
	typedef HInteractiveRunner this_type;
	typedef yaal::hcore::HArray<yaal::hcore::HString> lines_t;
private:
	lines_t _lines;
	HHuginn::ptr_t _huginn;
	HStringStream _streamCache;
public:
	HInteractiveRunner( void )
		: _lines()
		, _huginn()
		, _streamCache() {
		return;
	}
	bool add_line( yaal::hcore::HString const& line_ ) {
		M_PROLOG
		if ( ! line_.is_empty() ) {
			_lines.push_back( line_ );
		}
		_streamCache.clear();
		_streamCache << "main() {\n";
		int lineCount( static_cast<int>( _lines.get_size() ) );
		for ( int i( 0 ); i < ( lineCount - 1 ); ++ i ) {
			_streamCache << '\t' << _lines[i] << "\n";
		}
		HString result( ! _lines.is_empty() ? _lines.back() : "" );
		if ( ! result.is_empty() && ( result.back() == ';' ) ) {
			result.pop_back();
		}
		if ( ! result.is_empty() ) {
			_streamCache << "\treturn ( " << result << " );\n}\n";
		} else {
			_streamCache << "\treturn;\n}\n";
		}
		clog << _streamCache.string();
		_huginn = make_pointer<HHuginn>();
		_huginn->load( _streamCache, "*interactive session*" );
		_huginn->preprocess();
		bool ok( _huginn->parse() );
		if ( ! ok ) {
			if ( ( lineCount > 1 ) && ( ( _huginn->error_coordinate().line() - 1 ) == lineCount ) ) {
				if ( _lines[lineCount - 1].back() != ';' ) {
					_lines.pop_back();
					_lines.back().push_back( ';' );
					ok = add_line( line_ );
				}
			}
		} else {
			ok = _huginn->compile( HHuginn::COMPILER::BE_SLOPPY );
		}
		if ( ! ok ) {
			_lines.pop_back();
		}
		return ( ok );
		M_EPILOG
	}
	HHuginn::value_t execute( void ) {
		M_PROLOG
		_huginn->execute();
		return ( _huginn->result() );
		M_EPILOG
	}
	yaal::hcore::HString err( void ) const {
		M_PROLOG
		return ( _huginn->error_message() );
		M_EPILOG
	}
};

namespace {
yaal::hcore::HString dump_value( HHuginn::value_t const& value_ ) {
	yaal::hcore::HString str;
	switch ( static_cast<HHuginn::TYPE>( value_->type_id().get() ) ) {
		case ( HHuginn::TYPE::STRING ): {
			str.assign( '"' ).append( static_cast<HHuginn::HString const*>( value_.raw() )->value() ).append( '"' );
		} break;
		case ( HHuginn::TYPE::INTEGER ): {
			str = static_cast<HHuginn::HInteger const*>( value_.raw() )->value();
		} break;
		case ( HHuginn::TYPE::REAL ): {
			str = static_cast<HHuginn::HReal const*>( value_.raw() )->value();
		} break;
		case ( HHuginn::TYPE::NUMBER ): {
			str.assign( '$' ).append( static_cast<HHuginn::HNumber const*>( value_.raw() )->value().to_string() );
		} break;
		case ( HHuginn::TYPE::CHARACTER ): {
			str.assign( "'" ).append( static_cast<HHuginn::HCharacter const*>( value_.raw() )->value() ).append( "'" );
		} break;
		case ( HHuginn::TYPE::NONE ): {
			str = "none";
		} break;
		default: {
			str = "<<< unknown >>>";
		}
	}
	return ( str );
}
}

int interactive_session( void ) {
	M_PROLOG
	char const prompt[] = "huginn> ";
	HString line;
	HInteractiveRunner ir;
	cout << prompt << flush;
	while ( cin.read_until( line ) > 0 ) {
		if ( ir.add_line( line ) ) {
			cout << dump_value( ir.execute() ) << endl;
		} else {
			cerr << ir.err() << endl;
		}
		cout << prompt << flush;
	}
	cout << endl;
	return ( 0 );
	M_EPILOG
}

}

