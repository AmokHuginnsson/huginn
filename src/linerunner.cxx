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
#include "linerunner.hxx"
#include "setup.hxx"

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;
using namespace yaal::tools::huginn;

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
	, _wordCache()
	, _source()
	, _session( session_ ) {
	if ( ! setup._noDefaultImports ) {
		_imports.emplace_back( "import Mathematics as M;" );
		_imports.emplace_back( "import Algorithms as A;" );
		_imports.emplace_back( "import Text as T;" );
	}
	HSignalService::get_instance().register_handler( SIGINT, call( &HLineRunner::handle_interrupt, this, _1 ) );
	return;
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
	_streamCache.reset();

	HString input( line_ );

	input.trim( inactive );

	bool isImport( importParser( to_string( input ).append( ";" ) ) );
	bool isDefinition( classParser( input ) || functionParser( input ) );

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
	_huginn = make_pointer<HHuginn>();
	_huginn->load( _streamCache, _session );
	_huginn->preprocess();
	bool ok( _huginn->parse() );

	bool gotSemi( false );
	if ( ! ok ) {
		_source.erase( _source.get_length() - 3 );
		_source.append( ";\n}\n" );
		_streamCache.str( _source );
		_huginn = make_pointer<HHuginn>();
		_huginn->load( _streamCache, _session );
		_huginn->preprocess();
		gotSemi = ok = _huginn->parse();
	}
	if ( isImport || gotSemi ) {
		input.push_back( ';' );
	}
	if ( ok ) {
		ok = _huginn->compile( HHuginn::COMPILER::BE_SLOPPY );
	}

	_lastLineType = isImport ? LINE_TYPE::IMPORT : ( isDefinition ? LINE_TYPE::DEFINITION : LINE_TYPE::CODE );
	if ( ok ) {
		_lastLine = input;
		if ( gotInput ) {
			_lines.push_back( input );
		} else if ( isImport ) {
			_imports.push_back( input );
		} else if ( isDefinition ) {
			_definitions.push_back( input );
			_definitionsLineCount += static_cast<int>( count( input.begin(), input.end(), '\n' ) + 1 );
		}
		fill_words();
	} else {
		_lastLine = input;
	}
	return ( ok );
	M_EPILOG
}

HHuginn::value_t HLineRunner::execute( void ) {
	M_PROLOG
	bool ok( true );
	if ( ! ( ok = _huginn->execute() ) ) {
		if ( _lastLineType == LINE_TYPE::CODE ) {
			_lines.pop_back();
		} else if ( _lastLineType == LINE_TYPE::IMPORT ) {
			_imports.pop_back();
		} else if ( _lastLineType == LINE_TYPE::DEFINITION ) {
			_definitions.pop_back();
			_definitionsLineCount += static_cast<int>( count( _lastLine.begin(), _lastLine.end(), '\n' ) + 1 );
		}
	} else {
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

yaal::tools::HHuginn const* HLineRunner::huginn( void ) const {
	return ( _huginn.raw() );
}

yaal::hcore::HString HLineRunner::err( void ) const {
	M_PROLOG
	int lineNo( _huginn->error_coordinate().line() );
	int mainLineNo( static_cast<int>( _imports.get_size() + _definitionsLineCount + _definitions.get_size() + 1 ) );
	if ( _lastLineType == LINE_TYPE::DEFINITION ) {
		mainLineNo += static_cast<int>( count( _lastLine.begin(), _lastLine.end(), '\n' ) + 1 );
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
	for ( yaal::hcore::HString const& line : _imports ) {
		cout << line << endl;
	}
	if ( lineNo <= static_cast<int>( _imports.get_size() + 1 ) ) {
		cout << offending << endl;
	}
	if ( ! _imports.is_empty() ) {
		cout << endl;
	}
	for ( yaal::hcore::HString const& line : _definitions ) {
		cout << line << endl << endl;
	}
	if ( ( lineNo > static_cast<int>( _imports.get_size() + 1 ) ) && ( lineNo <= mainLineNo ) ) {
		cout << offending << endl << endl;
	}
	cout << "main() {" << endl;
	for ( yaal::hcore::HString const& line : _lines ) {
		cout << "\t" << line << endl;
	}
	if ( lineNo > mainLineNo ) {
		cout << "\t" << offending << endl;
	}
	cout << "}" << endl;
	return ( _huginn->error_message() );
	M_EPILOG
}

int HLineRunner::handle_interrupt( int ) {
	yaal::_isKilled_ = true;
	_interrupted = true;
	return ( 1 );
}

void HLineRunner::fill_words( void ) {
	M_PROLOG
	_wordCache.clear();
	_streamCache.reset();
	_huginn->dump_vm_state( _streamCache );
	HString word;
	_streamCache.read_until( word ); /* drop header */
	while ( _streamCache.read_until( word, _whiteSpace_.data() ) > 0 ) {
		if ( word.is_empty() || ( word.back() == ':' ) || ( word.back() == ')' ) || ( word.front() == '(' ) ) {
			continue;
		}
		_wordCache.push_back( word );
	}
	sort( _wordCache.begin(), _wordCache.end() );
	_wordCache.erase( unique( _wordCache.begin(), _wordCache.end() ), _wordCache.end() );
	return;
	M_EPILOG
}

HLineRunner::words_t const& HLineRunner::words( void ) {
	M_PROLOG
	if ( _wordCache.is_empty() ) {
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
		if ( _huginn->parse() && _huginn->compile( HHuginn::COMPILER::BE_SLOPPY ) ) {
			fill_words();
		}
	}
	return ( _wordCache );
	M_EPILOG
}

}

