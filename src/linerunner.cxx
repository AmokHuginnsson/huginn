/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <csignal>

#include <yaal/hcore/hfile.hxx>
#include <yaal/tools/ansi.hxx>
#include <yaal/tools/signals.hxx>
#include <yaal/tools/executingparser.hxx>
#include <yaal/tools/hterminal.hxx>
#include <yaal/tools/tools.hxx>
#include <yaal/tools/stringalgo.hxx>
#include <yaal/tools/huginn/functionreference.hxx>
#include <yaal/tools/huginn/helper.hxx>

M_VCSID( "$Id: " __ID__ " $" )
M_VCSID( "$Id: " __TID__ " $" )

#include "config.hxx"

#ifdef USE_REPLXX
#	include <replxx.hxx>
#	define REPL_print repl.print
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
	, _symbolToTypeCache()
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
	_symbolToTypeCache.clear();
	_locals.clear();
	_source.clear();
	_description.clear();
	_streamCache.clear();
	_huginn = make_pointer<HHuginn>();
	_huginn->reset();
	_interrupted = false;
	_lastLine.clear();
	_lastLineType = LINE_TYPE::NONE;
	_definitionsLineCount = 0;
	_definitions.clear();
	_imports.clear();
	_lines.clear();
	settingsObserver._maxCallStackSize = _huginnMaxCallStack_;
	settingsObserver._modulePath = setup._modulePath;
	return;
	M_EPILOG
}

namespace {
yaal::hcore::HString first_name( yaal::hcore::HString const& input_ ) {
	int long nonNameIdx( input_.find_one_of( hcore::HString( character_class<CHARACTER_CLASS::WHITESPACE>().data() ).append( '(' ) ) );
	return ( input_.substr( 0, nonNameIdx != yaal::hcore::HString::npos ? nonNameIdx : 0 ) );
}
}

void HLineRunner::do_introspect( yaal::tools::HIntrospecteeInterface& introspectee_ ) {
	M_PROLOG
	_locals = introspectee_.get_locals( 0 );
	return;
	M_EPILOG
}

bool HLineRunner::add_line( yaal::hcore::HString const& line_, bool persist_ ) {
	M_PROLOG
	static char const inactive[] = ";\t \r\n\a\b\f\v";
	static HHuginn grammarSource;
	static executing_parser::HRule grammar( grammarSource.make_engine() );
	static executing_parser::HRuleBase const* importRule( grammar.find( "importStatement" ) );
	static executing_parser::HRuleBase const* fromRule( grammar.find( "fromStatement" ) );
	static executing_parser::HRuleBase const* classRule( grammar.find( "classDefinition" ) );
	static executing_parser::HRuleBase const* enumRule( grammar.find( "enumDefinition" ) );
	static executing_parser::HRuleBase const* functionRule( grammar.find( "functionDefinition" ) );
	static HExecutingParser importParser( *importRule, HExecutingParser::INIT_MODE::TRUST_GRAMMAR );
	static HExecutingParser fromParser( *fromRule, HExecutingParser::INIT_MODE::TRUST_GRAMMAR );
	static HExecutingParser classParser( *classRule, HExecutingParser::INIT_MODE::TRUST_GRAMMAR );
	static HExecutingParser enumParser( *enumRule, HExecutingParser::INIT_MODE::TRUST_GRAMMAR );
	static HExecutingParser functionParser( *functionRule, HExecutingParser::INIT_MODE::TRUST_GRAMMAR );

	M_ASSERT( ! line_.is_empty() );

	_lastLineType = LINE_TYPE::NONE;

	HHuginn preprocessor;
	HStringStream src( line_ );
	preprocessor.load( src );
	preprocessor.preprocess();
	preprocessor.dump_preprocessed_source( src );
	hcore::HString input( src.string() );

	input.trim( inactive );

	bool isImport( importParser( to_string( input ).append( ";" ) ) || fromParser( to_string( input ).append( ";" ) ) );
	bool isDefinition( classParser( input ) || enumParser( input ) || ( functionParser( input ) && ! is_keyword( first_name( input ) ) ) );

	/* Keep documentation strings. */
	input.assign( line_ ).trim( inactive );

	_streamCache.reset();

	for ( HEntry const& import : _imports ) {
		_streamCache << import.data() << "\n";
	}
	if ( isImport ) {
		_streamCache << input << ";\n";
	}
	_streamCache << "\n";

	for ( HEntry const& definition : _definitions ) {
		_streamCache << definition.data() << "\n\n";
	}
	if ( isDefinition ) {
		_streamCache << input << "\n\n";
	}

	_streamCache << "main() {\n";
	for ( HEntry const& line : _lines ) {
		_streamCache << '\t' << line.data() << "\n";
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
		ok = _huginn->compile( settingsObserver._modulePath, HHuginn::COMPILER::BE_SLOPPY, this );
	}

	_lastLineType = isImport ? LINE_TYPE::IMPORT : ( isDefinition ? LINE_TYPE::DEFINITION : ( ok ? LINE_TYPE::CODE : LINE_TYPE::NONE ) );
	_lastLine = input;
	if ( ok ) {
		if ( gotInput ) {
			_lines.emplace_back( _lastLine, persist_ );
		} else if ( isImport ) {
			_imports.emplace_back( _lastLine, persist_ );
		} else if ( isDefinition ) {
			_definitions.emplace_back( _lastLine, persist_ );
			_definitionsLineCount += static_cast<int>( count( _lastLine.cbegin(), _lastLine.cend(), '\n'_ycp ) + 1 );
		}
		_description.prepare( *_huginn );
		_symbolToTypeCache.clear();
	}
	return ( ok );
	M_EPILOG
}

