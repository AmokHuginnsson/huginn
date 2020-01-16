/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <yaal/hcore/system.hxx>
#include <yaal/tools/util.hxx>
#include <yaal/tools/hfsitem.hxx>
#include <yaal/tools/filesystem.hxx>

#include "util.hxx"
#include "src/systemshell.hxx"
#include "src/quotes.hxx"

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;
using namespace yaal::tools::util;
using namespace yaal::tools::string;

namespace huginn {

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
	if ( ! hp.is_empty() ) {
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

void apply_glob( yaal::tools::string::tokens_t& interpolated_, yaal::hcore::HString&& param_, bool wantGlob_ ) {
	M_PROLOG
	if ( ! wantGlob_ ) {
		interpolated_.push_back( unescape_system( yaal::move( param_ ) ) );
		return;
	}
	semantic_unescape( param_ );
	filesystem::paths_t fr( filesystem::glob( param_ ) );
	if ( ! fr.is_empty() ) {
		interpolated_.insert( interpolated_.end(), fr.begin(), fr.end() );
	} else {
		util::unescape( param_, executing_parser::_escapes_ );
		interpolated_.push_back( param_ );
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
			if ( ( *it == ";" ) || ( *it == "&" ) ) {
				head = true;
			}
		}
	}
	HSystemShell::chains_t chains;
	chains.emplace_back( tokens_t() );
	for ( HString const& t : tokens ) {
		if ( ( t == ";" ) || ( t == "&" ) ) {
			if ( ! chains.back()._tokens.is_empty() ) {
				chains.back()._background = t == "&";
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
	return ( static_cast<int>( _jobs.get_size() ) );
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

void HSystemShell::substitute_from_shell( yaal::hcore::HString& token_ ) const {
	M_PROLOG
	HRegex re( "\\${\\d+}" );
	util::escape_mask_map_t emm;
	HScopeExitCall sec(
		call( mask_escape, ref( token_ ), ref( emm ), '\\'_ycp ),
		call( unmask_escape, ref( token_ ), cref( emm ), '\\'_ycp )
	);
	token_.assign( re.replace( token_, call( subst_argv, cref( _argvs ), _1 ) ) );
	char const ARG_STAR[] = "${*}";
	char const ARG_AT[] = "${@}";
	tokens_t emptyArgv;
	HSystemShell::tokens_t const& argv( ! _argvs.is_empty() ? _argvs.top() : emptyArgv );

	if ( ( token_.find( ARG_STAR ) != HString::npos ) || ( token_.find( ARG_AT ) != HString::npos ) ) {
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
		token_.replace( ARG_STAR, argvStr ).replace( ARG_AT, argvStr );
	}
	token_.replace( "${#}", to_string( argv.get_size() - 1 ) );
	return;
	M_EPILOG
}

}

