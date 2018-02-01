/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <yaal/hcore/base.hxx>
M_VCSID( "$Id: " __ID__ " $" )
M_VCSID( "$Id: " __TID__ " $" )
#include "quotes.hxx"

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;
using namespace yaal::tools::string;

namespace huginn {

bool in_quotes( yaal::hcore::HString const& str_ ) {
	M_PROLOG
	int singleQuoteCount( 0 );
	int doubleQuoteCount( 0 );
	bool escaped( false );
	for ( code_point_t c : str_ ) {
		if ( escaped ) {
			escaped = false;
			continue;
		}
		if ( c == '\\' ) {
			escaped = true;
		} else if ( ( c == '"' ) && ( ( singleQuoteCount % 2 ) == 0 ) ) {
			++ doubleQuoteCount;
		} else if ( ( c == '\'' ) && ( ( doubleQuoteCount % 2 ) == 0 ) ) {
			++ singleQuoteCount;
		}
	}
	return ( ( doubleQuoteCount % 2 ) || ( singleQuoteCount % 2 ) );
	M_EPILOG
}

yaal::tools::string::tokens_t split_quotes( yaal::hcore::HString const& str_ ) {
	M_PROLOG
	tokens_t tokens;
	int singleQuoteCount( 0 );
	int doubleQuoteCount( 0 );
	bool escaped( false );
	HString token;
	for ( code_point_t c : str_ ) {
		if ( escaped ) {
			token.push_back( c );
			escaped = false;
			continue;
		}
		if ( character_class( CHARACTER_CLASS::WHITESPACE ).has( c ) ) {
			bool inQuotes( ( doubleQuoteCount % 2 ) || ( singleQuoteCount % 2 ) );
			if ( ! inQuotes ) {
				if ( ! token.is_empty() ) {
					tokens.push_back( token.replace( "\\ ", " " ).replace( "\\\t", "\t" ) );
					token.clear();
				}
				continue;
			}
		}
		token.push_back( c );
		if ( c == '\\' ) {
			escaped = true;
		} else if ( ( c == '"' ) && ( ( singleQuoteCount % 2 ) == 0 ) ) {
			++ doubleQuoteCount;
		} else if ( ( c == '\'' ) && ( ( doubleQuoteCount % 2 ) == 0 ) ) {
			++ singleQuoteCount;
		}
	}
	if ( ! token.is_empty() ) {
		tokens.push_back( token.replace( "\\ ", " " ).replace( "\\\t", "\t" ) );
	}
	return ( tokens );
	M_EPILOG
}

}

