/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <yaal/hcore/hcore.hxx>
#include <yaal/tools/hfsitem.hxx>
#include <yaal/tools/huginn/helper.hxx>

M_VCSID( "$Id: " __ID__ " $" )
M_VCSID( "$Id: " __TID__ " $" )

#include "src/systemshell.hxx"
#include "src/quotes.hxx"
#include "src/main.hxx"
#include "util.hxx"
#include "src/colorize.hxx"

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;
using namespace yaal::tools::util;
using namespace yaal::tools::string;

namespace huginn {

namespace {
extern "C" {
extern char** environ;
}
int complete_environment_variable( HRepl::completions_t& completions_, yaal::hcore::HString const& prefix_, yaal::hcore::HString const& suffix_, HSystemShell const* systemShell_ ) {
	int added( 0 );
	HString value;
	HRepl::completions_t completions;
	for ( char** e( environ ); *e; ++ e ) {
		HString envVar( *e );
		HString::size_type eqPos( envVar.find( '='_ycp ) );
		if ( eqPos != HString::npos ) {
			value.assign( envVar, eqPos + 1 );
			envVar.erase( eqPos );
		} else {
			value.clear();
		}
		if ( envVar.starts_with( prefix_ ) ) {
			completions.emplace_back( envVar + suffix_, file_color( yaal::move( value ), systemShell_, color( GROUP::ENVIRONMENT ) ) );
			++ added;
		}
	}
	arrange( completions );
	concat( completions_, completions );
	return ( added );
}
}

bool HSystemShell::fallback_completions( tokens_t const& tokens_, yaal::hcore::HString const& prefix_, completions_t& completions_ ) const {
	M_PROLOG
	HString context( ( tokens_.get_size() == 1 ) ? tokens_.front() : "" );
	HString prefix( ! tokens_.is_empty() ? tokens_.back() : HString() );
	if ( prefix.starts_with( "${" ) ) {
		HString varName( prefix.substr( 2 ) );
		int added( complete_environment_variable( completions_, varName, HString(), this ) );
		if ( added == 1 ) {
			completions_.back() = HRepl::HCompletion( completions_.back().text() + "}" );
		}
		if ( added > 0 ) {
			return ( true );
		}
	}
	if ( prefix_.is_empty() || context.is_empty() ) {
		return ( false );
	}
	completions_t completions;
	for ( system_commands_t::value_type const& sc : _systemCommands ) {
		if ( sc.first.starts_with( context ) ) {
			completions.emplace_back( sc.first + " ", color( GROUP::EXECUTABLES ) );
		}
	}
	for ( builtins_t::value_type const& b : _builtins ) {
		if ( b.first.starts_with( context ) ) {
			completions.emplace_back( b.first + " ", color( GROUP::SHELL_BUILTINS ) );
		}
	}
	for ( aliases_t::value_type const& a : _aliases ) {
		if ( a.first.starts_with( context ) ) {
			completions.emplace_back( a.first + " ", color( GROUP::ALIASES ) );
		}
	}
	arrange( completions );
	concat( completions_, completions );
	return( false );
	M_EPILOG
}

void HSystemShell::filename_completions(
	tokens_t const& tokens_,
	yaal::hcore::HString const& prefix_, FILENAME_COMPLETIONS filenameCompletions_,
	yaal::hcore::HString const& suffix_, completions_t& completions_, bool maybeExec_, bool fresh_
) const {
	M_PROLOG
	static HString const SEPARATORS( "/\\" );
	bool wantExec( tokens_.is_empty() || ( ( tokens_.get_size() == 1 ) && maybeExec_ ) );
	HString context( ! tokens_.is_empty() ? tokens_.back() : "" );
	context = unescape_system( yaal::move( context ) );
	HString prefix(
		! context.is_empty()
		&& ! prefix_.is_empty()
		&& ( SEPARATORS.find( context.back() ) == HString::npos )
			? filesystem::basename( context )
			: ( context == "." ? "." : "" )
	);
	HString path;
	HRegex pattern;
	if ( ! suffix_.is_empty() ) {
		try {
			pattern.compile( suffix_ );
		} catch ( HRegexException const& ) {
		}
	}
	context.erase( context.get_length() - prefix.get_length() );
	substitute_environment( context, ENV_SUBST_MODE::RECURSIVE );
	if ( ! fresh_ && filesystem::exists( context ) ) {
		path.assign( context );
	}
	if ( path.is_empty() ) {
		path.assign( "." ).append( PATH_SEP );
	}
	path = unescape_system( yaal::move( path ) );
	prefix = unescape_system( yaal::move( prefix ) );
	substitute_environment( path, ENV_SUBST_MODE::RECURSIVE );
	HFSItem dir( path );
	if ( ! dir.is_directory() ) {
		return;
	}
	HString name;
	completions_t dirs;
	completions_t files;
	int ignored( 0 );
	for ( HFSItem const& f : dir ) {
		bool ignoredThis( false );
		name.assign( f.get_name() );
		if ( _ignoredFiles.is_valid() && _ignoredFiles.matches( name ) ) {
			ignoredThis = true;
		}
		if ( ! prefix.is_empty() && ( name.find( prefix ) != 0 ) ) {
			continue;
		}
		if ( prefix.is_empty() && ( name.front() == '.' ) ) {
			continue;
		}
		bool isDirectory( f.is_directory() );
		bool isExec( f.is_executable() );
		if ( ( filenameCompletions_ == FILENAME_COMPLETIONS::DIRECTORY ) && ! isDirectory ) {
			continue;
		}
		if ( ( filenameCompletions_ == FILENAME_COMPLETIONS::EXECUTABLE ) && ! isExec ) {
			continue;
		}
		if ( wantExec && ! ( isDirectory || isExec ) ) {
			continue;
		}
		if ( ! isDirectory && pattern.is_valid() && ! pattern.matches( name ) ) {
			continue;
		}
		name = escape_path( name );
		bool isDir( f.is_directory() );
		( isDir ? dirs : files ).emplace_back( name + ( isDir ? '/'_ycp : ' '_ycp ), file_color( path + name, this ) );
		if ( ignoredThis ) {
			++ ignored;
		}
	}
	arrange( dirs );
	arrange( files );
	completions_t completions( yaal::move( dirs ) );
	concat( completions, files );
	if ( _ignoredFiles.is_valid() && ( ( completions.get_size() - ignored ) > 0 ) ) {
		completions.erase(
			remove_if(
				completions.begin(),
				completions.end(),
				[this, &name]( HRepl::HCompletion const& c ) -> bool {
					name.assign( c.text() ).trim();
					return ( _ignoredFiles.matches( name ) );
				}
			),
			completions.end()
		);
	}
	concat( completions_, completions );
	return;
	M_EPILOG
}

void HSystemShell::user_completions( yaal::tools::HHuginn::value_t const& userCompletions_, tokens_t const& tokens_, yaal::hcore::HString const& prefix_, completions_t& completions_ ) const {
	M_PROLOG
	HHuginn::type_id_t t( !! userCompletions_ ? userCompletions_->type_id() : tools::huginn::type_id( HHuginn::TYPE::NONE ) );
	if ( t == HHuginn::TYPE::TUPLE ) {
		tools::huginn::HTuple::values_t const& data( static_cast<tools::huginn::HTuple const*>( userCompletions_.raw() )->value() );
		for ( HHuginn::value_t const& v : data ) {
			user_completions( v, tokens_, prefix_, completions_ );
		}
	} else if ( t == HHuginn::TYPE::LIST ) {
		tools::huginn::HList::values_t const& data( static_cast<tools::huginn::HList const*>( userCompletions_.raw() )->value() );
		completions_t completions;
		for ( HHuginn::value_t const& v : data ) {
			if ( v->type_id() != HHuginn::TYPE::STRING ) {
				continue;
			}
			HString const& completion( tools::huginn::get_string( v ) );
			completions.emplace_back( completion, completion.front() == '-' ? color( GROUP::SWITCHES ) : COLOR::ATTR_DEFAULT );
		}
		arrange( completions );
		concat( completions_, completions );
	} else if ( t == HHuginn::TYPE::STRING ) {
		completions_from_string( tools::huginn::get_string( userCompletions_ ), tokens_, prefix_, completions_ );
	}
	if ( completions_.is_empty() ) {
		fallback_completions( tokens_, prefix_, completions_ );
	}
	return;
	M_EPILOG
}

void HSystemShell::completions_from_commands( yaal::hcore::HString const& prefix_, yaal::hcore::HString const& suffix_, completions_t& completions_ ) const {
	M_PROLOG
	for ( system_commands_t::value_type const& sc : _systemCommands ) {
		if ( sc.first.starts_with( prefix_ ) ) {
			completions_.push_back( sc.first + suffix_ );
		}
	}
	return;
	M_EPILOG
}

void HSystemShell::completions_from_su_commands( yaal::hcore::HString const& prefix_, yaal::hcore::HString const& suffix_, completions_t& completions_ ) const {
	M_PROLOG
	for ( system_commands_t::value_type const& sc : _systemSuperUserCommands ) {
		if ( sc.first.starts_with( prefix_ ) ) {
			completions_.push_back( sc.first + suffix_ );
		}
	}
	return;
	M_EPILOG
}

void HSystemShell::completions_from_string( yaal::hcore::HString const& completionAction_, tokens_t const& tokens_, yaal::hcore::HString const& prefix_, completions_t& completions_ ) const {
	M_PROLOG
	HString completionAction( completionAction_ );
	completionAction.lower();
	static char const PREFIX[] = "prefix";
	HString::size_type colonPos( completionAction_.find( ':'_ycp ) );
	HString suffix;
	if ( colonPos != HString::npos ) {
		suffix.assign( completionAction_, colonPos + 1 );
		completionAction.erase( colonPos );
	}
	if ( ( completionAction == "directories" ) || ( completionAction == "dirs" ) ) {
		filename_completions( tokens_, prefix_, FILENAME_COMPLETIONS::DIRECTORY, HString(), completions_, false, false );
	} else if ( completionAction == "files" ) {
		filename_completions( tokens_, prefix_, FILENAME_COMPLETIONS::FILE, suffix, completions_, false, false );
	} else if ( completionAction == "executables" ) {
		if ( tokens_.is_empty() || ( tokens_.back().find( PATH_SEP ) == HString::npos ) ) {
			completions_from_commands( prefix_, suffix, completions_ );
			completions_from_su_commands( prefix_, suffix, completions_ );
		} else {
			filename_completions( tokens_, prefix_, FILENAME_COMPLETIONS::EXECUTABLE, HString(), completions_, false, false );
		}
	} else if ( completionAction == "commands" ) {
		completions_from_commands( prefix_, suffix, completions_ );
	} else if (
		( completionAction == "su-commands" )
		|| ( completionAction == "sudo-commands" )
		|| ( completionAction == "super-user-commands" )
	) {
		completions_from_su_commands( prefix_, suffix, completions_ );
	} else if ( completionAction == "aliases" ) {
		for ( aliases_t::value_type const& a : _aliases ) {
			if ( a.first.starts_with( prefix_ ) ) {
				completions_.push_back( a.first + suffix );
			}
		}
	} else if (
		( completionAction == "environmentvariables" )
		|| ( completionAction == "environment-variables" )
		|| ( completionAction == "envvars" )
		|| ( completionAction == "environment" )
	) {
		complete_environment_variable( completions_, prefix_, suffix, this );
	} else if ( completionAction == PREFIX ) {
		static int const PREFIX_LEN( static_cast<int>( sizeof ( PREFIX ) ) - 1 );
		colonPos = completionAction_.find( ':'_ycp, PREFIX_LEN + 1 );
		if ( colonPos == HString::npos ) {
			return;
		}
		tokens_t tokens( tokens_ );
		if ( ! tokens.is_empty() ) {
			tokens.back() = completionAction_.mid( colonPos + 1 );
		}
		completions_from_string( completionAction_.substr( PREFIX_LEN + 1, colonPos - ( PREFIX_LEN + 1 ) ), tokens, prefix_, completions_ );
	}
	return;
	M_EPILOG
}

template<typename coll_t>
bool is_prefix_impl( coll_t const& coll_, yaal::hcore::HString const& stem_ ) {
	M_PROLOG
	typename coll_t::const_iterator it( coll_.lower_bound( stem_ ) );
	while ( it != coll_.end() ) {
		if ( it->first.starts_with( stem_ ) ) {
			return ( true );
		}
		++ it;
	}
	return ( false );
	M_EPILOG
}

bool HSystemShell::is_prefix( yaal::hcore::HString const& stem_ ) const {
	M_PROLOG
	return ( is_prefix_impl( _builtins, stem_ ) || is_prefix_impl( _systemCommands, stem_ ) || is_prefix_impl( _aliases, stem_ ) );
	M_EPILOG
}

namespace {

inline bool is_file_redirection( REDIR redir_ ) {
	return (
		( redir_ == REDIR::IN )
		|| ( redir_ == REDIR::OUT )
		|| ( redir_ == REDIR::ERR )
		|| ( redir_ == REDIR::OUT_ERR )
		|| ( redir_ == REDIR::APP_OUT )
		|| ( redir_ == REDIR::APP_ERR )
		|| ( redir_ == REDIR::APP_OUT_ERR )
	);
}

bool ends_with_whitespace( yaal::hcore::HString const& s_ ) {
	bool endsWithWhitespace( false );
	bool escape( false );
	for ( code_point_t cp : s_ ) {
		endsWithWhitespace = false;
		if ( escape ) {
			escape = false;
			continue;
		}
		if ( cp == '\\' ) {
			escape = true;
			continue;
		}
		endsWithWhitespace = character_class<CHARACTER_CLASS::WHITESPACE>().has( cp );
	}
	return ( endsWithWhitespace );
}

}

HShell::completions_t HSystemShell::do_gen_completions( yaal::hcore::HString const& context_, yaal::hcore::HString const& prefix_, bool hints_ ) const {
	M_PROLOG
	chains_t chains( split_chains( context_, EVALUATION_MODE::TRIAL ) );
	tokens_t tokens( ! chains.is_empty() ? chains.back()._tokens : tokens_t() );
	REDIR redir( REDIR::NONE );
	if ( ! tokens.is_empty() ) {
		util::escape_mask_map_t emm;
		mask_escape( tokens.back(), emm, '\\'_ycp );
		HString::size_type subPos( HString::npos );
		char const substPrefixes[][3] = { "$(", "<(", ">(" };
		for ( char const* substPrefix : substPrefixes ) {
			HString::size_type pos( tokens.back().find_last( substPrefix ) );
			if ( ( pos != HString::npos ) && ( ( subPos == HString::npos ) || ( pos > subPos ) ) ) {
				subPos = pos;
			}
		}
		unmask_escape( tokens.back(), emm, '\\'_ycp );
		if ( subPos != HString::npos ) {
			HString context( tokens.back().mid( subPos + 2 ) );
			chains = split_chains( context, EVALUATION_MODE::TRIAL );
			tokens = ! chains.is_empty() ? chains.back()._tokens : tokens_t();
		}
	}
	for ( tokens_t::iterator it( tokens.begin() ); it != tokens.end(); ) {
		REDIR newRedir = str_to_redir( *it );
		if ( is_shell_token( *it ) ) {
			if ( ! is_file_redirection( newRedir ) || ( redir == REDIR::NONE ) ) {
				redir = newRedir;
			}
			++ it;
			it = tokens.erase( tokens.begin(), it );
		} else {
			++ it;
		}
	}
	bool isFileRedirection( is_file_redirection( redir ) );
	tokens.erase( remove( tokens.begin(), tokens.end(), "" ), tokens.end() );
	bool endsWithWhitespace( ends_with_whitespace( context_ ) );
	if ( endsWithWhitespace ) {
		tokens.push_back( "" );
	}
	bool isPrefix( ! tokens.is_empty() && is_prefix( tokens.front() ) );
	HHuginn::value_t userCompletions(
		! ( tokens.is_empty() || ( ( tokens.get_size() == 1 ) && isPrefix && ! endsWithWhitespace ) || isFileRedirection )
			? _lineRunner.call( "complete", { _lineRunner.huginn()->value( tokens ) }, setup._verbose ? &cerr : nullptr )
			: HHuginn::value_t{}
	);
	if ( endsWithWhitespace ) {
		tokens.pop_back();
	}
	completions_t completions;
	if ( !! userCompletions && ( userCompletions->type_id() != HHuginn::TYPE::NONE ) ) {
		user_completions( userCompletions, tokens, prefix_, completions );
	} else {
		if ( isFileRedirection || ( ! fallback_completions( tokens, prefix_, completions ) && ! hints_ ) ) {
			filename_completions(
				tokens,
				prefix_,
				FILENAME_COMPLETIONS::FILE,
				HString(),
				completions,
				! endsWithWhitespace && ! isFileRedirection,
				endsWithWhitespace
			);
		}
	}
	return ( completions );
	M_EPILOG
}

}

