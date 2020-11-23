/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <csignal>

#include <yaal/hcore/hfile.hxx>
#include <yaal/hcore/hclock.hxx>
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
#include "quotes.hxx"
#include "setup.hxx"
#include "settings.hxx"
#include "meta.hxx"

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;
using namespace yaal::tools::util;
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
	, _sessionFiles()
	, _tag( tag_ )
	, _ignoreIntrospection( false )
	, _executing( false )
	, _errorMessage()
	, _errorLine( 0 )
	, _errorColumn( 0 )
	, _mutex( HMutex::TYPE::RECURSIVE ) {
	M_PROLOG
	HHuginn::disable_grammar_verification();
	reset_session( true );
	HSignalService::get_instance().register_handler( SIGINT, hcore::call( &HLineRunner::handle_interrupt, this, _1 ) );
	return;
	M_EPILOG
}

void HLineRunner::reset( void ) {
	M_PROLOG
	HLock l( _mutex );
	reset_session( true );
	return;
	M_EPILOG
}

void HLineRunner::reload( void ) {
	M_PROLOG
	HLock l( _mutex );
	entries_t sessionFiles( _sessionFiles );
	reset_session( true );
	for ( HEntry const& sessionFile : sessionFiles ) {
		load_session_impl( sessionFile.data(), sessionFile.persist(), false );
	}
	_sessionFiles = sessionFiles;
	return;
	M_EPILOG
}

void HLineRunner::reset_session( bool full_ ) {
	M_PROLOG
	HLock l( _mutex );
	_errorMessage.clear();
	_errorLine = 0;
	_errorColumn = 0;
	_executing = false;
	_ignoreIntrospection = false;
	if ( full_ ) {
		_sessionFiles.clear();
	}
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

void HLineRunner::save_error_info( void ) {
	M_PROLOG
	_errorMessage.assign( _huginn->error_message() );
	_errorLine = _huginn->error_coordinate().line();
	_errorColumn = _huginn->error_coordinate().column();
	return;
	M_EPILOG
}

namespace {
yaal::hcore::HString first_name( yaal::hcore::HString const& input_ ) {
	int long nonNameIdx( input_.find_one_of( hcore::HString( character_class<CHARACTER_CLASS::WHITESPACE>().data() ).append( '(' ) ) );
	return ( input_.substr( 0, nonNameIdx != yaal::hcore::HString::npos ? nonNameIdx : 0 ) );
}
static yaal::hcore::HString const _noop_ = "/**/";
}

void HLineRunner::do_introspect( yaal::tools::HIntrospecteeInterface& introspectee_ ) {
	M_PROLOG
	if ( _ignoreIntrospection ) {
		return;
	}
	_locals = introspectee_.get_locals( 0 );
	return;
	M_EPILOG
}

bool HLineRunner::add_line( yaal::hcore::HString const& line_, bool persist_ ) {
	M_PROLOG
	HLock l( _mutex );
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

	if ( input.is_empty() && ( line_ != _noop_ ) ) {
		return ( true );
	}

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
		gotSemi = ok = amend( ";\n}\n" );
	} else if ( ! ( isDefinition || isImport ) && ! input.is_empty() && ( input.back() != ';' ) && ( input != _noop_ ) ) {
		ok = amend( "\nnone;\n}\n" );
	}
	if ( isImport || gotSemi ) {
		input.push_back( ';'_ycp );
	}
	if ( ok ) {
		ok = _huginn->compile( settingsObserver._modulePath, HHuginn::COMPILER::BE_SLOPPY, this );
	}

	_lastLineType = ok ? ( isImport ? LINE_TYPE::IMPORT : ( isDefinition ? LINE_TYPE::DEFINITION : LINE_TYPE::CODE ) ) : LINE_TYPE::NONE;
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
	if ( ! ok ) {
		save_error_info();
	}
	return ( ok );
	M_EPILOG
}

bool HLineRunner::amend(  yaal::hcore::HString const& with_ ) {
	M_PROLOG
	_source.erase( _source.get_length() - 3 );
	_source.append( with_ );
	_streamCache.str( _source );
	_huginn->reset();
	_huginn->load( _streamCache, _tag );
	_huginn->preprocess();
	return ( _huginn->parse() );
	M_EPILOG
}

