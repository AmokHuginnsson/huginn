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
	enum class LINE_TYPE {
		NONE,
		CODE,
		IMPORT
	};
private:
	lines_t _lines;
	lines_t _imports;
	LINE_TYPE _lastLine;
	HHuginn::ptr_t _huginn;
	HStringStream _streamCache;
public:
	HInteractiveRunner( void )
		: _lines()
		, _imports()
		, _lastLine( LINE_TYPE::NONE )
		, _huginn()
		, _streamCache() {
		if ( ! setup._noDefaultImports ) {
			_imports.emplace_back( "import Mathematics as M;" );
			_imports.emplace_back( "import Algorithms as A;" );
			_imports.emplace_back( "import Text as T;" );
		}
		return;
	}
	bool add_line( yaal::hcore::HString const& line_ ) {
		M_PROLOG
		static char const inactive[] = ";\t \r\n\a\b\f\v";
		static HRegex importPattern( "\\s*import\\s+[A-Za-z]+\\s+as\\s+[A-Za-z]+;?" );
		_lastLine = LINE_TYPE::NONE;
		bool isImport( importPattern.matches( line_ ) );
		_streamCache.clear();

		HString result( line_ );

		bool gotSemi( false );
		result.trim_right( _whiteSpace_.data() );
		while ( ! result.is_empty() && ( result.back() == ';' ) ) {
			result.pop_back();
			result.trim_right( _whiteSpace_.data() );
			gotSemi = true;
		}
		bool gotResult( ! result.is_empty() );

		if ( ! ( gotResult || _lines.is_empty() ) ) {
			result = _lines.back();
			result.trim_right( inactive );
		}

		bool expr( ! result.is_empty() && ( result.back() != '}' ) );

		int lineCount( static_cast<int>( _lines.get_size() ) );

		for ( yaal::hcore::HString const& import : _imports ) {
			_streamCache << import << endl;
		}

		if ( isImport ) {
			gotResult = false;
			if ( ! _lines.is_empty() ) {
				result = _lines.back();
			} else {
				result.clear();
			}
			result.trim_right( inactive );
			if ( result.is_empty() ) {
				expr = false;
			}
			_streamCache << line_ << ( ( line_.back() != ';' ) ? ";" : "" ) << endl;
		}

		_streamCache << "main() {\n";
		for ( int i( 0 ); i < ( lineCount - ( gotResult ? 0 : 1 ) ); ++ i ) {
			_streamCache << '\t' << _lines[i] << "\n";
		}

		if ( expr ) {
			_streamCache << "\treturn ( " << result << " );\n}\n";
		} else {
			if ( ! result.is_empty() ) {
				_streamCache << "\t" << result << ( gotSemi ? ";" : "" ) << "\n";
			}
			_streamCache << "\treturn;\n}\n";
		}
		clog << _streamCache.string();
		_huginn = make_pointer<HHuginn>();
		_huginn->load( _streamCache, "*interactive session*" );
		_huginn->preprocess();
		bool ok( _huginn->parse() );
		if ( ! ok ) {
			int importCount( static_cast<int>( _imports.get_size() ) );
			if ( ( lineCount > 0 ) && ( ( _huginn->error_coordinate().line() - 2 ) == ( lineCount + importCount ) ) ) {
				if ( _lines.back().back() != ';' ) {
					_lines.back().push_back( ';' );
					ok = add_line( line_ );
					if ( ok ) {
						_lines.pop_back();
					} else {
						_lines.back().pop_back();
					}
				}
			}
		} else {
			ok = _huginn->compile( HHuginn::COMPILER::BE_SLOPPY );
		}

		if ( ok ) {
			if ( gotResult ) {
				if ( gotSemi ) {
					result.push_back( ';' );
				}
				_lines.push_back( result );
			} else if ( isImport ) {
				_imports.push_back( line_ );
				if ( line_.back() != ';' ) {
					_imports.back().push_back( ';' );
				}
			}
			_lastLine = isImport ? LINE_TYPE::IMPORT : LINE_TYPE::CODE;
		}
		return ( ok );
		M_EPILOG
	}
	HHuginn::value_t execute( void ) {
		M_PROLOG
		if ( ! _huginn->execute() ) {
			if ( _lastLine == LINE_TYPE::CODE ) {
				_lines.pop_back();
			} else if ( _lastLine == LINE_TYPE::IMPORT ) {
				_imports.pop_back();
			}
		}
		return ( _huginn->result() );
		M_EPILOG
	}
	yaal::hcore::HString err( void ) const {
		M_PROLOG
		return ( _huginn->error_message() );
		M_EPILOG
	}
};

int interactive_session( void ) {
	M_PROLOG
	char const prompt[] = "huginn> ";
	HString line;
	HInteractiveRunner ir;
	cout << prompt << flush;
	while ( cin.read_until( line ) > 0 ) {
		if ( ir.add_line( line ) ) {
			HHuginn::value_t res( ir.execute() );
			if ( !! res ) {
				cout << to_string( res ) << endl;
			} else {
				cerr << ir.err() << endl;
			}
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

