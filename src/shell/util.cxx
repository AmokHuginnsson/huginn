/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <yaal/hcore/system.hxx>
#include <yaal/tools/util.hxx>
#include <yaal/tools/hfsitem.hxx>
#include <yaal/tools/filesystem.hxx>

#include "util.hxx"
#include "src/systemshell.hxx"
#include "src/quotes.hxx"
#include "capture.hxx"

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;
using namespace yaal::tools::util;
using namespace yaal::tools::string;

namespace huginn {

char const ARG_AT[] = "${@}";

yaal::hcore::HString stringify_command( yaal::tools::string::tokens_t const& tokens_, int skip_ ) {
	M_PROLOG
	HString s;
	for ( HString const& token : tokens_ ) {
		if ( skip_ > 0 ) {
			-- skip_;
			continue;
		}
		if ( ! s.is_empty() ) {
			s.append( " " );
		}
		s.append( token );
	}
	return ( s );
	M_EPILOG
}

filesystem::path_t compact_path( filesystem::path_t const& path_ ) {
	HString hp( system::home_path() );
	if ( hp.is_empty() ) {
		return ( path_ );
	}
	if ( ! path_.starts_with( hp ) ) {
		return ( path_ );
	}
	return ( "~"_ys.append( path_.substr( hp.get_length() ) ) );
}

bool is_suid( yaal::tools::filesystem::path_t const& path_ ) {
	HFSItem fsItem( path_ );
	if ( ! fsItem ) {
		return ( false );
	}
	if ( ! fsItem.is_executable() ) {
		return ( false );
	}
	return ( ( fsItem.get_permissions() & 06000 ) != 0 );
}

inline bool dot_start( yaal::hcore::HString const& s_ ) {
	return ( ! s_.is_empty() && ( s_.front() == '.' ) );
}

void apply_glob( yaal::tools::string::tokens_t& interpolated_, yaal::hcore::HString&& param_, bool wantGlob_ ) {
	M_PROLOG
	if ( ! wantGlob_ ) {
		interpolated_.push_back( unescape_system( yaal::move( param_ ) ) );
		return;
	}
	semantic_unescape( param_ );
	filesystem::paths_t fr( filesystem::glob( param_ ) );
	if ( ! fr.is_empty() ) {
		if ( dot_start( param_ ) ) {
			interpolated_.insert( interpolated_.end(), fr.begin(), fr.end() );
		} else {
			remove_copy_if(
				fr.begin(),
				fr.end(),
				back_insert_iterator( interpolated_ ),
				dot_start
			);
		}
	} else {
		interpolated_.push_back( unescape_system( yaal::move( param_ ) ) );
	}
	return;
	M_EPILOG
}

namespace {

tokens_t tokenize_shell_tilda( yaal::hcore::HString const& str_ ) {
	M_PROLOG
	tokens_t tokens( tokenize_shell( str_ ) );
	for ( HString& t : tokens ) {
		denormalize_path( t );
	}
	return ( tokens );
	M_EPILOG
}

}

void HSystemShell::resolve_string_aliases( tokens_t& tokens_, tokens_t::iterator it ) const {
	M_PROLOG
	HLock l( _mutex );
	if ( tokens_.is_empty() ) {
		return;
	}
	typedef yaal::hcore::HHashSet<yaal::hcore::HString> alias_hit_t;
	alias_hit_t aliasHit;
	tokens_t aliasBody;
	while ( true ) {
		aliases_t::const_iterator a( _aliases.find( *it ) );
		if ( a == _aliases.end() ) {
			break;
		}
		if ( ! aliasHit.insert( *it ).second ) {
			break;
		}
		HString head( a->second.front() );
		try {
			QUOTES quotes( str_to_quotes( head ) );
			if ( ( quotes != QUOTES::SINGLE ) && ( quotes != QUOTES::DOUBLE ) ) {
				break;
			}
		} catch ( HException const& ) {
			break;
		}
		strip_quotes( head );
		aliasBody = tokenize_shell_tilda( head );
		tokens_.erase( it );
		tokens_.insert( it, a->second.begin(), a->second.end() );
		tokens_.erase( it );
		tokens_.insert( it, aliasBody.begin(), aliasBody.end() );
	}
	return;
	M_EPILOG
}

HSystemShell::chains_t HSystemShell::split_chains( yaal::hcore::HString const& str_, EVALUATION_MODE evaluationMode_ ) const {
	M_PROLOG
	tokens_t tokens( tokenize_shell_tilda( str_ ) );
	if ( evaluationMode_ != EVALUATION_MODE::TRIAL ) {
		bool head( true );
		for ( tokens_t::iterator it( tokens.begin() ); it != tokens.end(); ++ it ) {
			if ( head ) {
				resolve_string_aliases( tokens, it );
				head = false;
			}
			if ( ( *it == SHELL_TERM ) || ( *it == SHELL_BG ) ) {
				head = true;
			}
		}
	}
	HSystemShell::chains_t chains;
	chains.emplace_back( tokens_t() );
	for ( HString const& t : tokens ) {
		if ( ( t == SHELL_TERM ) || ( t == SHELL_BG ) ) {
			if ( ! chains.back()._tokens.is_empty() ) {
				chains.back()._background = t == SHELL_BG;
				chains.emplace_back( tokens_t() );
			}
			continue;
		} else if ( ! t.is_empty() && ( t.front() == '#' ) ) {
			break;
		}
		chains.back()._tokens.push_back( t );
	}
	return ( chains );
	M_EPILOG
}

yaal::hcore::HString HSystemShell::expand( yaal::hcore::HString&& str_ ) {
	return ( stringify_command( interpolate( str_, EVALUATION_MODE::DIRECT ) ) );
}

int HSystemShell::job_count( void ) const {
	HLock l( _mutex );
	int jobCount( static_cast<int>( _jobs.get_size() ) );
	return ( jobCount );
}

namespace {

yaal::hcore::HString subst_argv( HSystemShell::argvs_t const& argvs_, yaal::hcore::HString const& token_ ) {
	if ( argvs_.is_empty() ) {
		return ( HString() );
	}
	HString val( token_ );
	val.shift_left( 2 ).pop_back();
	int n( -1 );
	try {
		n = lexical_cast<int>( val );
	} catch ( ... ) {
	}
	HSystemShell::tokens_t const& argv( argvs_.top() );
	return ( ( n >= 0 ) && ( n < argv.get_size() ) ? argv[n] : HString() );
}

}

void HSystemShell::substitute_from_shell( yaal::hcore::HString& token_, QUOTES quotes_ ) const {
	M_PROLOG
	HLock l( _mutex );
	M_ASSERT( quotes_ != QUOTES::SINGLE );
	HRegex re( "\\${\\d+}" );
	util::escape_mask_map_t emm;
	HScopeExitCall sec(
		call( mask_escape, ref( token_ ), ref( emm ), '\\'_ycp ),
		call( unmask_escape, ref( token_ ), cref( emm ), '\\'_ycp )
	);
	token_.assign( re.replace( token_, call( subst_argv, cref( _argvs ), _1 ) ) );
	char const ARG_STAR[] = "${*}";
	system::argv_t emptyArgv;
	system::argv_t const& argv( ! _argvs.is_empty() ? _argvs.top() : emptyArgv );

	if ( ( token_.find( ARG_STAR ) != HString::npos ) || ( ( quotes_ != QUOTES::DOUBLE ) && ( token_.find( ARG_AT ) != HString::npos ) ) ) {
		HString argvStr;
		bool skip( true );
		for ( HString const& arg : argv ) {
			if ( skip ) {
				skip = false;
				continue;
			}
			if ( ! argvStr.is_empty() ) {
				argvStr.push_back( ' '_ycp );
			}
			argvStr.append( arg );
		}
		token_.replace( ARG_STAR, argvStr );
		if ( quotes_ != QUOTES::DOUBLE ) {
			token_.replace( ARG_AT, argvStr );
		}
	}
	token_.replace( "${#}", to_string( argv.get_size() - 1 ) );
	return;
	M_EPILOG
}

bool HSystemShell::substitute_arg_at( tokens_t& interpolated_, yaal::hcore::HString& param_, yaal::hcore::HString& token_ ) const {
	M_PROLOG
	util::escape_mask_map_t emm;
	mask_escape( token_, emm, '\\'_ycp );
	tokens_t tokens;
	system::argv_t emptyArgv;
	system::argv_t const& argv( ! _argvs.is_empty() ? _argvs.top() : emptyArgv );
	bool argAtSubstituted( false );
	while ( ! token_.is_empty() ) {
		HString::size_type argAtPos( token_.find( ARG_AT ) );
		if ( argAtPos == HString::npos ) {
			param_.append( token_ );
			break;
		}
		argAtSubstituted = true;
		param_.append( token_, 0, argAtPos );
		token_.shift_left( argAtPos + 4 );
		if ( argv.get_size() < 2 ) {
			continue;
		}
		param_.append( argv[1] );
		if ( argv.get_size() == 2 ) {
			continue;
		}
		tokens.push_back( yaal::move( param_ ) );
		tokens.insert( tokens.end(), argv.begin() + 2, argv.end() - 1 );
		param_.assign( argv.back() );
	}
	for ( yaal::hcore::HString& token : tokens ) {
		util::unmask_escape( token, emm, '\\'_ycp );
	}
	interpolated_.insert( interpolated_.end(), tokens.begin(), tokens.end() );
	util::unmask_escape( param_, emm, '\\'_ycp );
	param_.replace( "\\", "\\\\" );
	return ( argAtSubstituted );
	M_EPILOG
}

void HSystemShell::substitute_command( yaal::hcore::HString& token_ ) {
	M_PROLOG
	HLock l( _mutex );
	bool escaped( false );
	bool execStart( false );
	bool inExecQuotes( false );
	HString tmp( yaal::move( token_ ) );
	HString subst;
	for ( code_point_t c : tmp ) {
		if ( escaped ) {
			( inExecQuotes ? subst : token_ ).push_back( c );
			escaped = false;
			continue;
		}
		if ( execStart ) {
			inExecQuotes = c == '(';
			execStart = false;
			if ( ! inExecQuotes ) {
				token_.push_back( '$'_ycp );
				token_.push_back( c );
			}
			continue;
		}
		if ( inExecQuotes && ( c == ')' ) ) {
			HCapture capture( QUOTES::EXEC );
			run_line( subst, EVALUATION_MODE::COMMAND_SUBSTITUTION, &capture );
			token_.append( capture.buffer() );
			subst.clear();
			inExecQuotes = false;
			continue;
		}
		if ( c == '\\' ) {
			( inExecQuotes ? subst : token_ ).push_back( c );
			escaped = true;
			continue;
		}
		if ( c == '$' ) {
			execStart = true;
			continue;
		}
		( inExecQuotes ? subst : token_ ).push_back( c );
	}
	return;
	M_EPILOG
}

}

