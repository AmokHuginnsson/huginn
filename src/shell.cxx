/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <cstdlib>
#include <yaal/hcore/hcore.hxx>
#include <yaal/hcore/hfile.hxx>
#include <yaal/hcore/hrawfile.hxx>
#include <yaal/hcore/duration.hxx>
#include <yaal/tools/hpipedchild.hxx>
#include <yaal/tools/hfsitem.hxx>
#include <yaal/tools/hhuginn.hxx>
#include <yaal/tools/filesystem.hxx>

M_VCSID( "$Id: " __ID__ " $" )
M_VCSID( "$Id: " __TID__ " $" )

#include "shell.hxx"
#include "quotes.hxx"
#include "setup.hxx"

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;
using namespace yaal::tools::string;

namespace huginn {

namespace {

enum class QUOTES {
	NONE,
	SINGLE,
	DOUBLE
};

enum class REDIR {
	NONE,
	IN,
	OUT,
	APP,
	PIPE
};

#ifndef __MSVCXX__
code_point_t PATH_SEP = filesystem::path::SEPARATOR;
char const PATH_ENV_SEP[] = ":";
#else
code_point_t PATH_SEP = '\\'_ycp;
char const PATH_ENV_SEP[] = ";";
#endif
char const* const HOME_PATH( ::getenv( HOME_ENV_VAR ) );

void strip_quotes( HString& str_ ) {
	str_.pop_back();
	str_.shift_left( 1 );
	return;
}

QUOTES test_quotes( yaal::hcore::HString const& token_ ) {
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

REDIR test_redir( yaal::hcore::HString const& token_ ) {
	REDIR redir( REDIR::NONE );
	if ( token_ == "<" ) {
		redir = REDIR::IN;
	} else if ( token_ == ">" ) {
		redir = REDIR::OUT;
	} else if ( token_ == ">>" ) {
		redir = REDIR::APP;
	} else if ( token_ == "|" ) {
		redir = REDIR::PIPE;
	}
	return ( redir );
}

tokens_t split_quotes_tilda( yaal::hcore::HString const& str_ ) {
	M_PROLOG
	tokens_t tokens( split_quotes( str_ ) );
	for ( HString& t : tokens ) {
		if ( HOME_PATH && ! t.is_empty() && ( t.front() == '~' ) ) {
			t.replace( 0, 1, HOME_PATH );
		}
	}
	return ( tokens );
	M_EPILOG
}

}

HShell::HShell( HLineRunner& lr_ )
	: _lineRunner( lr_ )
	, _systemCommands()
	, _builtins()
	, _aliases() {
	M_PROLOG
	_builtins.insert( make_pair( "alias", call( &HShell::alias, this, _1 ) ) );
	_builtins.insert( make_pair( "cd", call( &HShell::cd, this, _1 ) ) );
	_builtins.insert( make_pair( "unalias", call( &HShell::unalias, this, _1 ) ) );
	char const* PATH_ENV( ::getenv( "PATH" ) );
	do {
		if ( ! PATH_ENV ) {
			break;
		}
		tokens_t paths( split<>( PATH_ENV, PATH_ENV_SEP ) );
		reverse( paths.begin(), paths.end() );
		for ( yaal::hcore::HString const& p : paths ) {
			HFSItem dir( p );
			if ( ! dir ) {
				continue;
			}
			for ( HFSItem const& file : dir ) {
				HString name( file.get_name() );
#ifndef __MSVCXX__
				if ( ! ( file.is_executable() && file.is_file() ) ) {
					continue;
				}
#else
				name.lower();
				HString ext( name.right( 4 ) );
				if ( ( ext != ".exe" ) && ( ext != ".com" ) && ( ext != ".cmd" ) && ( ext != ".bat" ) ) {
					continue;
				}
				name.erase( name.get_size() - 4 );
#endif
				_systemCommands[name] = p;
			}
		}
		HFile init( setup._sessionDir + PATH_SEP + "shell.init", HFile::OPEN::READING );
		if ( !! init ) {
			HString line;
			while ( getline( init, line ).good() ) {
				run( line );
			}
		}
	} while ( false );
	M_EPILOG
}

HShell::system_commands_t const& HShell::system_commands( void ) const {
	return ( _systemCommands );
}

bool HShell::run( yaal::hcore::HString const& line_ ) {
	M_PROLOG
	tokens_t tokens( split_quotes_tilda( line_ ) );
	if ( line_.is_empty() || ( line_.front() == '#' ) ) {
		return ( true );
	}
	builtins_t::const_iterator builtin( _builtins.find( tokens.front() ) );
	if ( builtin != _builtins.end() ) {
		builtin->second( tokens );
		return ( true );
	}
	bool ok( false );
	try {
		HString inPath;
		HString outPath;
		bool append( denormalize( tokens, inPath, outPath ) );
		if ( tokens.is_empty() ) {
			return ( true );
		}
		HStreamInterface* in( &cin );
		HFile fileIn;
		if ( ! inPath.is_empty() ) {
			fileIn.open( inPath, HFile::OPEN::READING );
			in = &fileIn;
		}
		HStreamInterface* out( &cout );
		HFile fileOut;
		if ( ! outPath.is_empty() ) {
			fileOut.open( outPath, append ? HFile::OPEN::WRITING | HFile::OPEN::APPEND : HFile::OPEN::WRITING );
			out = &fileOut;
		}
		HPipedChild pc;
		if ( setup._shell->is_empty() ) {
			HString image( tokens.front() );
#ifdef __MSVCXX__
			image.lower();
#endif
			system_commands_t::const_iterator it( _systemCommands.find( image ) );
			if ( it != _systemCommands.end() ) {
#ifndef __MSVCXX__
				image = it->second + PATH_SEP + image;
#else
				char const exts[][8] = { ".cmd", ".com", ".exe" };
				for ( char const* e : exts ) {
					image = it->second + PATH_SEP + tokens.front() + e;
					if ( filesystem::exists( image ) ) {
						break;
					}
				}
#endif
			}
			tokens.erase( tokens.begin() );
			pc.spawn( image, tokens, in, out, &cerr );
		} else {
			pc.spawn( *setup._shell, { "-c", join( tokens, " " ) }, in, out, &cerr );
		}
		ok = true;
		static time::duration_t const CENTURY( time::duration( 520, time::UNIT::WEEK ) );
		static int const CENTURY_IN_SECONDS( static_cast<int>( time::in_units<time::UNIT::SECOND>( CENTURY ) ) );
		HPipedChild::STATUS s( pc.finish( CENTURY_IN_SECONDS ) );
		if ( s.type != HPipedChild::STATUS::TYPE::NORMAL ) {
			cerr << "Abort " << s.value << endl;
		} else if ( s.value != 0 ) {
			cout << "Exit " << s.value << endl;
		}
	} catch ( HException const& e ) {
		ok = ( _systemCommands.count( tokens.front() ) > 0 );
		cerr << e.what() << endl;
	}
	return ( ok );
	M_EPILOG
}

bool HShell::denormalize( tokens_t& tokens_, yaal::hcore::HString& inPath_, yaal::hcore::HString& outPath_ ) {
	M_PROLOG
	inPath_.clear();
	outPath_.clear();
	bool wasSpace( true );
	resolve_aliases( tokens_ );
	bool append( false );
	for ( tokens_t::iterator it( tokens_.begin() ); it != tokens_.end(); ++ it ) {
		REDIR redir( test_redir( *it ) );
		if ( ! it->is_empty() && ( it->front() == '#' ) ) {
			tokens_.erase( it, tokens_.end() );
			break;
		}
		if ( ! wasSpace && ( wasSpace = it->is_empty() ) ) {
			tokens_.erase( it -- );
			continue;
		}
		denormalize( *it );
		if ( wasSpace && ( redir == REDIR::NONE ) ) {
			filesystem::paths_t fr( filesystem::glob( *it ) );
			tokens_.erase( it );
			tokens_.insert( it, fr.begin(), fr.end() );
			it += ( fr.get_size() - 1 );
		} else if ( ! wasSpace ) {
			HString s( yaal::move( *it ) );
			tokens_.erase( it -- );
			it->append( s );
		}
		if ( ( wasSpace = ( redir != REDIR::NONE ) ) ) {
			if ( ( tokens_.end() - it ) < 2 ) {
				throw HRuntimeException( "Missing name or redirect." );
			}
			tokens_.erase( it );
			tokens_.erase( it );
			if ( test_redir( *it ) != REDIR::NONE ) {
				throw HRuntimeException( "Missing name or redirect." );
			}
			if ( ( redir == REDIR::OUT ) || ( redir == REDIR::APP ) ) {
				if ( ! outPath_.is_empty() ) {
					throw HRuntimeException( "Ambiguous output redirect." );
				}
				append = redir == REDIR::APP;
				outPath_.assign( *it );
			} else if ( redir == REDIR::IN ) {
				if ( ! inPath_.is_empty() ) {
					throw HRuntimeException( "Ambiguous input redirect." );
				}
				inPath_.assign( *it );
			}
			tokens_.erase( it );
			-- it;
		}
	}
	return ( append );
	M_EPILOG
}

void HShell::substitute_variable( yaal::hcore::HString& token_ ) {
	M_PROLOG
	for ( HIntrospecteeInterface::HVariableView const& vv : _lineRunner.locals() ) {
		if ( vv.name() == token_ ) {
			if ( setup._shell->is_empty() ) {
				token_.assign( to_string( vv.value(), _lineRunner.huginn() ) );
			} else {
				token_.assign( "'" ).append( to_string( vv.value(), _lineRunner.huginn() ) ).append( "'" );
			}
			break;
		}
	}
	return;
	M_EPILOG
}

void HShell::resolve_aliases( tokens_t& tokens_ ) {
	M_PROLOG
	typedef yaal::hcore::HHashSet<yaal::hcore::HString> alias_hit_t;
	alias_hit_t aliasHit;
	while ( true ) {
		aliases_t::const_iterator a( _aliases.find( tokens_.front() ) );
		if ( a == _aliases.end() ) {
			break;
		}
		if ( ! aliasHit.insert( tokens_.front() ).second ) {
			break;
		}
		tokens_.erase( tokens_.begin() );
		tokens_.insert( tokens_.begin(), a->second.begin(), a->second.end() );
	}
	return;
	M_EPILOG
}

void HShell::denormalize( yaal::hcore::HString& token_ ) {
	M_PROLOG
	substitute_variable( token_ );
	QUOTES quotes( test_quotes( token_ ) );
	if ( quotes != QUOTES::SINGLE ) {
		substitute_environment( token_, ENV_SUBST_MODE::RECURSIVE );
		util::unescape( token_, executing_parser::_escapes_ );
	}
	if ( quotes != QUOTES::NONE ) {
		strip_quotes( token_ );
	}
	return;
	M_EPILOG
}

HLineRunner::words_t HShell::filename_completions( yaal::hcore::HString const& context_, yaal::hcore::HString const& prefix_ ) const {
	M_PROLOG
	static HString const SEPARATORS( "/\\" );
	tokens_t tokens( split_quotes_tilda( context_ ) );
	HString const context( ! tokens.is_empty() ? tokens.back() : "" );
	HLineRunner::words_t filesNames;
	HString prefix(
		! context.is_empty() && ( SEPARATORS.find( context.back() ) == HString::npos )
			? filesystem::basename( context )
			: ( context == "." ? "." : "" )
	);
	HString path;
	path.assign( context, 0, context.get_length() - prefix.get_length() );
	if ( path.is_empty() ) {
		path.assign( "." ).append( PATH_SEP );
	}
	int removedSepCount( static_cast<int>( prefix.get_length() - prefix_.get_length() ) );
	HFSItem dir( path );
	if ( !! dir ) {
		HString name;
		for ( HFSItem const& f : dir ) {
			name.assign( f.get_name() );
			if ( prefix.is_empty() || ( name.find( prefix ) == 0 ) ) {
				if ( removedSepCount > 0 ) {
					name.shift_left( removedSepCount );
				} else {
					name.insert( 0, prefix_, 0, - removedSepCount );
				}
				name.replace( " ", "\\ " ).replace( "\\t", "\\\\t" );
				filesNames.push_back( name + ( f.is_directory() ? PATH_SEP : ' '_ycp ) );
			}
		}
	}
	return ( filesNames );
	M_EPILOG
}

void HShell::alias( tokens_t const& tokens_ ) {
	M_PROLOG
	int argCount( static_cast<int>( tokens_.get_size() ) );
	if ( argCount == 2 ) {
		cout << left;
		for ( aliases_t::value_type const& a : _aliases ) {
			cout << setw( 8 ) << a.first;
			for ( HString const& s : a.second ) {
				cout << ( s.is_empty() ? " " : "" ) << s;
			}
			cout << endl;
		}
		cout << right;
	} else if ( argCount == 3 ) {
		aliases_t::const_iterator a( _aliases.find( tokens_.back() ) );
		cout << a->first << " ";
		if ( a != _aliases.end() ) {
			for ( HString const& s : a->second ) {
				cout << ( s.is_empty() ? " " : "" ) << s;
			}
			cout << endl;
		}
	} else {
		_aliases.insert( make_pair( tokens_[2], tokens_t( tokens_.begin() + 4, tokens_.end() ) ) );
	}
	return;
	M_EPILOG
}

void HShell::unalias( tokens_t const& tokens_ ) {
	M_PROLOG
	int argCount( static_cast<int>( tokens_.get_size() ) );
	if ( argCount < 3 ) {
		cerr << "unalias: Missing parameter!" << endl;
	}
	for ( HString const& t : tokens_ ) {
		if ( ! t.is_empty() ) {
			_aliases.erase( t );
		}
	}
	return;
	M_EPILOG
}

void HShell::cd( tokens_t const& tokens_ ) {
	M_PROLOG
	int argCount( static_cast<int>( tokens_.get_size() ) );
	if ( argCount > 3 ) {
		cerr << "cd: Too many arguments!" << endl;
		return;
	}
	if ( ( argCount == 1 ) && ! HOME_PATH ) {
		cerr << "cd: Home path not set." << endl;
		return;
	}
	HString path( argCount > 1 ? tokens_.back() : HOME_PATH );
	try {
		filesystem::chdir( path );
	} catch ( HException const& e ) {
		cerr << e.what() << endl;
	}
	return;
	M_EPILOG
}

}

