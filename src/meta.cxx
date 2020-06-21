/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <cstring>
#include <cstdio>

#include <yaal/hcore/hcore.hxx>
#include <yaal/hcore/hfile.hxx>
#include <yaal/hcore/hclock.hxx>
#include <yaal/tools/ansi.hxx>
#include <yaal/tools/util.hxx>
#include <yaal/tools/stringalgo.hxx>
#include <yaal/tools/hterminal.hxx>
#include <yaal/tools/huginn/value.hxx>
M_VCSID( "$Id: " __ID__ " $" )
M_VCSID( "$Id: " __TID__ " $" )

#include "config.hxx"

#include "meta.hxx"
#include "settings.hxx"
#include "colorize.hxx"
#include "interactive.hxx"
#include "pager.hxx"
#include "commit_id.hxx"

#include "setup.hxx"

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;
using namespace yaal::tools::util;
using namespace yaal::ansi;

namespace huginn {

namespace {

template<typename... args_t>
void print( HRepl* repl_, args_t&&... args_ ) {
	if ( repl_ ) {
		repl_->print( std::forward<args_t>( args_ )... );
	} else {
		::printf( std::forward<args_t>( args_ )... );
	}
}

char const* start( char const* str_ ) {
	char const* s( str_ );
	if ( ! ( setup._jupyter || setup._noColor ) ) {
		if ( ! strcmp( str_, "**" ) ) {
			s = *brightcyan;
		} else if ( ! strcmp( str_, "`" ) ) {
			s = *brightgreen;
		}
	}
	return ( s );
}
char const* end( char const* str_ ) {
	return ( ( setup._jupyter || setup._noColor ) ? str_ : *reset );
}
yaal::hcore::HString highlight( yaal::hcore::HString const& str_ ) {
	static const HTheme theme(
		COLOR::FG_BRIGHTCYAN,
		COLOR::FG_CYAN,
		COLOR::FG_BRIGHTGREEN,
		COLOR::FG_WHITE
	);
	return ( ( ! ( setup._jupyter || setup._noColor ) ) ? util::highlight( str_, theme ) : str_ );
}

void doc( HRepl* repl_ ) {
	M_PROLOG
	char const docStr[] =
		"**//doc**              - show this help message\n"
		"**//doc** *symbol*       - show documentation for given *symbol*\n"
		"**//doc** *class*.*method* - show documentation for given *method* in *class*\n"
		"**//(quit**|**exit**|**bye)**  - end interactive session and exit program\n"
		"**//imports**          - show a list of imports currently in effect\n"
		"**//source**           - show current session source\n"
		"**//declarations**     - list locally declared classes and functions\n"
		"**//variables**        - list currently defined local variables\n"
		"**//set**              - show runner/engine options currently in effect\n"
		"**//set** *option*=*value* - set given *option* to new *value*\n"
		"**//reset**            - wipe out current session state\n"
		"**//reload**           - reload initialization files for current session\n"
		"**//load** *sess-file*   - load additional session file\n"
		"**//lsmagic**          - list available magic commands\n"
		"**//time**[*count*] *code* - measure execution time of given *code* running it *count* times\n"
		"**//version**          - print engine (yaal library) and runner version\n"
	;
	if ( setup._interactive && ! setup._noColor ) {
		print( repl_, "%s", HUTF8String( highlight( docStr ) ).raw() );
	} else {
		cout << to_string( docStr ).replace( "*", "" ).replace( "//", setup._jupyter ? "%" : "//" ) << flush;
	}
	return;
	M_EPILOG
}

void doc_topic( HLineRunner& lr_, HRepl* repl_, yaal::hcore::HString const& line_ ) {
	M_PROLOG
	hcore::HString symbol( line_.substr( 4 ) );
	symbol.trim_right( "()" );
	HUTF8String utf8( symbol );
	hcore::HString doc( lr_.doc( symbol, true ) );
	HDescription::words_t const& members( lr_.members( symbol, true ) );
	if ( ! doc.is_empty() ) {
		if ( ! members.is_empty() && ( doc.find( "`"_ys.append( symbol ).append( "`" ) ) == HString::npos ) ) {
			print( repl_, "%s%s%s - ", start( "`" ), utf8.c_str(), end( "`" ) );
		}
		int long ms( symbol.find( '.'_ycp ) );
		if ( ms != hcore::HString::npos ) {
			++ ms;
			symbol.erase( 0, ms );
			int long ss( doc.find( symbol ) );
			if ( ss != hcore::HString::npos ) {
				int long sl( symbol.get_length() );
				symbol.replace( "_", "\\_" );
				doc.replace( ss, sl, symbol );
			}
		}
		print( repl_, "%s\n", HUTF8String( highlight( doc ) ).c_str() );
		if ( ! members.is_empty() ) {
			print( repl_, "Class %s%s%s has following members:\n", start( "`" ), utf8.c_str(), end( "`" ) );
		}
	} else if ( ! members.is_empty() ) {
		print( repl_, "Class %s%s%s is not documented but has following members:\n", start( "`" ), utf8.c_str(), end( "`" ) );
	} else {
		print( repl_, "symbol %s%s%s is unknown or undocumented\n", start( "`" ), utf8.c_str(), end( "`" ) );
	}
	if ( ! members.is_empty() ) {
		for ( yaal::hcore::HString const& m : members ) {
			utf8.assign( m );
			print( repl_, "+ %s\n", utf8.c_str() );
		}
	}
	return;
	M_EPILOG
}

static char const SET[] = "set";

bool set( HLineRunner& lr_, yaal::hcore::HString const& line_, yaal::hcore::HString const& setting_ ) {
	M_PROLOG
	if ( line_.get_length() == ( static_cast<int>( sizeof ( SET ) ) - 1 ) ) {
		for ( rt_settings_t::value_type const& s : rt_settings( true ) ) {
			cout << s.first << "=" << s.second << endl;
		}
	} else if ( character_class<CHARACTER_CLASS::WHITESPACE>().has( line_[static_cast<int>( sizeof ( SET ) ) - 1] ) ) {
		apply_setting( *lr_.huginn(), setting_.substr( static_cast<int>( sizeof ( SET ) ) ) );
	} else {
		return ( false );
	}
	return ( true );
	M_EPILOG
}

void variables( HLineRunner& lr_ ) {
	M_PROLOG
	int maxLen( 0 );
	for ( HIntrospecteeInterface::HVariableView const& vv : lr_.locals() ) {
		if ( vv.name().get_length() > maxLen ) {
			maxLen = static_cast<int>( vv.name().get_length() );
		}
	}
	for ( HIntrospecteeInterface::HVariableView const& vv : lr_.locals() ) {
		cout << vv.name() << setw( maxLen - static_cast<int>( vv.name().get_length() ) + 3 ) << " - " << vv.value()->get_class()->name() << endl;
	}
	return;
	M_EPILOG
}

void declarations( HLineRunner& lr_ ) {
	M_PROLOG
	HStringStream ss;
	HHuginn preproc;
	for ( HLineRunner::HEntry const& d : lr_.definitions() ) {
		ss.str( d.data() );
		preproc.reset();
		preproc.load( ss );
		preproc.preprocess();
		preproc.dump_preprocessed_source( ss );
		string::tokens_t lines( string::split( ss.str(), "\n" ) );
		for ( hcore::HString& l : lines ) {
			l.trim_left();
			l.trim_right( " {" );
			if ( ! l.is_empty() ) {
				cout << l << endl;
				break;
			}
		}
	}
	return;
	M_EPILOG
}

void source( HLineRunner& lr_ ) {
	M_PROLOG
	if ( setup._jupyter ) {
		cout << lr_.source();
	} else if ( setup._interactive && ! setup._noColor ) {
		pager( colorize( lr_.source() ) );
	} else {
		pager( lr_.source() );
	}
	return;
	M_EPILOG
}

static char const HISTORY[] = "history";

bool history( HRepl& repl_, yaal::hcore::HString const& line_ ) {
	M_PROLOG
	if ( line_.get_length() == ( static_cast<int>( sizeof ( HISTORY ) ) - 1 ) ) {
		for ( HRepl::HHistoryEntry const& he : repl_.history() ) {
			cout << he.text() << endl;
		}
	} else if ( character_class<CHARACTER_CLASS::WHITESPACE>().has( line_[static_cast<int>( sizeof ( HISTORY ) ) - 1] ) ) {
		HString histCmd( line_.substr( static_cast<int>( sizeof ( HISTORY ) ) ) );
		histCmd.trim();
		if ( histCmd == "clear" ) {
			repl_.clear_history();
		} else {
			return ( false );
		}
	} else {
		return ( false );
	}
	return ( true );
	M_EPILOG
}

void timeit( HLineRunner& lr_, HString&& line_ ) {
	M_PROLOG
	static int const TIME_LEN( sizeof ( "time" ) - 1 );
	HString::size_type pos( line_.find_one_of( character_class<CHARACTER_CLASS::WHITESPACE>().data() ) );
	if ( pos == HString::npos ) {
		return;
	}
	HString countStr( line_.substr( TIME_LEN, pos - TIME_LEN ) );
	int count( 1 );
	try {
		count = ! countStr.is_empty() ? lexical_cast<int>( countStr ) : 1;
	} catch ( ... ) {
		return;
	}
	line_.shift_left( pos + 1 );
	line_.trim();
	if ( ! lr_.add_line( line_, false ) ) {
		cerr << lr_.err() << endl;
		return;
	}
	HLineRunner::HTimeItResult timeitResult( lr_.timeit( count ) );
	if ( timeitResult.count() < count ) {
		cerr << lr_.err() << endl;
	}
	if ( timeitResult.count() == 0 ) {
		/* only show error message */
	} else if ( count == 1 ) {
		cout << lexical_cast<HString>( timeitResult.total() ) << endl;
	} else if ( timeitResult.count() > 1 ) {
		cout
			<< "repetitions:    " << timeitResult.count() << "\n"
			<< "iteration time: " << lexical_cast<HString>( timeitResult.iteration() ) << "\n"
			<< "total time:     " << lexical_cast<HString>( timeitResult.total() )
			<< endl;
	}
	return;
	M_EPILOG
}

}

bool meta( HLineRunner& lr_, yaal::hcore::HString const& line_, HRepl* repl_ ) {
	M_PROLOG
	static char const LOAD[] = "load";
	bool isMeta( true );
	bool statusOk( true );
	hcore::HString line( line_ );
	line.trim_left();
	if ( line.find( "//" ) != 0 ) {
		return ( false );
	}
	line.shift_left( 2 );
	line.trim_left();
	hcore::HString setting( line );
	line.trim_right();
	HRegex time( "^time[0-9]*\\s" );
	try {
		HUTF8String utf8;
		if ( ( line == "quit" ) || ( line == "exit" ) || ( line == "bye" ) ) {
			setup._interactive = false;
		} else if ( line == "declarations" ) {
			declarations( lr_ );
		} else if ( line == "variables" ) {
			variables( lr_ );
		} else if ( line == "source" ) {
			source( lr_ );
		} else if ( repl_ && ( line.find( HISTORY ) == 0 ) ) {
			isMeta = history( *repl_, line );
		} else if (
			( line.get_length() >= static_cast<int>( sizeof ( LOAD ) + 1 ) )
			&& ( line.find( LOAD ) == 0 )
			&& character_class<CHARACTER_CLASS::WHITESPACE>().has( line[static_cast<int>( sizeof ( LOAD ) ) - 1] )
		) {
			line.shift_left( static_cast<int>( sizeof ( LOAD ) ) );
			line.trim();
			lr_.load_session( line, false, false );
		} else if ( time.matches( line ) ) {
			timeit( lr_, yaal::move( line ) );
		} else if ( line == "imports" ) {
			for ( HLineRunner::HEntry const& l : lr_.imports() ) {
				cout << l.data() << endl;
			}
		} else if ( line == "doc" ) {
			doc( repl_ );
		} else if ( ( line.find( "doc " ) == 0 ) || (  line.find( "doc\t" ) == 0  ) ) {
			doc_topic( lr_, repl_, line );
		} else if ( line.find( SET ) == 0 ) {
			isMeta = set( lr_, line, setting );
		} else if ( line == "reset" ) {
			lr_.reset();
		} else if ( line == "reload" ) {
			lr_.reload();
		} else if ( line == "lsmagic" ) {
			cout << string::join( magic_names(), " " ) << endl;
		} else if ( line == "version" ) {
			banner( repl_ );
		} else {
			isMeta = false;
		}
	} catch ( HHuginnException const& e ) {
		statusOk = false;
		cout << e.what() << endl;
	} catch ( HLexicalCastException const& e ) {
		statusOk = false;
		cout << e.what() << endl;
	} catch ( HRuntimeException const& e ) {
		statusOk = false;
		cout << e.what() << endl;
	}
	if ( isMeta && setup._jupyter ) {
		cout << ( statusOk ? "// ok" : "// error" ) << endl;
	}
	return ( isMeta );
	M_EPILOG
}

magic_names_t magic_names( void ) {
	return (
		magic_names_t( {
			"bye", "declarations", "doc", "exit", "history",
			"imports", "load", "lsmagic", "quit", "reload", "reset", "set",
			"source", "time", "variables", "version"
		} )
	);
}

void banner( HRepl* repl_ ) {
	typedef yaal::hcore::HArray<yaal::hcore::HString> tokens_t;
	tokens_t yaalVersion( string::split<tokens_t>( yaal_version( true ), character_class<CHARACTER_CLASS::WHITESPACE>().data(), HTokenizer::DELIMITED_BY_ANY_OF ) );
	if ( ! ( setup._noColor || setup._jupyter ) ) {
		print( repl_, "%s", ansi_color( GROUP::PROMPT_MARK ) );
	}
	cout << endl
		<<    "  _                 _              | A programming language with no quirks," << endl
		<<    " | |               (_)             | so simple every child can master it." << endl
		<<    " | |__  _   _  __ _ _ _ __  _ __   | Homepage: https://huginn.org/" << endl
		<< " | '_ \\| | | |/ _` | | '_ \\| '_ \\  | " << PACKAGE_STRING << endl
		<<    " | | | | |_| | (_| | | | | | | | | | " << COMMIT_ID << endl
		<<  " |_| |_|\\__,_|\\__, |_|_| |_|_| |_| | yaal " << yaalVersion[0] << endl
		<<    "               __/ |               | " << yaalVersion[1] << endl
		<<    "              (___/                | " << host_info_string() << endl;
	if ( ! ( setup._noColor || setup._jupyter ) ) {
		print( repl_, "%s", *ansi::reset );
	}
	cout << endl;
	return;
}

}

