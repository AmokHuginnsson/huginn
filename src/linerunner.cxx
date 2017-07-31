/*
---           `huginn' 0.0.0 (c) 1978 by Marcin 'Amok' Konarski            ---

  linerunner.cxx - this file is integral part of `huginn' project.

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
#include <yaal/tools/ansi.hxx>
#include <yaal/tools/signals.hxx>
#include <yaal/tools/hterminal.hxx>

#include <signal.h>

M_VCSID( "$Id: " __ID__ " $" )
M_VCSID( "$Id: " __TID__ " $" )

#include "config.hxx"

#ifdef USE_REPLXX
#	include <replxx.h>
#	define REPL_print replxx_print
#elif defined( USE_EDITLINE )
#	define REPL_print printf
#else
#	define REPL_print printf
#endif
#include "linerunner.hxx"
#include "setup.hxx"

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;
using namespace yaal::tools::huginn;

namespace yaal { namespace tools { namespace huginn {
bool is_keyword( yaal::hcore::HString const& );
} } }

namespace huginn {

HLineRunner::HLineRunner( yaal::hcore::HString const& session_ )
	: _lines()
	, _imports()
	, _definitions()
	, _definitionsLineCount( 0 )
	, _lastLineType( LINE_TYPE::NONE )
	, _lastLine()
	, _interrupted( false )
	, _huginn()
	, _streamCache()
	, _description()
	, _source()
	, _locals()
	, _session( session_ ) {
	M_PROLOG
	HHuginn::disable_grammar_verification();
	reset();
	HSignalService::get_instance().register_handler( SIGINT, call( &HLineRunner::handle_interrupt, this, _1 ) );
	return;
	M_EPILOG
}

void HLineRunner::reset( void ) {
	M_PROLOG
	_lines.clear();
	_imports.clear();
	_definitions.clear();
	_definitionsLineCount = 0;
	_lastLineType = LINE_TYPE::NONE;
	_lastLine.clear();
	_interrupted = false;
	_locals.clear();
	_huginn = make_pointer<HHuginn>();
	_huginn->reset();
	_streamCache.clear();
	_description.clear();
	_source.clear();
	if ( ! setup._noDefaultImports ) {
		_imports.emplace_back( "import Mathematics as M;" );
		_imports.emplace_back( "import Algorithms as A;" );
		_imports.emplace_back( "import Text as T;" );
	}
	return;
	M_EPILOG
}

namespace {
yaal::hcore::HString first_name( yaal::hcore::HString const& input_ ) {
	int long nonNameIdx( input_.find_one_of( HString( character_class( CHARACTER_CLASS::WHITESPACE ).data() ).append( '(' ) ) );
	return ( input_.substr( 0, nonNameIdx != yaal::hcore::HString::npos ? nonNameIdx : 0 ) );
}
}

void HLineRunner::do_introspect( yaal::tools::HIntrospecteeInterface& introspectee_ ) {
	M_PROLOG
	_locals = introspectee_.get_locals( 0 );
	return;
	M_EPILOG
}

bool HLineRunner::add_line( yaal::hcore::HString const& line_ ) {
	M_PROLOG
	static char const inactive[] = ";\t \r\n\a\b\f\v";
	static HHuginn grammarSource;
	static executing_parser::HRule grammar( grammarSource.make_engine() );
	static executing_parser::HRuleBase const* importRule( grammar.find( "importStatement" ) );
	static executing_parser::HRuleBase const* classRule( grammar.find( "classDefinition" ) );
	static executing_parser::HRuleBase const* functionRule( grammar.find( "functionDefinition" ) );
	static HExecutingParser importParser( *importRule );
	static HExecutingParser classParser( *classRule );
	static HExecutingParser functionParser( *functionRule );
	_lastLineType = LINE_TYPE::NONE;

	HHuginn preprocessor;
	HStringStream src( line_ );
	preprocessor.load( src );
	preprocessor.preprocess();
	preprocessor.dump_preprocessed_source( src );
	HString input( src.string() );

	input.trim( inactive );

	bool isImport( importParser( to_string( input ).append( ";" ) ) );
	bool isDefinition( classParser( input ) || ( functionParser( input ) && ! is_keyword( first_name( input ) ) ) );

	_streamCache.reset();

	for ( yaal::hcore::HString const& import : _imports ) {
		_streamCache << import << "\n";
	}
	if ( isImport ) {
		_streamCache << input << ";\n";
	}
	_streamCache << "\n";

	for ( yaal::hcore::HString const& c : _definitions ) {
		_streamCache << c << "\n\n";
	}
	if ( isDefinition ) {
		_streamCache << input << "\n\n";
	}

	_streamCache << "main() {\n";
	for ( HString const& l : _lines ) {
		_streamCache << '\t' << l << "\n";
	}

	bool gotInput( ! ( isImport || isDefinition || input.is_empty() ) );
	if ( gotInput ) {
		_streamCache << "\t" << input << "\n";
	}

	_streamCache << "}" << "\n";
	_source = _streamCache.string();
	_huginn->reset();
	_huginn->load( _streamCache, _session );
	_huginn->preprocess();
	bool ok( _huginn->parse() );

	bool gotSemi( false );
	if ( ! ok ) {
		_source.erase( _source.get_length() - 3 );
		_source.append( ";\n}\n" );
		_streamCache.str( _source );
		_huginn->reset();
		_huginn->load( _streamCache, _session );
		_huginn->preprocess();
		gotSemi = ok = _huginn->parse();
	}
	if ( isImport || gotSemi ) {
		input.push_back( ';'_ycp );
	}
	if ( ok ) {
		ok = _huginn->compile( setup._modulePath, HHuginn::COMPILER::BE_SLOPPY, this );
	}

	_lastLineType = isImport ? LINE_TYPE::IMPORT : ( isDefinition ? LINE_TYPE::DEFINITION : ( ok ? LINE_TYPE::CODE : LINE_TYPE::NONE ) );
	_lastLine = input;
	if ( ok ) {
		if ( gotInput ) {
			_lines.push_back( _lastLine );
		} else if ( isImport ) {
			_imports.push_back( _lastLine );
		} else if ( isDefinition ) {
			_definitions.push_back( _lastLine );
			_definitionsLineCount += static_cast<int>( count( _lastLine.cbegin(), _lastLine.cend(), '\n'_ycp ) + 1 );
		}
		_description.prepare( *_huginn );
	}
	return ( ok );
	M_EPILOG
}

HHuginn::value_t HLineRunner::execute( void ) {
	M_PROLOG
	bool ok( true );
	if ( ( ok = _huginn->execute() ) ) {
		_description.note_locals( _locals );
		clog << _source;
	}
	if ( _interrupted ) {
		_interrupted = false;
		yaal::_isKilled_ = false;
		ok = false;
	}
	return ( ok ? _huginn->result() : HHuginn::value_t() );
	M_EPILOG
}

bool HLineRunner::use_result( void ) const {
	return ( _lastLineType == LINE_TYPE::CODE );
}

void HLineRunner::undo( void ) {
	M_PROLOG
	if ( _lastLineType == LINE_TYPE::CODE ) {
		_lines.pop_back();
	} else if ( _lastLineType == LINE_TYPE::IMPORT ) {
		_imports.pop_back();
	} else if ( _lastLineType == LINE_TYPE::DEFINITION ) {
		_definitions.pop_back();
		_definitionsLineCount -= static_cast<int>( count( _lastLine.cbegin(), _lastLine.cend(), '\n'_ycp ) + 1 );
	}
	_lastLineType = LINE_TYPE::NONE;
	return;
	M_EPILOG
}

yaal::tools::HHuginn const* HLineRunner::huginn( void ) const {
	return ( _huginn.raw() );
}

yaal::tools::HHuginn* HLineRunner::huginn( void ) {
	return ( _huginn.raw() );
}

yaal::hcore::HString HLineRunner::err( void ) const {
	M_PROLOG
	if ( setup._hideErrorContext ) {
		HString m( _huginn->error_message() );
		int long p( m.find( ": " ) );
		if ( p != HString::npos ) {
			m.shift_left( p + 2 );
		}
		return ( m );
	}
	int lineNo( _huginn->error_coordinate().line() );
	int mainLineNo( static_cast<int>( _imports.get_size() + 1 + _definitionsLineCount + _definitions.get_size() + 1 ) );
	if ( _lastLineType == LINE_TYPE::DEFINITION ) {
		mainLineNo += static_cast<int>( count( _lastLine.begin(), _lastLine.end(), '\n'_ycp ) + 1 );
	}
	int colNo( _huginn->error_coordinate().column() - 1 /* col no is 1 bases */ - ( lineNo > mainLineNo ? 1 /* we add tab key to user input */ : 0 ) );
	bool useColor( is_a_tty( cout ) && ! ( setup._noColor || setup._jupyter ) );
	hcore::HString offending;
	int lineCount( static_cast<int>( _imports.get_size() + _definitionsLineCount + _definitions.get_size() + _lines.get_size() ) + 1 /* empty line after imports */ + 1 /* main() */ + 1 /* current line === last line */ );
	if ( useColor && ( lineNo <= lineCount ) && ( colNo < static_cast<int>( _lastLine.get_length() ) ) ) {
		offending
			.assign( _lastLine.left( colNo ) )
			.append( *ansi::bold )
			.append( _lastLine[colNo] )
			.append( *ansi::reset )
			.append( _lastLine.mid( colNo + 1 ) );
	} else {
		offending = _lastLine;
	}
	HUTF8String utf8;
	for ( yaal::hcore::HString const& line : _imports ) {
		utf8.assign( line );
		REPL_print( "%s\n", utf8.c_str() );
	}
	if ( lineNo <= static_cast<int>( _imports.get_size() + 1 ) ) {
		utf8.assign( offending );
		REPL_print( "%s\n", utf8.c_str() );
	}
	if ( ! _imports.is_empty() ) {
		REPL_print( "\n" );
	}
	for ( yaal::hcore::HString const& line : _definitions ) {
		utf8.assign( line );
		REPL_print( "%s\n\n", utf8.c_str() );
	}
	if ( ( lineNo > static_cast<int>( _imports.get_size() + 1 ) ) && ( lineNo <= mainLineNo ) ) {
		utf8.assign( offending );
		REPL_print( "%s\n\n", utf8.c_str() );
	}
	REPL_print( "main() {\n" );
	for ( int i( 0 ), COUNT( static_cast<int>( _lines.get_size() ) - ( _lastLineType == LINE_TYPE::CODE ? 1 : 0 ) ); i < COUNT; ++ i ) {
		utf8.assign( _lines[i] );
		REPL_print( "\t%s\n", utf8.c_str() );
	}
	if ( lineNo > mainLineNo ) {
		utf8.assign( offending );
		REPL_print( "\t%s\n", utf8.c_str() );
	}
	REPL_print( "}\n" );
	return ( _huginn->error_message() );
	M_EPILOG
}

