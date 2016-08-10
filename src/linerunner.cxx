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
	, _classes()
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
	static HRegex importPattern( "\\s*import\\s+[A-Za-z]+\\s+as\\s+[A-Za-z]+;?", HRegex::COMPILE::EXTENDED );
	static HRegex classPattern( "\\s*class\\s+[A-Z][A-Za-z]*\\s*(:\\s*[A-Z][A-Za-z]*\\s*)?{", HRegex::COMPILE::EXTENDED );
	_lastLineType = LINE_TYPE::NONE;
	bool isImport( importPattern.matches( line_ ) );
	bool isClass( classPattern.matches( line_ ) );
	_streamCache.reset();

	HString result( line_ );

	bool gotSemi( false );
	result.trim( _whiteSpace_.data() );
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

	int lineCount( static_cast<int>( _lines.get_size() ) );

	for ( yaal::hcore::HString const& import : _imports ) {
		_streamCache << import << endl;
	}
	for ( yaal::hcore::HString const& c : _classes ) {
		_streamCache << c << endl;
	}

	if ( isImport || isClass ) {
		gotResult = false;
		if ( ! _lines.is_empty() ) {
			result = _lines.back();
		} else {
			result.clear();
		}
		result.trim_right( inactive );
		_streamCache << line_ << ( ( line_.back() != ';' ) && isImport ? ";" : "" ) << endl;
	}

	_streamCache << "main() {\n";
	for ( int i( 0 ); i < ( lineCount - ( gotResult ? 0 : 1 ) ); ++ i ) {
		_streamCache << '\t' << _lines[i] << "\n";
	}

	if ( ! result.is_empty() ) {
		_streamCache << "\t" << result << ( gotSemi || ( result.back() != '}' ) ? ";" : "" ) << endl;
	}
	_streamCache << "}" << endl;
	_source = _streamCache.string();
	_huginn = make_pointer<HHuginn>();
	_huginn->load( _streamCache, _session );
	_huginn->preprocess();
	bool ok( _huginn->parse() );
	if ( ! ok ) {
		int importCount( static_cast<int>( _imports.get_size() ) );
		int classCount( static_cast<int>( _classes.get_size() ) );
		if ( ( lineCount > 0 ) && ( ( _huginn->error_coordinate().line() - 2 ) == ( lineCount + importCount + classCount ) ) ) {
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
			_lines.push_back( _lastLine = result );
		} else if ( isImport ) {
			_imports.push_back( _lastLine = line_ );
			if ( line_.back() != ';' ) {
				_imports.back().push_back( ';' );
			}
		} else if ( isClass ) {
			_classes.push_back( _lastLine = line_ );
		}
		_lastLineType = isImport ? LINE_TYPE::IMPORT : ( isClass ? LINE_TYPE::CLASS : LINE_TYPE::CODE );
		fill_words();
	} else {
		_lastLine = isImport || isClass ? line_ : result;
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
		} else if ( _lastLineType == LINE_TYPE::CLASS ) {
			_classes.pop_back();
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

yaal::hcore::HString HLineRunner::err( void ) const {
	M_PROLOG
	int lineNo( _huginn->error_coordinate().line() );
	int colNo( _huginn->error_coordinate().column() - 1 /* col no is 1 bases */ - 1 /* we add tab key to user input */ );
	bool useColor( is_a_tty( cout ) && ! ( setup._noColor || setup._jupyter ) );
	hcore::HString offending;
	int lineCount( static_cast<int>( _imports.get_size() + _classes.get_size() + _lines.get_size() ) + 1 /* main() */ + 1 /* current line === last line */ );
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
	for ( yaal::hcore::HString const& line : _classes ) {
		cout << line << endl;
	}
	if ( lineNo <= static_cast<int>( _imports.get_size() + 1 ) ) {
		cout << offending << ( _lastLine.back() != ';' ? ";" : "" ) << endl;
	}
	cout << "main() {" << endl;
	for ( yaal::hcore::HString const& line : _lines ) {
		cout << "\t" << line << endl;
	}
	if ( lineNo > static_cast<int>( _imports.get_size() + 1 ) ) {
		cout << "\t" << offending << ( offending.back() != '}' ? ";" : "" ) << endl;
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

