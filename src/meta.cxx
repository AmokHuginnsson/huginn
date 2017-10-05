/*
---           `huginn' 0.0.0 (c) 1978 by Marcin 'Amok' Konarski            ---

  meta.cxx - this file is integral part of `huginn' project.

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

#include <cstring>
#include <cstdio>

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
	line.trim();
	if ( line.find( "//" ) != 0 ) {
		return ( false );
	}
	line.shift_left( 2 );
	line.trim_left();
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
		} else if ( ( line.find( "doc " ) == 0 ) || (  line.find( "doc\t" ) == 0  ) ) {
			HString symbol( line.substr( 4 ) );
			utf8.assign( symbol );
			HString doc( lr_.doc( symbol ) );
			HDescription::words_t const& methods( lr_.methods( symbol ) );
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
					apply_setting( *lr_.huginn(), line.substr( 4 ) );
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
	return ( magic_names_t( { "bye", "doc", "exit", "imports", "lsmagic", "quit", "reset", "set", "source" } ) );
}

}