HHuginn::value_t HLineRunner::execute( void ) {
	M_PROLOG
	bool ok( true );
	if ( ( ok = _huginn->execute() ) ) {
		clog << _source;
	} else {
		undo();
	}
	_description.note_locals( _locals, ok );
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
	hcore::HString m( _huginn->error_message() );
	if ( setup._errorContext != ERROR_CONTEXT::VISIBLE ) {
		int long p( m.find( ": " ) );
		if ( p != hcore::HString::npos ) {
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
#ifdef USE_REPLXX
	replxx::Replxx repl;
#endif
	if ( setup._errorContext == ERROR_CONTEXT::VISIBLE ) {
		for ( HEntry const& line : _imports ) {
			utf8.assign( line.data() );
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
		for ( HEntry const& line : _definitions ) {
			utf8.assign( line.data() );
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
			utf8.assign( _lines[i].data() );
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

	for ( HEntry const& import : _imports ) {
		_streamCache << import.data() << "\n";
	}
	_streamCache << "\n";

	for ( HEntry const& definition : _definitions ) {
		_streamCache << definition.data() << "\n\n";
	}

	_streamCache << "main() {\n";
	for ( HEntry const& line : _lines ) {
		_streamCache << '\t' << line.data() << "\n";
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

HLineRunner::words_t const& HLineRunner::words( bool inDocContext_ ) {
	M_PROLOG
	if ( _description.symbols( inDocContext_ ).is_empty() ) {
		prepare_source();
		_huginn = make_pointer<HHuginn>();
		_huginn->load( _streamCache, _tag );
		_huginn->preprocess();
		if ( _huginn->parse() && _huginn->compile( settingsObserver._modulePath, HHuginn::COMPILER::BE_SLOPPY ) ) {
			_description.prepare( *_huginn );
		}
	}
	return ( _description.symbols( inDocContext_ ) );
	M_EPILOG
}

HLineRunner::words_t const& HLineRunner::members( yaal::hcore::HString const& symbol_, bool inDocContext_ ) {
	words( inDocContext_ ); // gen docs.
	return ( _description.members( symbol_ ) );
}

HDescription::words_t const& HLineRunner::dependent_symbols( yaal::hcore::HString const& symbol_, bool inDocContext_ ) {
	M_PROLOG
	words( inDocContext_ ); // gen docs.
	words_t const* w( &_description.members( symbol_ ) );
	if ( w->is_empty() ) {
		hcore::HString sym( symbol_ );
		if ( inDocContext_ ) {
			words_t tokens( tools::string::split( symbol_, "." ) );
			tokens.front() = _description.package_alias( tokens.front() );
			if ( ! tokens.front().is_empty() ) {
				sym = tools::string::join( tokens, "." );
			}
		}
		w = &_description.members( symbol_type_name( sym ) );
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

HLineRunner::entries_t const& HLineRunner::imports( void ) const {
	M_PROLOG
	return ( _imports );
	M_EPILOG
}

yaal::hcore::HString HLineRunner::doc( yaal::hcore::HString const& symbol_, bool inDocContext_ ) {
	M_PROLOG
	words( inDocContext_ ); // gen docs.
	return ( _description.doc( symbol_ ) );
	M_EPILOG
}

yaal::tools::huginn::HClass const* HLineRunner::symbol_type_id( yaal::tools::HHuginn::value_t const& value_ ) {
	M_PROLOG
	HClass const* c( value_->get_class() );
	if ( c->type_id() == HHuginn::TYPE::FUNCTION_REFERENCE ) {
		HClass const* juncture( static_cast<HFunctionReference const*>( value_.raw() )->juncture() );
		if ( !! juncture ) {
			c = juncture;
		}
	}
	return ( c );
	M_EPILOG
}

yaal::tools::huginn::HClass const* HLineRunner::symbol_type_id( yaal::hcore::HString const& symbol_ ) {
	M_PROLOG
	if ( symbol_.is_empty() ) {
		return ( nullptr );
	}
	symbol_types_t::const_iterator it( _symbolToTypeCache.find( symbol_ ) );
	if ( it != _symbolToTypeCache.end() ) {
		return ( it->second );
	}
	if ( ! _description.members( symbol_ ).is_empty() ) {
		return ( nullptr );
	}
	tools::huginn::HClass const* c( nullptr );
	bool found( false );
	for ( HIntrospecteeInterface::HVariableView const& vv : _locals ) {
		if ( vv.name() == symbol_ ) {
			HHuginn::value_t v( vv.value() );
			if ( !! v ) {
				c = symbol_type_id( v );
				found = true;
			}
			break;
		}
	}
	if ( ! found ) {
		symbol_types_t keep( yaal::move( _symbolToTypeCache ) );
		if ( add_line( symbol_, false ) ) {
			HHuginn::value_t res( execute() );
			if ( !! res ) {
				c = symbol_type_id( res );
				undo();
				_huginn->reset( 1 );
				found = true;
			}
		}
		_symbolToTypeCache.swap( keep );
	}
	if ( found ) {
		_symbolToTypeCache[symbol_] = c;
	}
	return ( c );
	M_EPILOG
}

yaal::hcore::HString HLineRunner::symbol_type_name( yaal::hcore::HString const& symbol_ ) {
	M_PROLOG
	tools::huginn::HClass const* c( symbol_type_id( symbol_ ) );
	return ( c ? full_class_name( *c->runtime(), c, false ) : symbol_ );
	M_EPILOG
}

HDescription::SYMBOL_KIND HLineRunner::symbol_kind( yaal::hcore::HString const& name_ ) const {
	M_PROLOG
	return ( _description.symbol_kind( name_ ) );
	M_EPILOG
}

void HLineRunner::load_session( yaal::tools::filesystem::path_t const& path_, bool persist_ ) {
	M_PROLOG
	if ( ! ( filesystem::exists( path_ ) && filesystem::is_regular_file( path_ ) ) ) {
		return;
	}
	HFile f( path_, HFile::OPEN::READING );
	if ( ! f ) {
		settingsObserver._modulePath = setup._modulePath;
		return;
	}
	LINE_TYPE currentSection( LINE_TYPE::NONE );
	hcore::HString line;
	hcore::HString definition;
	auto defCommit = [this, &definition, &persist_]() {
		if ( ! definition.is_empty() ) {
			_definitions.emplace_back( definition, persist_ );
			_definitionsLineCount += static_cast<int>( count( definition.cbegin(), definition.cend(), '\n'_ycp ) + 1 );
			definition.clear();
		}
	};
	int extraLines( 0 );
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
				HScopedValueReplacement<bool> jupyter( setup._jupyter, false );
				meta( *this, line );
				++ extraLines;
			}
			continue;
		}
		switch ( currentSection ) {
			case ( LINE_TYPE::IMPORT ): {
				if ( find( _imports.begin(), _imports.end(), line ) == _imports.end() ) {
					_imports.emplace_back( line, persist_ );
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
				_lines.emplace_back( line, persist_ );
			} break;
			default: {
			}
		}
	}
	prepare_source();
	_huginn->reset();
	_huginn->load( _streamCache, _tag, extraLines );
	_huginn->preprocess();
	if ( _huginn->parse() && _huginn->compile( settingsObserver._modulePath, HHuginn::COMPILER::BE_SLOPPY, this ) && _huginn->execute() ) {
		_description.prepare( *_huginn );
		_description.note_locals( _locals, true );
	} else {
		cout << "Holistic session reload failed (" << path_ << "):\n" << _huginn->error_message() << "\nPerforming step-by-step reload." << endl;
		reset();
		f.seek( 0, HFile::SEEK::BEGIN );
		hcore::HString buffer;
		currentSection = LINE_TYPE::NONE;
		while ( getline( f, line ).good() ) {
			if ( line == "//code" ) {
				currentSection = LINE_TYPE::CODE;
			} else if ( line.find( "//set " ) == 0 ) {
				HScopedValueReplacement<bool> jupyter( setup._jupyter, false );
				meta( *this, line );
			} else if ( line.find( "import " ) == 0 ) {
				if ( add_line( line, persist_ ) ) {
					execute();
				}
			} else if ( line.find( "//" ) != 0 ) {
				buffer.append( line ).append( "\n" );
				if ( line.is_empty() ) {
					if ( add_line( buffer, persist_ ) ) {
						execute();
					} else if ( currentSection == LINE_TYPE::CODE ) {
						for ( hcore::HString const& code : tools::string::split( buffer, "\n" ) ) {
							if ( add_line( code, persist_ ) ) {
								execute();
							}
						}
					}
					buffer.clear();
				}
			}
		}
		if ( ! buffer.is_empty() && add_line( buffer, persist_ ) ) {
			execute();
		} else if ( currentSection == LINE_TYPE::CODE ) {
			for ( hcore::HString const& code : tools::string::split( buffer, "\n" ) ) {
				if ( add_line( code, persist_ ) ) {
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
	hcore::HString escaped;
	code_point_t quote( 0 );
	hcore::HString literal;
	hcore::HString* s( &escaped );
	for ( hcore::HString::const_iterator it( str_.begin() ), end( str_.end() ); it != end; ++ it ) {
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
			quote = 0_ycp;
			s = &escaped;
			util::escape( literal, executing_parser::_escapes_ );
			escaped.append( literal );
			literal.clear();
		} else if ( ( ( cur == '"'_ycp ) || ( cur == '\''_ycp ) ) && ( quote == 0 ) ) {
			s = &literal;
			quote = cur;
		}
		s->push_back( cur );
	}
	return ( escaped );
	M_EPILOG
}

}

void HLineRunner::save_session( yaal::tools::filesystem::path_t const& path_ ) {
	M_PROLOG
	HFile f( path_, HFile::OPEN::WRITING | HFile::OPEN::TRUNCATE );
	hcore::HString escaped;
	if ( !! f ) {
		f << "// This file was generated automatically, do not edit it!" << endl;
		for ( rt_settings_t::value_type const& s : rt_settings() ) {
			f << "//set " << s.first << "=" << s.second << endl;
		}
		f << "//import" << endl;
		for ( HEntry const& import : _imports ) {
			if ( ! import.persist() ) {
				continue;
			}
			f << import.data() << endl;
		}
		f << "//definition" << endl;
		for ( HEntry const& definition : _definitions ) {
			if ( ! definition.persist() ) {
				continue;
			}
			f << escape( definition.data() ) << "\n" << endl;
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

