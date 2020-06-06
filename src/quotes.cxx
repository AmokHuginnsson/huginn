/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <yaal/hcore/base.hxx>
#include <yaal/hcore/hcore.hxx>
#include <yaal/hcore/hfile.hxx>
#include <yaal/hcore/hqueue.hxx>
#include <yaal/hcore/hstack.hxx>
#include <yaal/hcore/bound.hxx>
#include <yaal/hcore/system.hxx>
#include <yaal/tools/filesystem.hxx>
#include <yaal/tools/executingparser.hxx>
M_VCSID( "$Id: " __ID__ " $" )
M_VCSID( "$Id: " __TID__ " $" )
#include "quotes.hxx"

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;
using namespace yaal::tools::util;
using namespace yaal::tools::string;

namespace huginn {

HQuoteObserver::HQuoteObserver( void )
	: _escaped( false )
	, _inSingleQuotes( false )
	, _inDoubleQuotes( false ) {
}

bool HQuoteObserver::notice( code_point_t cp_ ) {
	if ( _escaped ) {
		_escaped = false;
		return ( true );
	}
	if ( cp_ == '\\' ) {
		_escaped = true;
		return ( true );
	}
	bool inStrQuotes( _inSingleQuotes || _inDoubleQuotes );
	if ( ! inStrQuotes && ( cp_ == '\'' ) ) {
		_inSingleQuotes = true;
		return ( true );
	}
	if ( ! inStrQuotes && ( cp_ == '"' ) ) {
		_inDoubleQuotes = true;
		return ( true );
	}
	if ( _inSingleQuotes && ( cp_ == '\'' ) ) {
		_inSingleQuotes = false;
		return ( true );
	}
	if ( _inDoubleQuotes && ( cp_ == '"' ) ) {
		_inDoubleQuotes = false;
		return ( true );
	}
	return ( _inSingleQuotes || _inDoubleQuotes );
}

void HQuoteObserver::reset( void ) {
	_escaped = false;
	_inSingleQuotes = false;
	_inDoubleQuotes = false;
	return;
}

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
char const REDIR_IN[] = "<";
char const REDIR_OUT[] = ">";
char const REDIR_ERR[] = "!>";
char const REDIR_OUT_ERR[] = ">&";
char const REDIR_APP_OUT[] = ">>";
char const REDIR_APP_ERR[] = "!>>";
char const REDIR_APP_OUT_ERR[] = ">>&";
char const REDIR_ERR_OUT[] = "!&";
char const SHELL_AND[] = "&&";
char const SHELL_OR[] = "||";
char const SHELL_PIPE[] = "|";
char const SHELL_PIPE_ERR[] = "|&";
char const SHELL_TERM[] = ";";
char const SHELL_BG[] = "&";

void strip_quotes( HString& str_ ) {
	str_.pop_back();
	code_point_t cp( str_.front() );
	str_.shift_left( ( cp == '$' ) || ( cp == '<' ) || ( cp == '>' ) ? 2 : 1 );
	return;
}

QUOTES str_to_quotes( yaal::hcore::HString const& token_ ) {
	QUOTES quotes( QUOTES::NONE );
	if ( ! token_.is_empty() ) {
		if ( token_.front() == '\'' ) {
			quotes = QUOTES::SINGLE;
		} else if ( token_.front() == '"' ) {
			quotes = QUOTES::DOUBLE;
		} else if ( token_.starts_with( "$(" ) ) {
			quotes = QUOTES::EXEC;
		} else if ( token_.starts_with( "<(" ) ) {
			quotes = QUOTES::EXEC_SOURCE;
		} else if ( token_.starts_with( ">(" ) ) {
			quotes = QUOTES::EXEC_SINK;
		}
		if (
			( ( quotes == QUOTES::SINGLE ) || ( quotes == QUOTES::DOUBLE ) )
			&& ( ( token_.get_size() == 1 ) || ( token_.back() != token_.front() ) )
		) {
			throw HRuntimeException( "Unmatched '"_ys.append( token_.front() ).append( "'." ) );
		}
		if (
			( ( quotes == QUOTES::EXEC ) || ( quotes == QUOTES::EXEC_SOURCE ) || ( quotes == QUOTES::EXEC_SINK ) )
			&& ! token_.ends_with( ")" )
		) {
			throw HRuntimeException(
				"Unmatched '"_ys.append( quotes == QUOTES::EXEC ? '$' : ( quotes == QUOTES::EXEC_SOURCE ? '<' : '>' ) ).append( "('." )
			);
		}
	}
	return ( quotes );
}

REDIR str_to_redir( yaal::hcore::HString const& token_ ) {
	REDIR redir( REDIR::NONE );
	if ( token_ == REDIR_IN ) {
		redir = REDIR::IN;
	} else if ( token_ == REDIR_OUT ) {
		redir = REDIR::OUT;
	} else if ( token_ == REDIR_ERR ) {
		redir = REDIR::ERR;
	} else if ( token_ == REDIR_OUT_ERR ) {
		redir = REDIR::OUT_ERR;
	} else if ( token_ == REDIR_APP_OUT ) {
		redir = REDIR::APP_OUT;
	} else if ( token_ == REDIR_APP_ERR ) {
		redir = REDIR::APP_ERR;
	} else if ( token_ == REDIR_APP_OUT_ERR ) {
		redir = REDIR::APP_OUT_ERR;
	} else if ( token_ == REDIR_ERR_OUT ) {
		redir = REDIR::ERR_OUT;
	} else if ( token_ == SHELL_PIPE ) {
		redir = REDIR::PIPE;
	} else if ( token_ == SHELL_PIPE_ERR ) {
		redir = REDIR::PIPE_ERR;
	}
	return ( redir );
}

yaal::hcore::HString&& unescape_system( yaal::hcore::HString&& token_ ) {
	semantic_unescape( token_ );
	util::unescape( token_, executing_parser::_escapes_ );
	util::unescape( token_ );
	return ( yaal::move( token_ ) );
}

bool is_shell_token( yaal::hcore::HString const& token_ ) {
	return (
		( str_to_redir( token_ ) != REDIR::NONE )
		|| ( token_ == SHELL_AND )
		|| ( token_ == SHELL_OR )
		|| ( token_ == SHELL_TERM )
		|| ( token_ == SHELL_BG )
	);
}

namespace {

void consume_token( tokens_t& tokens_, yaal::hcore::HString& token_, bool& hardSpace_, bool newHardSpace_ ) {
	if ( token_.is_empty() ) {
		return;
	}
	if ( hardSpace_ || tokens_.is_empty() ) {
		tokens_.push_back( yaal::move( token_ ) );
	} else {
		tokens_.back().append( token_ );
		token_.clear();
	}
	hardSpace_ = newHardSpace_;
	return;
}

void consume_token( tokens_t& tokens_, yaal::hcore::HString& token_ ) {
	bool dummy( true );
	consume_token( tokens_, token_, dummy, false );
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
	substitute_environment( unescaped, ENV_SUBST_MODE::RECURSIVE );
	return ( unescaped );
	M_EPILOG
}

yaal::tools::string::tokens_t tokenize_shell( yaal::hcore::HString const& str_ ) {
	M_PROLOG
	char const SHELL_LIKE[] = "<>&|!;";
	character_class_t shellLike( SHELL_LIKE, static_cast<int>( sizeof ( SHELL_LIKE ) ) - 1 );
	tokens_t tokens;
	HString token;
	HString trialToken;
	bool escaped( false );
	bool inSingleQuotes( false );
	bool inDoubleQuotes( false );
	bool inExecQuotes( false );
	bool wasShellLike( false );
	bool hardSpace( false );
	auto non_exec = [&]( code_point_t c_ ) {
		bool isShellLike( shellLike.has( c_ ) );
		if ( isShellLike ) {
			if ( wasShellLike ) {
				trialToken.assign( token ).append( c_ );
				if ( ! is_shell_token( trialToken ) ) {
					consume_token( tokens, token, hardSpace, false );
				}
			} else {
				consume_token( tokens, token, hardSpace, false );
			}
		} else if ( wasShellLike && is_shell_token( token ) ) {
			hardSpace = ! token.is_empty();
			consume_token( tokens, token, hardSpace, true );
		}
		token.push_back( c_ );
		wasShellLike = isShellLike;
	};
	code_point_t execStart( unicode::CODE_POINT::INVALID );
	for ( code_point_t c : str_ ) {
		if ( escaped ) {
			escaped = false;
			token.push_back( c );
			continue;
		}
		if ( execStart != unicode::CODE_POINT::INVALID ) {
			inExecQuotes = c == '(';
			if ( inExecQuotes || ( execStart == '$' ) ) {
				if ( wasShellLike ) {
					consume_token( tokens, token, hardSpace, true );
				}
				wasShellLike = false;
				token.push_back( execStart );
				token.push_back( c );
				execStart = unicode::CODE_POINT::INVALID;
				continue;
			} else {
				non_exec( execStart );
				execStart = unicode::CODE_POINT::INVALID;
			}
		}
		bool inStrQuotes( inSingleQuotes || inDoubleQuotes );
		bool inQuotes( inStrQuotes || inExecQuotes );
		if ( c == '\\' ) {
			escaped = true;
			if ( wasShellLike ) {
				consume_token( tokens, token, hardSpace, true );
			}
			token.push_back( c );
			wasShellLike = false;
			continue;
		}
		if ( ! inStrQuotes && ( c == '\'' ) ) {
			if ( ! inExecQuotes ) {
				if ( wasShellLike ) {
					consume_token( tokens, token, hardSpace, true );
				}
				wasShellLike = false;
			}
			inSingleQuotes = true;
			token.push_back( c );
			continue;
		}
		if ( ! inStrQuotes && ( c == '"' ) ) {
			if ( ! inExecQuotes ) {
				if ( wasShellLike ) {
					consume_token( tokens, token, hardSpace, true );
				}
				wasShellLike = false;
			}
			inDoubleQuotes = true;
			token.push_back( c );
			continue;
		}
		if ( inSingleQuotes && ( c == '\'' ) ) {
			token.push_back( c );
			inSingleQuotes = false;
			continue;
		}
		if ( inDoubleQuotes && ( c == '"' ) ) {
			token.push_back( c );
			inDoubleQuotes = false;
			continue;
		}
		if ( inExecQuotes && ! ( inSingleQuotes || inDoubleQuotes ) && ( c == ')' ) ) {
			token.push_back( c );
			inExecQuotes = false;
			continue;
		}
		if ( inQuotes ) {
			token.push_back( c );
			continue;
		}
		if ( character_class<CHARACTER_CLASS::WHITESPACE>().has( c ) ) {
			hardSpace = hardSpace || is_shell_token( token );
			consume_token( tokens, token, hardSpace, true );
			continue;
		}
		if ( ! inQuotes && ( ( c == '$' ) || ( c == '<' ) || ( c == '>' ) ) ) {
			execStart = c;
			continue;
		}
		non_exec( c );
	}
	hardSpace = hardSpace || is_shell_token( token );
	consume_token( tokens, token, hardSpace, false );
	if ( ! tokens.is_empty() && tokens.back().is_empty() ) {
		tokens.pop_back();
	}
	return ( tokens );
	M_EPILOG
}

namespace {

int scan_number( HString& str_, int long& pos_ ) {
	int len( static_cast<int>( str_.get_length() ) );
	int s( static_cast<int>( pos_ ) );
	int i( s );
	if ( ( i < len ) && ( str_[i] == '-' ) ) {
		++ i;
	}
	while ( ( i < len ) && is_digit( str_[i] ) ) {
		++ i;
	}
	pos_ = i;
	return ( lexical_cast<int>( str_.substr( s, i - s ) ) );
}

struct OBrace {
	int long _start;
	int long _end;
	bool _comma;
	bool _completed;
	OBrace( void )
		: _start( HString::npos )
		, _end( HString::npos )
		, _comma( false )
		, _completed( false ) {
	}
};

//
// aa{bb,{cc,dd}ee}ff{gg,hh}ii
//

}

tokens_t brace_expansion( yaal::hcore::HString const& str_ ) {
	M_PROLOG
	tokens_t exploded;
	typedef HQueue<HString> explode_queue_t;
	typedef HArray<OBrace> braces_t;
	braces_t braces;
	explode_queue_t explodeQueue;
	HString current;
	explodeQueue.push( str_ );
	tokens_t variants;
	HString cache;
	while ( ! explodeQueue.is_empty() ) {
		current.assign( explodeQueue.front() );
		explodeQueue.pop();
		braces.clear();
		braces.resize( count( str_.begin(), str_.end(), '{'_ycp ), OBrace() );
		int level( -1 );
		HQuoteObserver qo;
		for ( int long i( 0 ), LEN( current.get_length() ); i < LEN; ++ i ) {
			code_point_t c( current[i] );
			if ( qo.notice( c ) ) {
				continue;
			}
			if ( c == '{' ) {
				++ level;
				if ( ! braces[level]._completed ) {
					braces[level]._start = i;
				}
				continue;
			}
			if ( ( c == ',' ) && ( level >= 0 ) ) {
				braces[level]._comma = true;
				continue;
			}
			if ( ( c == '}' ) && ( level >= 0 ) ) {
				if ( ! braces[level]._completed && braces[level]._comma ) {
					braces[level]._end = i;
					braces[level]._completed = true;
				}
				-- level;
				continue;
			}
		}
		qo.reset();
		braces_t::const_iterator it( find_if( braces.begin(), braces.end(), hcore::call( &OBrace::_completed, _1 ) == true ) );
		if ( it != braces.end() ) {
			level = 0;
			variants.clear();
			int long s( it->_start + 1 );
			for ( int long i( s ); i <= it->_end; ++ i ) {
				code_point_t c( current[i] );
				if ( qo.notice( c ) ) {
					continue;
				}
				if ( c == '{' ) {
					++ level;
				} else if ( ( ( c == ',' ) || ( c == '}' ) ) && ( level == 0 ) ) {
					variants.push_back( current.substr( s, i - s ) );
					s = i + 1;
				} else if ( c == '}' ) {
					-- level;
				}
			}
			for ( HString const& v : variants ) {
				cache.assign( current.substr( 0, it->_start ) ).append( v ).append( current.substr( it->_end + 1 ) );
				explodeQueue.push( cache );
			}
		} else {
			int from( 0 );
			int to( 0 );
			int step( 1 );
			int long start( HString::npos );
			int long end( HString::npos );
			for ( int long i( 0 ), LEN( current.get_length() ); i < LEN; ++ i ) {
				if ( qo.notice( current[i] ) ) {
					continue;
				}
				if ( current[i] == '{' ) {
					start = i;
					++ i;
					if ( i == LEN ) {
						break;
					}
					do {
						try {
							from = scan_number( current, i );
						} catch ( HException const& ) {
							break;
						}
						if ( ( i == LEN ) || ( current[i] != '.' ) ) {
							break;
						}
						++ i;
						if ( ( i == LEN ) || ( current[i] != '.' ) ) {
							break;
						}
						++ i;
						try {
							to = scan_number( current, i );
						} catch ( HException const& ) {
							break;
						}
						if ( ( i < LEN ) && ( current[i] == '}' ) ) {
							end = i;
							break;
						}
						if ( ( i == LEN ) || ( current[i] != '.' ) ) {
							break;
						}
						++ i;
						if ( ( i == LEN ) || ( current[i] != '.' ) ) {
							break;
						}
						++ i;
						try {
							step = abs( scan_number( current, i ) );
						} catch ( HException const& ) {
							break;
						}
						if ( ( i == LEN ) || ( current[i] != '}' ) ) {
							break;
						}
						end = i;
					} while ( false );
					if ( end != HString::npos ) {
						break;
					}
					i = start;
				}
			}
			if ( end != HString::npos ) {
				bool inc( from < to );
				for ( int i( from ); inc ? ( i <= to ) : ( i >= to ); i += ( inc ? step : -step ) ) {
					cache.assign( current.substr( 0, start ) ).append( i ).append( current.substr( end + 1 ) );
					explodeQueue.push( cache );
				}
			} else {
				exploded.push_back( current );
			}
		}
	}
	return ( exploded );
	M_EPILOG
}

yaal::tools::string::tokens_t tokenize_quotes( yaal::hcore::HString const& str_ ) {
	tokens_t tokens;
	HString token;
	bool escaped( false );
	bool inSingleQuotes( false );
	bool inDoubleQuotes( false );
	bool inExecQuotes( false );
	code_point_t execStart( unicode::CODE_POINT::INVALID );
	for ( code_point_t c : str_ ) {
		if ( escaped ) {
			escaped = false;
			token.push_back( c );
			continue;
		}
		if ( execStart != unicode::CODE_POINT::INVALID ) {
			inExecQuotes = c == '(';
			if ( inExecQuotes ) {
				consume_token( tokens, token );
			}
			token.push_back( execStart );
			token.push_back( c );
			execStart = unicode::CODE_POINT::INVALID;
			continue;
		}
		bool inStrQuotes( inSingleQuotes || inDoubleQuotes );
		bool inQuotes( inStrQuotes || inExecQuotes );
		if ( c == '\\' ) {
			escaped = true;
			token.push_back( c );
			continue;
		}
		if ( ! inStrQuotes && ( c == '\'' ) ) {
			inSingleQuotes = true;
			if ( ! inExecQuotes ) {
				consume_token( tokens, token );
			}
			token.push_back( c );
			continue;
		}
		if ( ! inStrQuotes && ( c == '"' ) ) {
			inDoubleQuotes = true;
			if ( ! inExecQuotes ) {
				consume_token( tokens, token );
			}
			token.push_back( c );
			continue;
		}
		if ( inSingleQuotes && ( c == '\'' ) ) {
			token.push_back( c );
			if ( ! inExecQuotes ) {
				consume_token( tokens, token );
			}
			inSingleQuotes = false;
			continue;
		}
		if ( inDoubleQuotes && ( c == '"' ) ) {
			token.push_back( c );
			if ( ! inExecQuotes ) {
				consume_token( tokens, token );
			}
			inDoubleQuotes = false;
			continue;
		}
		if ( inExecQuotes && ! ( inSingleQuotes || inDoubleQuotes ) && ( c == ')' ) ) {
			token.push_back( c );
			consume_token( tokens, token );
			inExecQuotes = false;
			continue;
		}
		if ( ! inQuotes && ( ( c == '$' ) || ( c == '<' ) || ( c == '>' ) ) ) {
			execStart = c;
			continue;
		}
		token.push_back( c );
	}
	consume_token( tokens, token );
	if ( ! tokens.is_empty() && tokens.back().is_empty() ) {
		tokens.pop_back();
	}
	return ( tokens );
}

void denormalize_path( filesystem::path_t& path_, bool substEnv_ ) {
	M_PROLOG
	if ( substEnv_ ) {
		substitute_environment( path_, ENV_SUBST_MODE::RECURSIVE );
	}
	if ( ! path_.is_empty() && ( path_.front() == '~' ) ) {
		path_.replace( 0, 1, system::home_path() );
	}
	return;
	M_EPILOG
}

}

