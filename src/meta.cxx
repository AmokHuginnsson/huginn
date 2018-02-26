/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <cstring>
#include <cstdio>

#include <yaal/hcore/hcore.hxx>
#include <yaal/hcore/hfile.hxx>
#include <yaal/tools/ansi.hxx>
#include <yaal/tools/util.hxx>
#include <yaal/tools/stringalgo.hxx>
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

#include "meta.hxx"
#include "settings.hxx"
#include "colorize.hxx"
#include "interactive.hxx"
#include "commit_id.hxx"

#include "setup.hxx"


using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;
using namespace yaal::tools::util;
using namespace yaal::ansi;

namespace huginn {

namespace {
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
}

bool meta( HLineRunner& lr_, yaal::hcore::HString const& line_ ) {
	M_PROLOG
	bool isMeta( true );
	bool statusOk( true );
	HString line( line_ );
	line.trim_left();
	if ( line.find( "//" ) != 0 ) {
		return ( false );
	}
	line.shift_left( 2 );
	line.trim_left();
	HString setting( line );
	line.trim_right();
	try {
		HUTF8String utf8;
		if ( ( line == "quit" ) || ( line == "exit" ) || ( line == "bye" ) ) {
			setup._interactive = false;
		} else if ( line == "source" ) {
			if ( setup._interactive && ! setup._noColor ) {
				utf8.assign( colorize( lr_.source() ) );
			} else {
				utf8.assign( lr_.source() );
			}
			REPL_print( "%s", utf8.c_str() );
		} else if (  line == "imports" ) {
			for ( HLineRunner::lines_t::value_type const& l : lr_.imports() ) {
				utf8.assign( l );
				REPL_print( "%s\n", utf8.c_str() );
			}
		} else if ( line == "doc" ) {
			char const doc[] =
				"**//doc**              - show this help message\n"
				"**//doc** *symbol*       - show documentation for given *symbol*\n"
				"**//doc** *class*.*method* - show documentation for given *method* in *class*\n"
				"**//(quit**|**exit**|**bye)**  - end interactive session and exit program\n"
				"**//imports**          - list currently effective import list\n"
				"**//source**           - show current session source\n"
				"**//set**              - show runner/engine options currently in effect\n"
				"**//set** *option*=*value* - set given *option* to new *value*\n"
				"**//reset**            - wipe out current session state\n"
				"**//lsmagic**          - list available magic commands\n"
				"**//version**          - print engine (yaal library) and runner version\n"
			;
			if ( setup._interactive && ! setup._noColor ) {
				REPL_print( "%s", HUTF8String( highlight( doc ) ).raw() );
			} else {
				cout << to_string( doc ).replace( "*", "" ).replace( "//", setup._jupyter ? "%" : "//" ) << flush;
			}
		} else if ( ( line.find( "doc " ) == 0 ) || (  line.find( "doc\t" ) == 0  ) ) {
			HString symbol( line.substr( 4 ) );
			symbol.trim_right( "(" );
			utf8.assign( symbol );
			HString doc( lr_.doc( symbol, true ) );
			HDescription::words_t const& methods( lr_.methods( symbol, true ) );
			if ( ! doc.is_empty() ) {
				if ( ! methods.is_empty() && ( doc.find( "`"_ys.append( symbol ).append( "`" ) ) == HString::npos ) ) {
					REPL_print( "%s%s%s - ", start( "`" ), utf8.c_str(), end( "`" ) );
				}
				int long ms( symbol.find( '.'_ycp ) );
				if ( ms != HString::npos ) {
					++ ms;
					symbol.erase( 0, ms );
					int long ss( doc.find( symbol ) );
					if ( ss != HString::npos ) {
						int long sl( symbol.get_length() );
						symbol.replace( "_", "\\_" );
						doc.replace( ss, sl, symbol );
					}
				}
				REPL_print( "%s\n", HUTF8String( highlight( doc ) ).c_str() );
				if ( ! methods.is_empty() ) {
					REPL_print( "Class %s%s%s has following members:\n", start( "`" ), utf8.c_str(), end( "`" ) );
				}
			} else if ( ! methods.is_empty() ) {
				REPL_print( "Class %s%s%s is not documented but has following members:\n", start( "`" ), utf8.c_str(), end( "`" ) );
			} else {
				REPL_print( "symbol %s%s%s is unknown or undocumented\n", start( "`" ), utf8.c_str(), end( "`" ) );
			}
			if ( ! methods.is_empty() ) {
				for ( yaal::hcore::HString const& m : methods ) {
					utf8.assign( m );
					REPL_print( "+ %s\n", utf8.c_str() );
				}
			}
		} else if ( line.find( "set" ) == 0 ) {
			if ( line.get_length() > 3 ) {
				if ( character_class( CHARACTER_CLASS::WHITESPACE ).has( line[3] ) ) {
					apply_setting( *lr_.huginn(), setting.substr( 4 ) );
				} else {
					isMeta = false;
				}
			} else {
				for ( rt_settings_t::value_type const& s : rt_settings() ) {
					cout << s.first << "=" << s.second << endl;
				}
			}
		} else if ( line == "reset" ) {
			lr_.reset();
		} else if ( line == "lsmagic" ) {
			cout << string::join( magic_names(), " " ) << endl;
		} else if ( line == "version" ) {
			banner();
		} else {
			isMeta = false;
		}
	} catch ( HHuginnException const& e ) {
		statusOk = false;
		REPL_print( "%s\n", e.what() );
	} catch ( HLexicalCastException const& e ) {
		statusOk = false;
		REPL_print( "%s\n", e.what() );
	} catch ( HRuntimeException const& e ) {
		statusOk = false;
		REPL_print( "%s\n", e.what() );
	}
	if ( isMeta && setup._jupyter ) {
		cout << ( statusOk ? "// ok" : "// error" ) << endl;
	}
	return ( isMeta );
	M_EPILOG
}

magic_names_t magic_names( void ) {
	return ( magic_names_t( { "bye", "doc", "exit", "imports", "lsmagic", "quit", "reset", "set", "source", "version" } ) );
}

void banner( void ) {
	typedef yaal::hcore::HArray<yaal::hcore::HString> tokens_t;
	tokens_t yaalVersion( string::split<tokens_t>( yaal_version( true ), character_class( CHARACTER_CLASS::WHITESPACE ).data(), HTokenizer::DELIMITED_BY_ANY_OF ) );
	if ( ! ( setup._noColor || setup._jupyter ) ) {
		REPL_print( "%s", ansi_color( GROUP::PROMPT_MARK ) );
	}
	cout << endl
		<<    "  _                 _              | A programming language with no quirks," << endl
		<<    " | |               (_)             | so simple every child can master it." << endl
		<<    " | |__  _   _  __ _ _ _ __  _ __   |" << endl
		<< " | '_ \\| | | |/ _` | | '_ \\| '_ \\  | Homepage: https://huginn.org/" << endl
		<<    " | | | | |_| | (_| | | | | | | | | | " << PACKAGE_STRING << endl
		<<  " |_| |_|\\__,_|\\__, |_|_| |_|_| |_| | " << COMMIT_ID << endl
		<<    "               __/ |               | yaal " << yaalVersion[0] << endl
		<<    "              (___/                | " << yaalVersion[1] << endl;
	if ( ! ( setup._noColor || setup._jupyter ) ) {
		REPL_print( "%s", *ansi::reset );
	}
	cout << endl;
	return;
}

}

