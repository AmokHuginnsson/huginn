/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <cstring>

#include <yaal/tools/ansi.hxx>
#include <yaal/tools/stringalgo.hxx>
#include <yaal/tools/hterminal.hxx>
#include <yaal/tools/keycode.hxx>
M_VCSID( "$Id: " __ID__ " $" )
M_VCSID( "$Id: " __TID__ " $" )
#include "pager.hxx"

#include "config.hxx"

#ifdef USE_REPLXX
#	include <replxx.hxx>
#	define REPL_print repl.print
#elif defined( USE_EDITLINE )
#	include <cstdio>
#	define REPL_print printf
#else
#	include <cstdio>
#	define REPL_print printf
#endif

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;
using namespace yaal::ansi;

namespace huginn {

namespace {

int line_length( yaal::hcore::HString const& str_ ) {
	int len( 0 );
	bool skip( false );
	for ( code_point_t cp : str_ ) {
		if ( skip && ( cp == 'm' ) ) {
			skip = false;
			continue;
		} else if ( cp == '\033' ) {
			skip = true;
		}
		if ( skip ) {
			continue;
		}
		++ len;
	}
	return len;
}

}

void pager( yaal::hcore::HString const& str_ ) {
	M_PROLOG
	static char const PROMPT[] = "Press 'Q' to quit or any other key to continue...";
	static char ERASE[sizeof ( PROMPT )] = { 0 };
	static char SPACE[sizeof ( PROMPT )] = { 0 };
	static bool once( true );
	if ( once ) {
		once = false;
		::memset( ERASE, '\b', sizeof ( ERASE ) - 1 );
		::memset( SPACE, ' ', sizeof ( SPACE ) - 1 );
	}
#ifdef USE_REPLXX
	replxx::Replxx repl;
#endif
	string::tokens_t lines( string::split( str_, '\n' ) );
	HTerminal& term( HTerminal::get_instance() );
	HTerminal::HSize termSize( term.size() );
	HUTF8String utf8;
	bool loop( true );
	int row( 0 );
	bool justStarted( true );
	auto line_height = [&termSize]( yaal::hcore::HString const& line_ ) {
		int lineLength( line_length( line_ ) );
		int lineHeight( lineLength / termSize.columns() + ( lineLength % termSize.columns() ? 1 : 0 ) );
		if ( lineHeight == 0 ) {
			++ lineHeight;
		}
		return ( lineHeight );
	};

	while ( loop ) {
		int lineCount( 0 );
		int rowInPage( 0 );
		while ( ( ( row + rowInPage ) < lines.get_size() ) && ( lineCount < ( termSize.lines() - ( justStarted ? 2 : 1 ) ) ) ) {
			HString const& line( lines[row + rowInPage] );
			lineCount += line_height( line );
			utf8.assign( line );
			REPL_print( "%s\n", utf8.c_str() );
			++ rowInPage;
		}
		if ( ( row + rowInPage ) == lines.get_size() ) {
			break;
		}
		justStarted = false;
		-- rowInPage;

		REPL_print( PROMPT );
		code_point_t key( term.get_key() );
		REPL_print( "%s%s%s", ERASE, SPACE, ERASE );
		switch ( key.get() ) {
			case ( KEY_CODE::PAGE_DOWN ): /* fall-through */
			case ( ' ' ): {
				row += rowInPage;
			} break;
			case ( KEY_CODE::END ): /* fall-through */
			case ( 'G' ): {
				if ( lines.is_empty() ) {
					break;
				}
				row = static_cast<int>( lines.get_size() ) - 1;
			} /* fall-through */
			case ( KEY_CODE::PAGE_UP ): /* fall-through */
			case ( 'b' ): {
				lineCount = 0;
				while ( true ) {
					HString const& line( lines[row] );
					lineCount += line_height( line );
					if ( lineCount >= ( termSize.lines() - 1 ) ) {
						break;
					}
					if ( row == 0 ) {
						break;
					}
					-- row;
				}
			} break;
			case ( KEY_CODE::HOME ): /* fall-through */
			case ( 'g' ): {
				row = 0;
			} break;
			case ( KEY_CODE::DOWN ): /* fall-through */
			case ( '\r' ): {
				++ row;
			} break;
			case ( KEY_CODE::UP ): /* fall-through */
			case ( '\b' ): {
				if ( row > 0 ) {
					-- row;
				}
			} break;
			case ( 'q' ): {
				loop = 0;
			} break;
		}
		if ( row >= lines.get_size() ) {
			loop = false;
		}
	}
	return;
	M_EPILOG
}

}

