/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <yaal/hcore/base.hxx>
#include <yaal/hcore/hfile.hxx>
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
	bool inSingleQuotes( false );
	bool inDoubleQuotes( false );
	bool escaped( false );
	for ( code_point_t c : str_ ) {
		if ( escaped ) {
			escaped = false;
			continue;
		}
		if ( c == '\\' ) {
			escaped = true;
		} else if ( ( c == '"' ) && ! inSingleQuotes ) {
			inDoubleQuotes = ! inDoubleQuotes;
		} else if ( ( c == '\'' ) && ! inDoubleQuotes ) {
			inSingleQuotes = ! inSingleQuotes;
		}
	}
	return ( inSingleQuotes || inDoubleQuotes );
	M_EPILOG
}

yaal::tools::string::tokens_t split_quotes( yaal::hcore::HString const& str_ ) {
	M_PROLOG
	HString SPLIT_ON( character_class( CHARACTER_CLASS::WHITESPACE ).data() );
	HString const KEEP( "<>|" );
	SPLIT_ON.append( KEEP );
	tokens_t tokens;
	bool inSingleQuotes( false );
	bool inDoubleQuotes( false );
	bool escaped( false );
	HString token;
	bool redir( false );
	for ( code_point_t c : str_ ) {
		if ( escaped ) {
			token.push_back( c );
			escaped = false;
			continue;
		}
		bool inQuotes( inSingleQuotes || inDoubleQuotes );
		if ( ! inQuotes && ( SPLIT_ON.find( c ) != HString::npos ) ) {
			bool keep( KEEP.find( c ) != HString::npos );
			if ( ! token.is_empty() ) {
				tokens.push_back( token.replace( "\\ ", " " ).replace( "\\\t", "\t" ) );
				token.clear();
			}
			if ( ! ( tokens.is_empty() || tokens.back().is_empty() ) ) {
				tokens.push_back( "" );
			} else if ( ( tokens.is_empty() || tokens.back().is_empty() ) && ! ( keep || redir ) ) {
				continue;
			}
			if ( keep ) {
				if ( ( c == '>' ) && ! redir ) {
					redir = true;
					continue;
				} else if ( c == '>' ) {
					tokens.push_back( ">>" );
				} else {
					if ( redir ) {
						tokens.push_back( ">" );
						tokens.push_back( "" );
					}
					tokens.push_back( c );
				}
				tokens.push_back( "" );
			} else if ( redir ) {
				tokens.push_back( ">" );
				tokens.push_back( "" );
			}
			redir = false;
			continue;
		}
		if ( redir ) {
			tokens.push_back( ">" );
			tokens.push_back( "" );
			redir = false;
		}
		if ( c == '\\' ) {
			escaped = true;
		} else if ( ( c == '"' ) && ! inSingleQuotes ) {
			inDoubleQuotes = ! inDoubleQuotes;
		} else if ( ( c == '\'' ) && ! inDoubleQuotes ) {
			inSingleQuotes = ! inSingleQuotes;
		}
		if ( ! inQuotes && ( inSingleQuotes || inDoubleQuotes ) && ! token.is_empty() ) {
			tokens.push_back( token.replace( "\\ ", " " ).replace( "\\\t", "\t" ) );
			token.clear();
		}
		token.push_back( c );
		if ( inQuotes && ! ( inSingleQuotes || inDoubleQuotes ) ) {
			tokens.push_back( token.replace( "\\ ", " " ).replace( "\\\t", "\t" ) );
			token.clear();
		}
	}
	if ( ! token.is_empty() ) {
		tokens.push_back( token.replace( "\\ ", " " ).replace( "\\\t", "\t" ) );
	} else if ( redir ) {
		tokens.push_back( ">" );
	}
	if ( ! tokens.is_empty() && tokens.back().is_empty() ) {
		tokens.pop_back();
	}
	return ( tokens );
	M_EPILOG
}

}

