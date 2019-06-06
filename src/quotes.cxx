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

yaal::hcore::HString unescape_huginn_code( yaal::hcore::HString const& code_ ) {
	M_PROLOG
	bool inSingleQuotes( false );
	bool inDoubleQuotes( false );
	bool escaped( false );
	HString unescaped;
	for ( code_point_t c : code_ ) {
		if ( escaped ) {
			escaped = false;
			unescaped.push_back( c );
			continue;
		}
		if ( c == '\\' ) {
			escaped = true;
			if ( ! ( inSingleQuotes || inDoubleQuotes ) ) {
				continue;
			}
		} else if ( ( c == '"' ) && ! inSingleQuotes ) {
			inDoubleQuotes = ! inDoubleQuotes;
		} else if ( ( c == '\'' ) && ! inDoubleQuotes ) {
			inSingleQuotes = ! inSingleQuotes;
		}
		unescaped.push_back( c );
	}
	return ( unescaped );
	M_EPILOG
}

yaal::tools::string::tokens_t split_quotes( yaal::hcore::HString const& str_ ) {
	M_PROLOG
	HString SPLIT_ON( character_class<CHARACTER_CLASS::WHITESPACE>().data() );
	HString const KEEP( "<>|&;" );
	HString const DOUBLE_SPLITTERS( ">|&" );
	SPLIT_ON.append( KEEP );
	code_point_t splitter( ' ' );
	tokens_t tokens;
	bool inSingleQuotes( false );
	bool inDoubleQuotes( false );
	bool escaped( false );
	HString token;
	bool splitted( false );
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
			} else if ( ( tokens.is_empty() || tokens.back().is_empty() ) && ! ( keep || splitted ) ) {
				continue;
			}
			if ( keep ) {
				if ( ( DOUBLE_SPLITTERS.find( c ) != HString::npos ) && ! splitted ) {
					splitted = true;
					splitter = c;
					continue;
				} else if ( c == splitter ) {
					tokens.push_back( to_string( splitter ).append( splitter ) );
				} else {
					if ( splitted ) {
						tokens.push_back( splitter );
						tokens.push_back( "" );
					}
					tokens.push_back( c );
				}
				tokens.push_back( "" );
			} else if ( splitted ) {
				tokens.push_back( splitter );
				tokens.push_back( "" );
			}
			splitted = false;
			continue;
		}
		if ( splitted ) {
			tokens.push_back( splitter );
			tokens.push_back( "" );
			splitted = false;
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
	} else if ( splitted ) {
		tokens.push_back( splitter );
	}
	if ( ! tokens.is_empty() && tokens.back().is_empty() ) {
		tokens.pop_back();
	}
	return ( tokens );
	M_EPILOG
}

}

