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
#	define REPL_print printf
#else
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
		if ( cp == 'm' ) {
			skip = false;
		} else if ( cp == '\033' ) {
			skip = true;
		}
		if ( skip ) {
			continue;
		}
		++ len;
	}
	return ( len );
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
	int lineCount( 0 );
	bool goToEnd( false );
	bool oneLine( false );
	HUTF8String utf8;
	for ( HString const& line : lines ) {
		int lineLength( line_length( line ) );
		int lineHeight( lineLength / termSize.columns() + ( lineLength % termSize.columns() ? 1 : 0 ) );
		if ( lineHeight == 0 ) {
			++ lineHeight;
		}
		lineCount += lineHeight;
		if ( oneLine || ( ! goToEnd && ( lineCount >= ( termSize.lines() - 1 ) ) ) ) {
			oneLine = false;
			REPL_print( PROMPT );
			code_point_t key( term.get_key() );
			REPL_print( "%s%s%s", ERASE, SPACE, ERASE );
			if ( key == 'q' ) {
				break;
			} else if ( ( key == 'G' ) || ( key == KEY_CODE::END ) ) {
				goToEnd = true;
			} else if ( ( key == '\r' ) || ( key == KEY_CODE::DOWN ) ) {
				oneLine = true;
			}
			lineCount = lineHeight;
		}
		utf8.assign( line );
		REPL_print( "%s\n", utf8.c_str() );
	}
	return;
	M_EPILOG
}

}

