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
#include <yaal/tools/filesystem.hxx>
#include <yaal/tools/executingparser.hxx>
#include <yaal/tools/hterminal.hxx>
#include <yaal/tools/tools.hxx>

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
#include "settings.hxx"
#include "meta.hxx"

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;
using namespace yaal::tools::huginn;

namespace yaal { namespace tools { namespace huginn {
bool is_keyword( yaal::hcore::HString const& );
} } }

namespace huginn {

HLineRunner::HLineRunner( yaal::hcore::HString const& tag_ )
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
	, _tag( tag_ ) {
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
	settingsObserver._maxCallStackSize = _huginnMaxCallStack_;
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
	static HExecutingParser importParser( *importRule, HExecutingParser::INIT_MODE::TRUST_GRAMMAR );
	static HExecutingParser classParser( *classRule, HExecutingParser::INIT_MODE::TRUST_GRAMMAR );
	static HExecutingParser functionParser( *functionRule, HExecutingParser::INIT_MODE::TRUST_GRAMMAR );
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
	_huginn->load( _streamCache, _tag );
	_huginn->preprocess();
	bool ok( _huginn->parse() );

	bool gotSemi( false );
	if ( ! ok ) {
		_source.erase( _source.get_length() - 3 );
		_source.append( ";\n}\n" );
		_streamCache.str( _source );
		_huginn->reset();
		_huginn->load( _streamCache, _tag );
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
	HString m( _huginn->error_message() );
	if ( setup._errorContext != ERROR_CONTEXT::VISIBLE ) {
		int long p( m.find( ": " ) );
		if ( p != HString::npos ) {
			m.shift_left( p + 2 );
		}
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
	if ( setup._errorContext == ERROR_CONTEXT::VISIBLE ) {
		for ( yaal::hcore::HString const& line : _imports ) {
			utf8.assign( line );
			REPL_print( "%s\n", utf8.c_str() );
		}
	}
	if ( setup._errorContext != ERROR_CONTEXT::HIDDEN ) {
		if ( lineNo <= static_cast<int>( _imports.get_size() + 1 ) ) {
			utf8.assign( offending );
			REPL_print( "%s\n", utf8.c_str() );
		}
	}
	if ( setup._errorContext == ERROR_CONTEXT::VISIBLE ) {
		if ( ! _imports.is_empty() ) {
			REPL_print( "\n" );
		}
		for ( yaal::hcore::HString const& line : _definitions ) {
			utf8.assign( line );
			REPL_print( "%s\n\n", utf8.c_str() );
		}
	}
	if ( setup._errorContext != ERROR_CONTEXT::HIDDEN ) {
		if ( ( lineNo > static_cast<int>( _imports.get_size() + 1 ) ) && ( lineNo <= mainLineNo ) ) {
			utf8.assign( offending );
			REPL_print( "%s\n\n", utf8.c_str() );
		}
	}
	if ( setup._errorContext == ERROR_CONTEXT::VISIBLE ) {
		REPL_print( "main() {\n" );
		for ( int i( 0 ), COUNT( static_cast<int>( _lines.get_size() ) - ( _lastLineType == LINE_TYPE::CODE ? 1 : 0 ) ); i < COUNT; ++ i ) {
			utf8.assign( _lines[i] );
			REPL_print( "\t%s\n", utf8.c_str() );
		}
	}
	if ( setup._errorContext != ERROR_CONTEXT::HIDDEN ) {
		if ( lineNo > mainLineNo ) {
			utf8.assign( offending );
			REPL_print( "\t%s\n", utf8.c_str() );
		}
	}
	if ( setup._errorContext == ERROR_CONTEXT::VISIBLE ) {
		REPL_print( "}\n" );
	}
	return ( m );
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
		_huginn->load( _streamCache, _tag );
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
	words_t const* w( &_description.methods( symbol_ ) );
	if ( w->is_empty() && add_line( "type("_ys.append( symbol_ ).append( ")" ) ) ) {
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

void HLineRunner::load_session( void ) {
	M_PROLOG
	HString path( setup._sessionDir + "/" + setup._session );
	if ( ! filesystem::is_regular_file( path ) ) {
		return;
	}
	HFile f( path, HFile::OPEN::READING );
	LINE_TYPE currentSection( LINE_TYPE::NONE );
	if ( !! f ) {
		HString line;
		HString definition;
		auto defCommit = [this, &definition]() {
			if ( ! definition.is_empty() ) {
				_definitions.push_back( definition );
				_definitionsLineCount += static_cast<int>( count( definition.cbegin(), definition.cend(), '\n'_ycp ) + 1 );
				definition.clear();
			}
		};
		while ( getline( f, line ).good() ) {
			if ( line.find( "//" ) == 0 ) {
				if ( line == "//import" ) {
					defCommit();
					currentSection = LINE_TYPE::IMPORT;
				} else if ( line == "//definition" ) {
					currentSection = LINE_TYPE::DEFINITION;
				} else if ( line == "//code" ) {
					defCommit();
					currentSection = LINE_TYPE::CODE;
				} else if ( line.find( "//set " ) == 0 ) {
					meta( *this, line );
				}
				continue;
			}
			switch ( currentSection ) {
				case ( LINE_TYPE::IMPORT ): {
					if ( find( _imports.begin(), _imports.end(), line ) == _imports.end() ) {
						_imports.push_back( line );
					}
				} break;
				case ( LINE_TYPE::DEFINITION ): {
					if ( line.is_empty() ) {
						defCommit();
					} else {
						if ( ! definition.is_empty() ) {
							definition.append( "\n" );
						}
						definition.append( line );
					}
				} break;
				case ( LINE_TYPE::CODE ): {
					_lines.push_back( line );
				} break;
				default: {
				}
			}
		}
		prepare_source();
		_huginn->load( _streamCache, _tag );
		_huginn->preprocess();
		if ( _huginn->parse() && _huginn->compile( setup._modulePath, HHuginn::COMPILER::BE_SLOPPY, this ) && _huginn->execute() ) {
			_description.prepare( *_huginn );
			_description.note_locals( _locals );
		} else {
			cout << "Holistic session reload failed:\n" << _huginn->error_message() << "\nPerforming step-by-step reload." << endl;
			reset();
			f.seek( 0, HFile::SEEK::SET );
			while ( getline( f, line ).good() ) {
				if ( add_line( line ) ) {
					execute();
				}
			}
		}
	}
	return;
	M_EPILOG
}

namespace {

yaal::hcore::HString escape( yaal::hcore::HString const& str_ ) {
	M_PROLOG
	HString escaped;
	code_point_t quote( 0 );
	HString literal;
	HString* s( &escaped );
	for ( HString::const_iterator it( str_.begin() ), end( str_.end() ); it != end; ++ it ) {
		code_point_t cur( *it );
		if ( cur == '\\'_ycp ) {
			s->push_back( cur );
			++ it;
			if ( it != end ) {
				s->push_back( *it );
				continue;
			}
			break;
		} else if ( cur == quote ) {
			s = &escaped;
			util::escape( literal, executing_parser::_escapes_ );
			escaped.append( literal );
			literal.clear();
		} else if ( ( cur == '"'_ycp ) || ( cur == '\''_ycp ) ) {
			s = &literal;
			quote = cur;
		}
		s->push_back( cur );
	}
	return ( escaped );
	M_EPILOG
}

}

void HLineRunner::save_session( void ) {
	M_PROLOG
	filesystem::create_directory( setup._sessionDir, 0700 );
	HString p( setup._sessionDir + "/" + setup._session );
	HFile f( p, HFile::OPEN::WRITING | HFile::OPEN::TRUNCATE );
	HString escaped;
	if ( !! f ) {
		f << "// This file was generated automatically, do not edit it!" << endl;
		for ( rt_settings_t::value_type const& s : rt_settings() ) {
			f << "//set " << s.first << "=" << s.second << endl;
		}
		f << "//import" << endl;
		for ( HString const& import : _imports ) {
			f << import << endl;
		}
		f << "//definition" << endl;
		for ( HString const& definition : _definitions ) {
			f << escape( definition ) << "\n" << endl;
		}
		f << "//code" << endl;
		for ( HIntrospecteeInterface::HVariableView const& vv : _locals ) {
			HHuginn::value_t v( vv.value() );
			if ( !! v ) {
				f << vv.name() << " = " << escape( to_string( v, _huginn.raw() ) ) << ";" << endl;
			}
		}
		f << "// vim: ft=huginn" << endl;
	} else if ( ! setup._session.is_empty() ) {
		cerr << "Cannot create session persistence file: " << f.get_error() << endl;
	}
	return;
	M_EPILOG
}

}

