/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include "util.hxx"
#include "src/systemshell.hxx"
#include "src/quotes.hxx"

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;
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

void HSystemShell::resolve_string_aliases( tokens_t& tokens_ ) const {
	M_PROLOG
	if ( tokens_.is_empty() ) {
		return;
	}
	typedef yaal::hcore::HHashSet<yaal::hcore::HString> alias_hit_t;
	alias_hit_t aliasHit;
	tokens_t aliasBody;
	while ( true ) {
		aliases_t::const_iterator a( _aliases.find( tokens_.front() ) );
		if ( a == _aliases.end() ) {
			break;
		}
		if ( ! aliasHit.insert( tokens_.front() ).second ) {
			break;
		}
		HString head( a->second.front() );
		QUOTES quotes( str_to_quotes( head ) );
		if ( ( quotes != QUOTES::SINGLE ) && ( quotes != QUOTES::DOUBLE ) ) {
			break;
		}
		strip_quotes( head );
		aliasBody = tokenize_shell_tilda( head );
		tokens_.erase( tokens_.begin() );
		tokens_.insert( tokens_.begin(), a->second.begin(), a->second.end() );
		tokens_.erase( tokens_.begin() );
		tokens_.insert( tokens_.begin(), aliasBody.begin(), aliasBody.end() );
	}
	return;
	M_EPILOG
}

HSystemShell::chains_t HSystemShell::split_chains( yaal::hcore::HString const& str_ ) const {
	M_PROLOG
	tokens_t tokens( tokenize_shell_tilda( str_ ) );
	resolve_string_aliases( tokens );
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
	while ( ! chains.is_empty() && chains.back()._tokens.is_empty() ) {
		chains.pop_back();
	}
	return ( chains );
	M_EPILOG
}

}