void HLineRunner::prepare_source( void ) {
	M_PROLOG
	_streamCache.reset();

	for ( yaal::hcore::HString const& import : _imports ) {
		_streamCache << import << "\n";
	}
	_streamCache << "\n";

	for ( yaal::hcore::HString const& c : _definitions ) {
		_streamCache << c << "\n\n";
	}

	_streamCache << "main() {\n";
	for ( HString const& l : _lines ) {
		_streamCache << '\t' << l << "\n";
	}

	_streamCache << "}" << "\n";
	_source = _streamCache.string();
	return;
	M_EPILOG
}

int HLineRunner::handle_interrupt( int ) {
	yaal::_isKilled_ = true;
	_interrupted = true;
	return ( 1 );
}

HLineRunner::words_t const& HLineRunner::words( void ) {
	M_PROLOG
	if ( _description.symbols().is_empty() ) {
		_streamCache.reset();
		for ( yaal::hcore::HString const& line : _imports ) {
			_streamCache << line << endl;
		}
		_streamCache << "main() {" << endl;
		for ( yaal::hcore::HString const& line : _lines ) {
			_streamCache << line << endl;
		}
		if ( ! _lines.is_empty() ) {
			if ( ( _lines.back().back() != ';' ) && ( _lines.back().back() != '}' ) ) {
				_streamCache << ';';
			}
		}
		_streamCache << "return;\n}\n" << endl;
		_huginn = make_pointer<HHuginn>();
		_huginn->load( _streamCache, "*interactive session*" );
		_huginn->preprocess();
		if ( _huginn->parse() && _huginn->compile( setup._modulePath, HHuginn::COMPILER::BE_SLOPPY ) ) {
			_description.prepare( *_huginn );
			_description.note_locals( _locals );
		}
	}
	return ( _description.symbols() );
	M_EPILOG
}

