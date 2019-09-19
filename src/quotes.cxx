/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <yaal/hcore/base.hxx>
#include <yaal/hcore/hfile.hxx>
#include <yaal/tools/filesystem.hxx>
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

#ifndef __MSVCXX__
code_point_t PATH_SEP = filesystem::path::SEPARATOR;
char const PATH_ENV_SEP[] = ":";
#else
code_point_t PATH_SEP = '\\'_ycp;
char const PATH_ENV_SEP[] = ";";
#endif
char const SHELL_AND[] = "&&";
char const SHELL_OR[] = "||";
char const SHELL_PIPE[] = "|";

void strip_quotes( HString& str_ ) {
	str_.pop_back();
	str_.shift_left( 1 );
	return;
}

QUOTES str_to_quotes( yaal::hcore::HString const& token_ ) {
	QUOTES quotes( QUOTES::NONE );
	if ( ! token_.is_empty() ) {
		if ( token_.front() == '\'' ) {
			quotes = QUOTES::SINGLE;
		} else if ( token_.front() == '"' ) {
			quotes = QUOTES::DOUBLE;
		}
		if ( ( quotes != QUOTES::NONE )
			&& ( ( token_.get_size() == 1 ) || ( token_.back() != token_.front() ) ) ) {
			throw HRuntimeException( "Unmatched '"_ys.append( token_.front() ).append( "'." ) );
		}
	}
	return ( quotes );
}

REDIR str_to_redir( yaal::hcore::HString const& token_ ) {
	REDIR redir( REDIR::NONE );
	if ( token_ == "<" ) {
		redir = REDIR::IN;
	} else if ( token_ == ">" ) {
		redir = REDIR::OUT;
	} else if ( token_ == ">&" ) {
		redir = REDIR::OUT_ERR;
	} else if ( token_ == ">>" ) {
		redir = REDIR::APP;
	} else if ( token_ == ">>&" ) {
		redir = REDIR::APP_ERR;
	} else if ( token_ == SHELL_PIPE ) {
		redir = REDIR::PIPE;
	} else if ( token_ == "|&" ) {
		redir = REDIR::PIPE_ERR;
	} else if ( token_ == ";" ) {
		redir = REDIR::PIPE_END;
	}
	return ( redir );
}

namespace {

yaal::hcore::HString&& unescape_whitespace( yaal::hcore::HString&& token_ ) {
	token_.replace( "\\ ", " " ).replace( "\\\t", "\t" );
	return ( yaal::move( token_ ) );
}

bool is_shell_token( yaal::hcore::HString const& token_ ) {
	return ( ( str_to_redir( token_ ) != REDIR::NONE ) || ( token_ == SHELL_AND ) || ( token_ == SHELL_OR ) );
}

void push_break( tokens_t& tokens_ ) {
	if ( ! ( tokens_.is_empty() || tokens_.back().is_empty() ) ) {
		tokens_.push_back( "" );
	}
}

void consume_token( tokens_t& tokens_, yaal::hcore::HString& token_, bool shell_ ) {
	if ( token_.is_empty() ) {
		return;
	}
	if ( shell_ ) {
		push_break( tokens_ );
	}
	tokens_.push_back( unescape_whitespace( yaal::move( token_ ) ) );
	if ( shell_ ) {
		tokens_.push_back( "" );
	}
}

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

yaal::tools::string::tokens_t tokenize_shell( yaal::hcore::HString const& str_ ) {
	M_PROLOG
	char const SHELL_LIKE[] = "<>&|;";
	character_class_t shellLike( SHELL_LIKE, static_cast<int>( sizeof ( SHELL_LIKE ) ) - 1 );
	tokens_t tokens;
	HString token;
	HString trialToken;
	bool escaped( false );
	bool inSindleQuotes( false );
	bool inDoubleQuotes( false );
	bool wasWhitespace( false );
	bool wasShellLike( false );
	for ( code_point_t c : str_ ) {
		if ( escaped ) {
			escaped = false;
			token.push_back( c );
			continue;
		}
		int inQuotes( inSindleQuotes || inDoubleQuotes );
		if ( c == '\\' ) {
			escaped = true;
			if ( ! inQuotes ) {
				if ( wasWhitespace ) {
					push_break( tokens );
				}
				consume_token( tokens, token, wasShellLike );
			}
			token.push_back( c );
			wasWhitespace = false;
			wasShellLike = false;
			continue;
		}
		if ( ! inQuotes && ( c == '\'' ) ) {
			inSindleQuotes = true;
			if ( wasWhitespace ) {
				push_break( tokens );
			} else {
				consume_token( tokens, token, wasShellLike );
			}
			wasWhitespace = false;
			wasShellLike = false;
			token.push_back( c );
			continue;
		}
		if ( ! inQuotes && ( c == '"' ) ) {
			inDoubleQuotes = true;
			if ( wasWhitespace ) {
				push_break( tokens );
			} else {
				consume_token( tokens, token, wasShellLike );
			}
			wasWhitespace = false;
			wasShellLike = false;
			token.push_back( c );
			continue;
		}
		if ( inSindleQuotes && ( c == '\'' ) ) {
			token.push_back( c );
			tokens.push_back( unescape_whitespace( yaal::move( token ) ) );
			inSindleQuotes = false;
			continue;
		}
		if ( inDoubleQuotes && ( c == '"' ) ) {
			token.push_back( c );
			tokens.push_back( unescape_whitespace( yaal::move( token ) ) );
			inDoubleQuotes = false;
			continue;
		}
		if ( inQuotes ) {
			token.push_back( c );
			continue;
		}
		bool whitespace( character_class<CHARACTER_CLASS::WHITESPACE>().has( c ) );
		bool isShellLike( shellLike.has( c ) );
		if ( whitespace && ! wasWhitespace ) {
			consume_token( tokens, token, wasShellLike );
			wasWhitespace = true;
			continue;
		}
		if ( whitespace ) {
			continue;
		}
		if ( wasWhitespace ) {
			push_break( tokens );
		}
		if ( isShellLike ) {
			if ( wasShellLike ) {
				trialToken.assign( token ).append( c );
				if ( ! is_shell_token( trialToken ) ) {
					consume_token( tokens, token, wasShellLike );
				}
			} else {
				consume_token( tokens, token, wasShellLike );
			}
		} else if ( wasShellLike ) {
			consume_token( tokens, token, wasShellLike );
		}
		token.push_back( c );
		wasWhitespace = false;
		wasShellLike = isShellLike;
	}
	consume_token( tokens, token, wasShellLike );
	if ( ! tokens.is_empty() && tokens.back().is_empty() ) {
		tokens.pop_back();
	}
	return ( tokens );
	M_EPILOG
}

}

