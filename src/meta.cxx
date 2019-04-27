/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <cstring>
#include <cstdio>

#include <yaal/hcore/hcore.hxx>
#include <yaal/hcore/hfile.hxx>
#include <yaal/tools/ansi.hxx>
#include <yaal/tools/util.hxx>
#include <yaal/tools/stringalgo.hxx>
#include <yaal/tools/hterminal.hxx>
#include <yaal/tools/huginn/value.hxx>
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
	hcore::HString line( line_ );
	line.trim_left();
	if ( line.find( "//" ) != 0 ) {
		return ( false );
	}
	line.shift_left( 2 );
	line.trim_left();
	hcore::HString setting( line );
	line.trim_right();
#ifdef USE_REPLXX
	replxx::Replxx repl;
#endif
	try {
		HUTF8String utf8;
		if ( ( line == "quit" ) || ( line == "exit" ) || ( line == "bye" ) ) {
			setup._interactive = false;
		} else if ( line == "declarations" ) {
			HStringStream ss;
			HHuginn preproc;
			for ( yaal::hcore::HString const& d : lr_.definitions() ) {
				ss.str( d );
				preproc.reset();
				preproc.load( ss );
				preproc.preprocess();
				preproc.dump_preprocessed_source( ss );
				HLineRunner::lines_t lines( string::split( ss.str(), "\n" ) );
				for ( hcore::HString& l : lines ) {
					l.trim_left();
					l.trim_right( " {" );
					if ( ! l.is_empty() ) {
						cout << l << endl;
						break;
					}
				}
			}
		} else if ( line == "variables" ) {
			int maxLen( 0 );
			for ( HIntrospecteeInterface::HVariableView const& vv : lr_.locals() ) {
				if ( vv.name().get_length() > maxLen ) {
					maxLen = static_cast<int>( vv.name().get_length() );
				}
			}
			for ( HIntrospecteeInterface::HVariableView const& vv : lr_.locals() ) {
				cout << vv.name() << setw( maxLen - static_cast<int>( vv.name().get_length() ) + 3 ) << " - " << vv.value()->get_class()->name() << endl;
			}
		} else if ( line == "source" ) {
			if ( setup._jupyter ) {
				cout << lr_.source();
			} else if ( setup._interactive && ! setup._noColor ) {
				pager( colorize( lr_.source() ) );
			} else {
				pager( lr_.source() );
			}
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
				"**//imports**          - show a list of imports currently in effect\n"
				"**//source**           - show current session source\n"
				"**//declarations**     - list locally declared classes and functions\n"
				"**//variables**        - list currently defined local variables\n"
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
			hcore::HString symbol( line.substr( 4 ) );
			symbol.trim_right( "()" );
			utf8.assign( symbol );
			hcore::HString doc( lr_.doc( symbol, true ) );
			HDescription::words_t const& members( lr_.members( symbol, true ) );
			if ( ! doc.is_empty() ) {
				if ( ! members.is_empty() && ( doc.find( "`"_ys.append( symbol ).append( "`" ) ) == HString::npos ) ) {
					REPL_print( "%s%s%s - ", start( "`" ), utf8.c_str(), end( "`" ) );
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
				REPL_print( "%s\n", HUTF8String( highlight( doc ) ).c_str() );
				if ( ! members.is_empty() ) {
					REPL_print( "Class %s%s%s has following members:\n", start( "`" ), utf8.c_str(), end( "`" ) );
				}
			} else if ( ! members.is_empty() ) {
				REPL_print( "Class %s%s%s is not documented but has following members:\n", start( "`" ), utf8.c_str(), end( "`" ) );
			} else {
				REPL_print( "symbol %s%s%s is unknown or undocumented\n", start( "`" ), utf8.c_str(), end( "`" ) );
			}
			if ( ! members.is_empty() ) {
				for ( yaal::hcore::HString const& m : members ) {
					utf8.assign( m );
					REPL_print( "+ %s\n", utf8.c_str() );
				}
			}
		} else if ( line.find( "set" ) == 0 ) {
			if ( line.get_length() > 3 ) {
				if ( character_class<CHARACTER_CLASS::WHITESPACE>().has( line[3] ) ) {
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
	return ( magic_names_t( { "bye", "declarations", "doc", "exit", "imports", "lsmagic", "quit", "reset", "set", "source", "variables", "version" } ) );
}

void banner( void ) {
	typedef yaal::hcore::HArray<yaal::hcore::HString> tokens_t;
	tokens_t yaalVersion( string::split<tokens_t>( yaal_version( true ), character_class<CHARACTER_CLASS::WHITESPACE>().data(), HTokenizer::DELIMITED_BY_ANY_OF ) );
#ifdef USE_REPLXX
	replxx::Replxx repl;
#endif
	if ( ! ( setup._noColor || setup._jupyter ) ) {
		REPL_print( "%s", ansi_color( GROUP::PROMPT_MARK ) );
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
		REPL_print( "%s", *ansi::reset );
	}
	cout << endl;
	return;
}

}