HLineRunner::words_t const& HLineRunner::methods( yaal::hcore::HString const& symbol_ ) {
	words(); // gen docs.
	return ( _description.methods( symbol_ ) );
}

HDescription::words_t const& HLineRunner::dependent_symbols( yaal::hcore::HString const& symbol_ ) {
	M_PROLOG
	words(); // gen docs.
	words_t const* w( &_description.symbols() );
	if ( add_line( "type("_ys.append( symbol_ ).append( ")" ) ) ) {
		HHuginn::value_t res( execute() );
		if ( !! res ) {
			HString type( to_string( res, _huginn.raw() ) );
			w = &_description.methods( type );
			undo();
			_huginn->reset( 1 );
		}
	}
	return ( *w );
	M_EPILOG
}

yaal::hcore::HString const& HLineRunner::source( void ) {
	M_PROLOG
	prepare_source();
	return ( _source );
	M_EPILOG
}

HLineRunner::lines_t const& HLineRunner::imports( void ) const {
	M_PROLOG
	return ( _imports );
	M_EPILOG
}

yaal::hcore::HString HLineRunner::doc( yaal::hcore::HString const& symbol_ ) {
	M_PROLOG
	words(); // gen docs.
	return ( _description.doc( symbol_ ) );
	M_EPILOG
}

}