HHuginn::value_t HLineRunner::execute( void ) {
	M_PROLOG
	return ( do_execute( true ) );
	M_EPILOG
}

HHuginn::value_t HLineRunner::do_execute( bool trimCode_ ) {
	M_PROLOG
	HLock l( _mutex );
	if ( _lastLineType == LINE_TYPE::NONE ) {
		return ( _huginn->value( nullptr ) );
	}
	HScopedValueReplacement<bool> markExecution( _executing, true );
	yaal::tools::HIntrospecteeInterface::variable_views_t localsOrig( _locals );
	int localVarCount( static_cast<int>( _locals.get_size() ) );
	int newStatementCount( _huginn->new_statement_count() );
	return ( finalize_execute( _huginn->execute(), trimCode_, localsOrig, localVarCount, newStatementCount ) );
	M_EPILOG
}

HLineRunner::HTimeItResult::HTimeItResult( int count_, yaal::hcore::time::duration_t total_ )
	: _count( count_ )
	, _total( total_ )
	, _iteration( count_ > 0 ? total_ / count_ : time::duration_t( -1 ) ) {
}

HLineRunner::HTimeItResult HLineRunner::timeit( int count_ ) {
	M_PROLOG
	HLock l( _mutex );
	HScopedValueReplacement<bool> markExecution( _executing, true );
	yaal::tools::HIntrospecteeInterface::variable_views_t localsOrig( _locals );
	int localVarCount( static_cast<int>( _locals.get_size() ) );
	int newStatementCount( _huginn->new_statement_count() );
	int i( 0 );
	bool ok( true );
	time::duration_t preciseTime( 0 );
	while ( ok && ( i < count_ ) ) {
		ok = _huginn->execute();
		preciseTime += _huginn->execution_time();
		++ i;
	}
	if ( ! ok ) {
		-- i;
	}
	HTimeItResult timeResult( i, preciseTime );
	finalize_execute( ok, true, localsOrig, localVarCount, newStatementCount );
	return ( timeResult );
	M_EPILOG
}

yaal::tools::HHuginn::value_t HLineRunner::finalize_execute( bool ok_, bool trimCode_, yaal::tools::HIntrospecteeInterface::variable_views_t const& localsOrig_, int localVarCount_, int newStatementCount_ ) {
	M_PROLOG
	HHuginn::value_t res;
	if ( ok_ ) {
		clog << _source;
		res = _huginn->result();
		if (
			trimCode_
			&& ! _ignoreIntrospection
			&& ( static_cast<int>( _locals.get_size() ) == localVarCount_ )
			&& ! _lines.is_empty()
			&& ( _lastLineType == LINE_TYPE::CODE )
		) {
			_lines.pop_back();
			_lastLineType = LINE_TYPE::TRIMMED_CODE;
			_huginn->reset( newStatementCount_ );
		}
		_description.note_locals( _locals );
	} else {
		save_error_info();
		undo();
		_locals = localsOrig_;
	}
	mend_interrupt();
	return ( res );
	M_EPILOG
}

void HLineRunner::mend_interrupt( void ) {
	if ( ! _interrupted ) {
		return;
	}
	_interrupted = false;
	yaal::_isKilled_ = false;
	return;
}

bool HLineRunner::use_result( void ) const {
	return ( ( _lastLineType == LINE_TYPE::CODE ) || ( _lastLineType == LINE_TYPE::TRIMMED_CODE ) );
}

