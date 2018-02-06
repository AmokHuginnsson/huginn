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

tokens_t split_quotes_tilda( yaal::hcore::HString const& str_ ) {
	M_PROLOG
	tokens_t tokens( split_quotes( str_ ) );
	for ( HString& t : tokens ) {
		if ( HOME_PATH && ( t.front() == '~' ) ) {
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
	HStreamInterface* in( &cin );
	HStreamInterface* out( &cout );
	HFile fileIn;
	HFile fileOut;
	bool ok( false );
	try {
		resolve_aliases( tokens );
		for ( tokens_t::iterator it( tokens.begin() ); it != tokens.end(); ++ it ) {
			bool append( *it == ">>" );
			bool redirOut( *it == ">" );
			bool redirIn( *it == "<" );
			if ( ! it->is_empty() && ( it->front() == '#' ) ) {
				tokens.erase( it, tokens.end() );
				break;
			}
			denormailze( *it );
			if ( redirIn || redirOut || append ) {
				tokens.erase( it );
				if ( it != tokens.end() ) {
					if ( redirOut || append ) {
						if ( fileOut.is_opened() ) {
							fileOut.close();
						}
						fileOut.open( *it, append ? HFile::OPEN::WRITING | HFile::OPEN::APPEND : HFile::OPEN::WRITING );
						out = &fileOut;
					} else {
						if ( fileIn.is_opened() ) {
							fileIn.close();
						}
						fileIn.open( *it, HFile::OPEN::READING );
						in = &fileIn;
					}
					tokens.erase( it );
				} else {
					ok = ( _systemCommands.count( tokens.front() ) > 0 );
					throw HRuntimeException( "Missing name or redirect." );
				}
				-- it;
				continue;
			}
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
		cerr << e.what() << endl;
	}
	return ( ok );
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

void HShell::denormailze( yaal::hcore::HString& token_ ) {
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
	if ( argCount == 1 ) {
		cout << left;
		for ( aliases_t::value_type const& a : _aliases ) {
			cout << setw( 8 ) << a.first << " " << join( a.second, " " ) << endl;
		}
		cout << right;
	} else if ( argCount == 2 ) {
		aliases_t::const_iterator a( _aliases.find( tokens_.back() ) );
		if ( a != _aliases.end() ) {
			cout << a->first << " " << join( a->second, " " ) << endl;
		}
	} else {
		_aliases.insert( make_pair( tokens_[1], tokens_t( tokens_.begin() + 2, tokens_.end() ) ) );
	}
	return;
	M_EPILOG
}

void HShell::unalias( tokens_t const& tokens_ ) {
	M_PROLOG
	int argCount( static_cast<int>( tokens_.get_size() ) );
	if ( argCount < 2 ) {
		cerr << "unalias: Missing parameter!" << endl;
	}
	for ( HString const& t : tokens_ ) {
		_aliases.erase( t );
	}
	return;
	M_EPILOG
}

void HShell::cd( tokens_t const& tokens_ ) {
	M_PROLOG
	int argCount( static_cast<int>( tokens_.get_size() ) );
	if ( argCount > 2 ) {
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

