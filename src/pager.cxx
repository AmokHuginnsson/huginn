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

int line_length( yaal::hcore::HString& str_, int leftShift_ ) {
	int len( 0 );
	bool skip( false );
	int extraShift( 0 );
	for ( code_point_t cp : str_ ) {
		if ( skip && ( cp == 'm' ) ) {
			if ( len < leftShift_ ) {
				++ extraShift;
			}
			skip = false;
			continue;
		} else if ( cp == '\033' ) {
			skip = true;
		}
		if ( skip ) {
			if ( len < leftShift_ ) {
				++ extraShift;
			}
			continue;
		}
		++ len;
	}
	str_.shift_left( leftShift_ + extraShift );
	return len;
}

int line_height( int termColumns_, yaal::hcore::HString& line_, int leftShift_ ) {
	int lineLength( line_length( line_, leftShift_ ) );
	int lineHeight( lineLength / termColumns_ + ( lineLength % termColumns_ ? 1 : 0 ) );
	if ( lineHeight == 0 ) {
		++ lineHeight;
	}
	return ( lineHeight );
}

int rows_in_page(
	string::tokens_t const& lines_, int startRow_, int leftShift_, int termColumns_, int maxTermRows_, bool print_, void*
#ifdef USE_REPLXX
	repl_
#endif
) {
#ifdef USE_REPLXX
	replxx::Replxx& repl( *static_cast<replxx::Replxx*>( repl_ ) );
#endif
	int lineCount( 0 );
	int rowsInPage( 0 );
	HUTF8String utf8;
	HString line;
	while ( ( ( startRow_ + rowsInPage ) < lines_.get_size() ) && ( lineCount < maxTermRows_ ) ) {
		line.assign( lines_[startRow_ + rowsInPage] );
		lineCount += line_height( termColumns_, line, leftShift_ );
		if ( print_ ) {
			utf8.assign( line );
			REPL_print( "%s\n", utf8.c_str() );
		}
		++ rowsInPage;
	}
	return rowsInPage;
}

}

void pager( yaal::hcore::HString const& str_, PAGER_MODE pagerMode_ ) {
	M_PROLOG
	static char const PROMPT[] = "Press 'Q' to quit or any other key to continue... (%4d) %3d%%";
	static const int extraCharacterCount( 1 );
	static char ERASE[sizeof ( PROMPT ) + extraCharacterCount] = { 0 };
	static char SPACE[sizeof ( PROMPT ) + extraCharacterCount] = { 0 };
	static bool once( true );
	if ( once ) {
		once = false;
		::memset( ERASE, '\b', sizeof ( ERASE ) - 1 );
		::memset( SPACE, ' ', sizeof ( SPACE ) - 1 );
	}
#ifdef USE_REPLXX
	replxx::Replxx repl;
#else
	int repl( 0 );
#endif
	string::tokens_t lines( string::split( str_, '\n' ) );
	HTerminal& term( HTerminal::get_instance() );
	HTerminal::HSize termSize( term.size() );
	HString line;
	HUTF8String utf8;
	bool loop( true );
	int row( 0 );
	int leftShift( 0 );
	bool justStarted( true );

	while ( loop ) {
		int rowsInPage( rows_in_page( lines, row, leftShift, termSize.columns(), ( termSize.lines() - ( justStarted ? 2 : 1 ) ), true, &repl ) );
		if ( ( ( row + rowsInPage ) == lines.get_size() ) && ( pagerMode_ == PAGER_MODE::EXIT ) ) {
			break;
		}
		justStarted = false;

		REPL_print( PROMPT, row, min( ( row + rowsInPage ), static_cast<int>( lines.get_size() ) ) * 100 / static_cast<int>( lines.get_size() ) );
		code_point_t key( term.get_key() );
		REPL_print( "%s%s%s", ERASE, SPACE, ERASE );
		switch ( key.get() ) {
			case ( KEY_CODE::PAGE_DOWN ): /* fall-through */
			case ( ' ' ): {
				int rowsInNextPage( rows_in_page( lines, row + rowsInPage, leftShift, termSize.columns(), ( termSize.lines() - 1 ), false, &repl ) );
				if ( rowsInNextPage >= ( termSize.lines() - 2 ) ) {
					-- rowsInPage;
				}
				row += rowsInPage;
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
				int lineCount( 0 );
				while ( true ) {
					line.assign( lines[row] );
					lineCount += line_height( termSize.columns(), line, leftShift );
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
			case ( KEY_CODE::RIGHT ): {
				++ leftShift;
			} break;
			case ( KEY_CODE::LEFT ): {
				if ( leftShift > 0 ) {
					-- leftShift;
				}
			} break;
			case ( 'q' ): {
				loop = false;
			} break;
		}
		if ( ( row + rowsInPage ) > lines.get_size() ) {
			if ( pagerMode_ == PAGER_MODE::EXIT ) {
				loop = false;
			} else {
				row = static_cast<int>( lines.get_size() ) - rowsInPage;
			}
		}
	}
	return;
	M_EPILOG
}

}