void HLineRunner::undo( void ) {
	M_PROLOG
	HLock l( _mutex );
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

void HLineRunner::mend( void ) {
	M_PROLOG
	if ( ( _lastLineType == LINE_TYPE::NONE ) || ( _lastLineType == LINE_TYPE::TRIMMED_CODE ) ) {
		add_line( _noop_, false );
		_lines.pop_back();
	}
	return;
	M_EPILOG
}

HHuginn::value_t HLineRunner::call( yaal::hcore::HString const& name_, yaal::tools::HHuginn::values_t const& args_, yaal::hcore::HStreamInterface* errStream_, bool allowMissing_ ) {
	M_PROLOG
	HLock l( _mutex );
	HScopedValueReplacement<bool> markExecution( _executing, true );
	HScopedValueReplacement<bool> ignoreIntrospection( _ignoreIntrospection, true );
	mend();
	HHuginn::value_t res( _huginn->call( name_, args_ ) );
	if ( ! res && errStream_ ) {
		hcore::HString const& errMsg( _huginn->error_message() );
		hcore::HString missingFunction( "Function `"_ys.append( name_ ).append( "(...)` is not defined." ) );
		hcore::HString missingSymbol( "Symbol `"_ys.append( name_ ).append( "` is not defined." ) );
		if (
			! allowMissing_
			|| (
				( errMsg.find( missingFunction ) == hcore::HString::npos )
				&& ( errMsg.find( missingSymbol ) == hcore::HString::npos )
			)
		) {
			*errStream_ << errMsg << endl;
		}
	}
	return ( res );
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
	hcore::HString m( _errorMessage );
	if ( setup._errorContext != ERROR_CONTEXT::VISIBLE ) {
		int long p( m.find( ": " ) );
		if ( p != hcore::HString::npos ) {
			m.shift_left( p + 2 );
		}
	}
	int mainLineNo( static_cast<int>( _imports.get_size() + 1 + _definitionsLineCount + _definitions.get_size() + 1 ) );
	if ( _lastLineType == LINE_TYPE::DEFINITION ) {
		mainLineNo += static_cast<int>( count( _lastLine.begin(), _lastLine.end(), '\n'_ycp ) + 1 );
	}
	int colNo( _errorColumn - 1 /* col no is 1 bases */ - ( _errorLine > mainLineNo ? 1 /* we add tab key to user input */ : 0 ) );
	bool useColor( is_a_tty( cout ) && ! ( setup._noColor || setup._jupyter ) );
	hcore::HString offending;
	int lineCount( static_cast<int>( _imports.get_size() + _definitionsLineCount + _definitions.get_size() + _lines.get_size() ) + 1 /* empty line after imports */ + 1 /* main() */ + 1 /* current line === last line */ );
	if ( useColor && ( _errorLine <= lineCount ) && ( colNo < static_cast<int>( _lastLine.get_length() ) ) ) {
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
		if ( _errorLine <= static_cast<int>( _imports.get_size() + 1 ) ) {
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
		if ( ( _errorLine > static_cast<int>( _imports.get_size() + 1 ) ) && ( _errorLine <= mainLineNo ) ) {
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
		if ( _errorLine > mainLineNo ) {
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
	HLock l( _mutex );
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
	M_PROLOG
	HLock l( _mutex );
	words( inDocContext_ ); // gen docs.
	return ( _description.members( symbol_ ) );
	M_EPILOG
}

HDescription::words_t const& HLineRunner::dependent_symbols( yaal::hcore::HString const& symbol_, bool inDocContext_ ) {
	M_PROLOG
	HLock l( _mutex );
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
	HLock l( _mutex );
	prepare_source();
	return ( _source );
	M_EPILOG
}

HLineRunner::entries_t const& HLineRunner::imports( void ) const {
	M_PROLOG
	HLock l( _mutex );
	return ( _imports );
	M_EPILOG
}

yaal::hcore::HString HLineRunner::doc( yaal::hcore::HString const& symbol_, bool inDocContext_ ) {
	M_PROLOG
	HLock l( _mutex );
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
			HHuginn::value_t res( do_execute( false ) );
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
	HLock l( _mutex );
	tools::huginn::HClass const* c( symbol_type_id( symbol_ ) );
	return ( c ? full_class_name( *c->runtime(), c, false ) : symbol_ );
	M_EPILOG
}

HDescription::SYMBOL_KIND HLineRunner::symbol_kind( yaal::hcore::HString const& name_ ) const {
	M_PROLOG
	HLock l( _mutex );
	return ( _description.symbol_kind( name_ ) );
	M_EPILOG
}

void HLineRunner::load_session( yaal::tools::filesystem::path_t const& path_, bool persist_, bool lenient_ ) {
	M_PROLOG
	HLock l( _mutex );
	try {
		load_session_impl( path_, persist_, true );
	} catch ( HException const& e ) {
		if ( ! lenient_ ) {
			cerr << e.what() << endl;
		}
	}
	return;
	M_EPILOG
}

void HLineRunner::load_session_impl( yaal::tools::filesystem::path_t const& path_, bool persist_, bool direct_ ) {
	M_PROLOG
	HLock l( _mutex );
	filesystem::path_t path( path_ );
	denormalize_path( path, true );
	if ( ! filesystem::exists( path ) ) {
		throw HRuntimeException( "`"_ys.append( path_ ).append( "` does not exist." ) );
	}
	if ( ! filesystem::is_regular_file( path ) ) {
		throw HRuntimeException( "`"_ys.append( path_ ).append( "` is not a regular file." ) );
	}
	HFile f( path, HFile::OPEN::READING );
	if ( ! f ) {
		settingsObserver._modulePath = setup._modulePath;
		throw HRuntimeException( "Failed to open `"_ys.append( path_ ).append( "` for reading." ) );
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
				++ extraLines;
			} else if ( line == "//definition" ) {
				currentSection = LINE_TYPE::DEFINITION;
				++ extraLines;
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
	bool ok( _huginn->parse() && _huginn->compile( settingsObserver._modulePath, HHuginn::COMPILER::BE_SLOPPY, this ) );
	if ( ok ) {
		HScopedValueReplacement<bool> markExecution( _executing, true );
		ok = _huginn->execute();
	}
	if (  ok  ) {
		_description.prepare( *_huginn );
		_description.note_locals( _locals );
	} else {
		cout << "Holistic session reload failed (" << path_ << "):\n" << _huginn->error_message() << "\nPerforming step-by-step reload." << endl;
		reset_session( false );
		if ( direct_ ) {
			for ( HEntry const& sessionFile : _sessionFiles ) {
				load_session_impl( sessionFile.data(), sessionFile.persist(), false );
			}
		}
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
					do_execute( false );
				}
			} else if ( line.find( "//" ) != 0 ) {
				buffer.append( line ).append( "\n" );
				if ( line.is_empty() ) {
					if ( add_line( buffer, persist_ ) ) {
						do_execute( false );
					} else if ( currentSection == LINE_TYPE::CODE ) {
						for ( hcore::HString const& code : tools::string::split( buffer, "\n" ) ) {
							if ( add_line( code, persist_ ) ) {
								do_execute( true );
							}
						}
					}
					buffer.clear();
				}
			}
		}
		if ( ! buffer.is_empty() && add_line( buffer, persist_ ) ) {
			do_execute( false );
		} else if ( currentSection == LINE_TYPE::CODE ) {
			for ( hcore::HString const& code : tools::string::split( buffer, "\n" ) ) {
				if ( code.is_empty() ) {
					continue;
				}
				if ( add_line( code, persist_ ) ) {
					do_execute( true );
				}
			}
		}
	}
	if ( direct_ ) {
		_sessionFiles.emplace_back( path_, persist_ );
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
			util::escape( literal, cxx_escape_table() );
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
	HLock lck( _mutex );
	add_line( "none", false );
	do_execute( false );
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
		HEntry const* entry( nullptr );
		for ( HIntrospecteeInterface::HVariableView const& vv : _locals ) {
			HHuginn::value_t v( vv.value() );
			if ( ! v ) {
				continue;
			}
			for ( HEntry const& l : _lines ) {
				hcore::HString name( tools::string::split( l.data(), "=" )[0].trim() );
				if ( name == vv.name() ) {
					entry = &l;
					break;
				}
			}
			if ( ! entry->persist() ) {
				continue;
			}
			f << vv.name() << " = " << escape( code( v, _huginn.raw() ) ) << ";" << endl;
		}
		f << "// vim: ft=huginn" << endl;
	} else if ( ! setup._session.is_empty() ) {
		cerr << "Cannot create session persistence file: " << f.get_error() << endl;
	}
	return;
	M_EPILOG
}

yaal::tools::HIntrospecteeInterface::variable_views_t const& HLineRunner::locals( void ) const {
	M_PROLOG
	HLock l( _mutex );
	return ( _locals );
	M_EPILOG
}

HLineRunner::entries_t const& HLineRunner::definitions( void ) const {
	M_PROLOG
	HLock l( _mutex );
	return ( _definitions );
	M_EPILOG
}

bool HLineRunner::is_executing( void ) const {
	return ( _executing );
}

}

