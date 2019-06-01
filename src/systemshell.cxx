/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <cstdlib>
#include <csignal>

#ifndef __MSVCXX__
#	include <unistd.h>
#endif

#include <yaal/hcore/hcore.hxx>
#include <yaal/hcore/hrawfile.hxx>
#include <yaal/hcore/bound.hxx>
#include <yaal/tools/hfsitem.hxx>
#include <yaal/tools/hhuginn.hxx>
#include <yaal/tools/signals.hxx>
#include <yaal/tools/streamtools.hxx>
#include <yaal/tools/hterminal.hxx>


M_VCSID( "$Id: " __ID__ " $" )
M_VCSID( "$Id: " __TID__ " $" )

#include "systemshell.hxx"
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
char const SHELL_AND[] = "&&";
char const SHELL_OR[] = "||";
char const SHELL_PIPE[] = "|";

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
	} else if ( token_ == SHELL_PIPE ) {
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

HSystemShell::chains_t split_chains( yaal::hcore::HString const& str_ ) {
	M_PROLOG
	tokens_t tokens( split_quotes_tilda( str_ ) );
	typedef yaal::hcore::HArray<tokens_t> chains_t;
	chains_t chains;
	chains.push_back( tokens_t() );
	bool skip( false );
	for ( HString const& t : tokens ) {
		if ( skip ) {
			skip = false;
			continue;
		}
		if ( t == ";" ) {
			if ( ! chains.back().is_empty() ) {
				chains.back().pop_back();
				chains.push_back( tokens_t() );
				skip = true;
			}
			continue;
		} else if ( ! t.is_empty() && ( t.front() == '#' ) ) {
			break;
		}
		chains.back().push_back( t );
	}
	return ( chains );
	M_EPILOG
}

}

yaal::tools::HPipedChild::STATUS HSystemShell::OCommand::finish( void ) {
	M_PROLOG
	_in.reset();
	HPipedChild::STATUS s;
	if ( !! _child ) {
		s = _child->finish( OSetup::CENTURY_IN_SECONDS );
		_child.reset();
	} else if ( !! _thread ) {
		_thread->finish();
		_thread.reset();
		s.type = HPipedChild::STATUS::TYPE::NORMAL;
	}
	HRawFile* fd( dynamic_cast<HRawFile*>( _out.raw() ) );
	if ( fd && fd->is_valid() ) {
		fd->close();
	}
	_out.reset();
	_pipe.reset();
	return ( s );
	M_EPILOG
}

HSystemShell::HSystemShell( HLineRunner& lr_ )
	: _lineRunner( lr_ )
	, _systemCommands()
	, _builtins()
	, _aliases()
	, _dirStack() {
	M_PROLOG
#ifndef __MSVCXX__
	if ( is_a_tty( STDIN_FILENO ) ) {
		int pgid( -1 );
		while ( tcgetpgrp( STDIN_FILENO ) != ( pgid = getpgrp() ) ) {
			system::kill( -pgid, SIGTTIN );
		}
		int interactiveAndJobControlSignals[] = {
			SIGINT, SIGQUIT, SIGTSTP, SIGTTIN, SIGTTOU
		};
		for ( int sigNo : interactiveAndJobControlSignals ) {
			M_ENSURE( signal( sigNo, SIG_IGN ) != SIG_ERR );
		}
		pgid = system::getpid();
		M_ENSURE( setpgid( pgid, pgid ) == 0 );
		M_ENSURE( tcsetpgrp( STDIN_FILENO, pgid ) == 0 );
	}
#endif
	_builtins.insert( make_pair( "alias", call( &HSystemShell::alias, this, _1 ) ) );
	_builtins.insert( make_pair( "cd", call( &HSystemShell::cd, this, _1 ) ) );
	_builtins.insert( make_pair( "unalias", call( &HSystemShell::unalias, this, _1 ) ) );
	_builtins.insert( make_pair( "setenv", call( &HSystemShell::setenv, this, _1 ) ) );
	_builtins.insert( make_pair( "unsetenv", call( &HSystemShell::unsetenv, this, _1 ) ) );
	learn_system_commands();
	filesystem::path_t initPath( setup._sessionDir + PATH_SEP + "shell.init" );
	HFile init( initPath, HFile::OPEN::READING );
	if ( ! init ) {
		return;
	}
	HString line;
	int lineNo( 1 );
	while ( getline( init, line ).good() ) {
		try {
			run_line( line );
		} catch ( HException const& e ) {
			cerr << initPath << ":" << lineNo << ": " << e.what() << endl;
		}
		++ lineNo;
	}
	return;
	M_EPILOG
}

void HSystemShell::learn_system_commands( void ) {
	M_PROLOG
	char const* PATH_ENV( ::getenv( "PATH" ) );
	if ( ! PATH_ENV ) {
		return;
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
	return;
	M_EPILOG
}

bool HSystemShell::do_run( yaal::hcore::HString const& line_ ) {
	M_PROLOG
	bool ok( false );
	try {
		ok = run_line( line_ );
	} catch ( HException const& e ) {
		cerr << e.what() << endl;
	}
	return ( ok );
	M_EPILOG
}

bool HSystemShell::run_line( yaal::hcore::HString const& line_ ) {
	M_PROLOG
	if ( line_.is_empty() || ( line_.front() == '#' ) ) {
		return ( true );
	}
	chains_t chains( split_chains( line_ ) );
	bool ok( false );
	for ( tokens_t& t : chains ) {
		ok = run_chain( t ) || ok;
	}
	return ( ok );
	M_EPILOG
}

bool HSystemShell::run_chain( tokens_t const& tokens_ ) {
	M_PROLOG
	tokens_t pipe;
	bool skip( false );
	bool ok( false );
	for ( HString const& t : tokens_ ) {
		if ( skip ) {
			skip = false;
			continue;
		}
		if ( ( t == SHELL_AND ) || ( t == SHELL_OR ) ) {
			if ( ! pipe.is_empty() ) {
				pipe.pop_back();
				OSpawnResult pr( run_pipe( pipe ) );
				pipe.clear();
				ok = pr._validShell;
				if ( ! ok || ( ( t == SHELL_AND ) && ( pr._exitStatus.value != 0 ) ) || ( ( t == SHELL_OR ) && ( pr._exitStatus.value == 0 ) ) ) {
					break;
				}
				skip = true;
			}
			continue;
		}
		pipe.push_back( t );
	}
	if ( ! pipe.is_empty() ) {
		OSpawnResult pr( run_pipe( pipe ) );
		ok = pr._validShell;
	}
	return ( ok );
	M_EPILOG
}

HSystemShell::OSpawnResult HSystemShell::run_pipe( tokens_t& tokens_ ) {
	M_PROLOG
	typedef yaal::hcore::HArray<OCommand> commands_t;
	commands_t commands;
	commands.push_back( OCommand{} );
	HString inPath;
	HString outPath;
	bool append( false );
	for ( tokens_t::iterator it( tokens_.begin() ); it != tokens_.end(); ++ it ) {
		REDIR redir( test_redir( *it ) );
		if ( redir == REDIR::PIPE ) {
			if ( ! outPath.is_empty() ) {
				throw HRuntimeException( "Ambiguous output redirect." );
			}
			if ( commands.back()._tokens.is_empty() ) {
				throw HRuntimeException( "Invalid null command." );
			}
			commands.back()._tokens.pop_back();
			commands.push_back( OCommand{} );
			++ it;
			continue;
		}
		if ( redir != REDIR::NONE ) {
			if ( ( tokens_.end() - it ) < 2 ) {
				throw HRuntimeException( "Missing name or redirect." );
			}
			++ it;
			++ it;
			if ( test_redir( *it ) != REDIR::NONE ) {
				throw HRuntimeException( "Missing name or redirect." );
			}
			if ( ( redir == REDIR::OUT ) || ( redir == REDIR::APP ) ) {
				if ( ! outPath.is_empty() ) {
					throw HRuntimeException( "Ambiguous output redirect." );
				}
				append = redir == REDIR::APP;
				outPath.assign( *it );
			} else if ( redir == REDIR::IN ) {
				if ( ! inPath.is_empty() || ( commands.get_size() > 1 ) ) {
					throw HRuntimeException( "Ambiguous input redirect." );
				}
				inPath.assign( *it );
			}
			commands.back()._tokens.pop_back();
			continue;
		}
		commands.back()._tokens.push_back( *it );
	}
	if ( ! inPath.is_empty() ) {
		commands.front()._in = make_pointer<HFile>( inPath, HFile::OPEN::READING );
	}
	if ( ! outPath.is_empty() ) {
		commands.back()._out = make_pointer<HFile>( outPath, append ? HFile::OPEN::WRITING | HFile::OPEN::APPEND : HFile::OPEN::WRITING );
	}
	for ( int i( 0 ), COUNT( static_cast<int>( commands.get_size() - 1 ) ); i < COUNT; ++ i ) {
		HPipe::ptr_t p( make_pointer<HPipe>() );
		commands[i]._out = p->in();
		commands[i + 1]._in = p->out();
		commands[i + 1]._pipe = p;
	}
	OSpawnResult sr;
	int leader( 0 );
	for ( OCommand& c : commands ) {
		sr._validShell = spawn( c, leader ) || sr._validShell;
		if ( ! leader && !! c._child ) {
			leader = c._child->get_pid();
		}
	}
	for ( OCommand& c : commands ) {
		sr = c.finish();
		if ( sr._exitStatus.type != HPipedChild::STATUS::TYPE::NORMAL ) {
			cerr << "Abort " << sr._exitStatus.value << endl;
		} else if ( sr._exitStatus.value != 0 ) {
			cout << "Exit " << sr._exitStatus.value << endl;
		}
	}
	return ( sr );
	M_EPILOG
}

void HSystemShell::run_huginn( void ) {
	M_PROLOG
	_lineRunner.execute();
	_lineRunner.huginn()->set_input_stream( cin );
	_lineRunner.huginn()->set_output_stream( cout );
	return;
	M_EPILOG
}

bool HSystemShell::spawn( OCommand& command_, int pgid_ ) {
	M_PROLOG
	resolve_aliases( command_._tokens );
	if ( ! is_command( command_._tokens.front() ) ) {
		if ( _lineRunner.add_line( string::join( command_._tokens, "" ) ) ) {
			command_._thread = make_pointer<HThread>();
			if ( !! command_._in ) {
				_lineRunner.huginn()->set_input_stream( command_._in );
			}
			if ( !! command_._out ) {
				_lineRunner.huginn()->set_output_stream( command_._out );
			}
			command_._thread->spawn( call( &HSystemShell::run_huginn, this ) );
			return ( true );
		} else {
			cerr << _lineRunner.err() << endl;
		}
		return ( false );
	}
	builtins_t::const_iterator builtin( _builtins.find( command_._tokens.front() ) );
	if ( builtin != _builtins.end() ) {
		command_._thread = make_pointer<HThread>();
		command_._thread->spawn( call( builtin->second, ref( command_ ) ) );
		return ( true );
	}
	bool ok( true );
	try {
		HString image;
		tokens_t tokens;
		denormalize( command_._tokens );
		if ( command_._tokens.is_empty() ) {
			return ( false );
		}
		if ( setup._shell->is_empty() ) {
			image.assign( command_._tokens.front() );
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
					image = it->second + PATH_SEP + command_._tokens.front() + e;
					if ( filesystem::exists( image ) ) {
						break;
					}
				}
#endif
			}
			command_._tokens.erase( command_._tokens.begin() );
			tokens = yaal::move( command_._tokens );
		} else {
			image.assign( *setup._shell );
			tokens.push_back( "-c" );
			tokens.push_back( join( command_._tokens, " " ) );
		}
		piped_child_t pc( make_pointer<HPipedChild>( command_._in, command_._out ) );
		command_._child = pc;
		pc->spawn(
			image,
			tokens,
			! command_._in ? &cin : nullptr,
			! command_._out ? &cout : nullptr,
			&cerr,
			pgid_,
			pgid_ == 0
		);
	} catch ( HException const& e ) {
		cerr << e.what() << endl;
		ok = false;
	}
	return ( ok );
	M_EPILOG
}

void HSystemShell::denormalize( tokens_t& tokens_ ) {
	M_PROLOG
	bool wasSpace( true );
	tokens_t exploded;
	tokens_t current;
	tokens_t tmp;
	tokens_t result;
	for ( HString const& tok : tokens_ ) {
		if ( ! wasSpace && ( wasSpace = tok.is_empty() ) ) {
			result.insert( result.end(), exploded.begin(), exploded.end() );
			exploded.clear();
			continue;
		}
		QUOTES quotes( test_quotes( tok ) );
		if ( quotes == QUOTES::NONE ) {
			current = explode( tok );
		} else {
			current.clear();
			current.push_back( tok );
		}
		for ( HString& t : current ) {
			substitute_variable( t );
		}
		if ( quotes != QUOTES::SINGLE ) {
			for ( HString& t : current ) {
				substitute_environment( t, ENV_SUBST_MODE::RECURSIVE );
				util::unescape( t, executing_parser::_escapes_ );
			}
		}
		if ( quotes != QUOTES::NONE ) {
			strip_quotes( current.front() );
		}
		if ( wasSpace ) {
			exploded = yaal::move( current );
		} else {
			if ( exploded.is_empty() ) {
				exploded = yaal::move( current );
			} else {
				for ( tokens_t::iterator explIt( exploded.begin() ); explIt != exploded.end(); ++ explIt ) {
					HString s( yaal::move( *explIt ) );
					tmp.clear();
					for ( HString const& t : current ) {
						tmp.push_back( s + t );
					}
					exploded.erase( explIt );
					exploded.insert( explIt, tmp.begin(), tmp.end() );
					explIt += ( tmp.get_size() - 1 );
				}
			}
		}
		if ( quotes == QUOTES::NONE ) {
			for ( tokens_t::iterator explIt( exploded.begin() ); explIt != exploded.end(); ++ explIt ) {
				filesystem::paths_t fr( filesystem::glob( *explIt ) );
				if ( ! fr.is_empty() ) {
					exploded.erase( explIt );
					exploded.insert( explIt, fr.begin(), fr.end() );
					explIt += ( fr.get_size() - 1 );
				}
			}
		}
		wasSpace = false;
	}
	if ( ! exploded.is_empty() ) {
		result.insert( result.end(), exploded.begin(), exploded.end() );
	}
	tokens_ = yaal::move( result );
	return;
	M_EPILOG
}

void HSystemShell::substitute_variable( yaal::hcore::HString& token_ ) {
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

void HSystemShell::resolve_aliases( tokens_t& tokens_ ) {
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

}

tokens_t HSystemShell::explode( yaal::hcore::HString const& str_ ) const {
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
		bool escaped( false );
		braces.clear();
		braces.resize( count( str_.begin(), str_.end(), '{'_ycp ), OBrace() );
		int level( -1 );
		for ( int long i( 0 ), LEN( current.get_length() ); i < LEN; ++ i ) {
			code_point_t c( current[i] );
			if ( escaped ) {
				escaped = false;
				continue;
			}
			if ( c == '\\' ) {
				escaped = true;
			} else if ( c == '{' ) {
				++ level;
				if ( ! braces[level]._completed ) {
					braces[level]._start = i;
				}
			} else if ( ( c == ',' ) && ( level >= 0 ) ) {
				braces[level]._comma = true;
			} else if ( ( c == '}' ) && ( level >= 0 ) ) {
				if ( ! braces[level]._completed && braces[level]._comma ) {
					braces[level]._end = i;
					braces[level]._completed = true;
				}
				-- level;
			}
		}
		braces_t::const_iterator it( find_if( braces.begin(), braces.end(), call( &OBrace::_completed, _1 ) == true ) );
		if ( it != braces.end() ) {
			level = 0;
			variants.clear();
			int long s( it->_start + 1 );
			for ( int long i( s ); i <= it->_end; ++ i ) {
				code_point_t c( current[i] );
				if ( escaped ) {
					escaped = false;
					continue;
				}
				if ( c == '\\' ) {
					escaped = true;
				} else if ( c == '{' ) {
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

HShell::completions_t HSystemShell::do_gen_completions( yaal::hcore::HString const& context_, yaal::hcore::HString const& prefix_ ) const {
	M_PROLOG
	completions_t completions;
	int long pfxLen( prefix_.get_length() );
	if ( ! context_.is_empty() ) {
		int long ctxLen( context_.get_length() );
		for ( system_commands_t::value_type const& sc : _systemCommands ) {
			if ( sc.first.find( context_ ) == 0 ) {
				completions.push_back( sc.first.mid( ctxLen - pfxLen ) + " " );
			}
		}
		for ( builtins_t::value_type const& b : _builtins ) {
			if ( b.first.find( context_ ) == 0 ) {
				completions.push_back( b.first.mid( ctxLen - pfxLen ) + " " );
			}
		}
		for ( aliases_t::value_type const& a : _aliases ) {
			if ( a.first.find( context_ ) == 0 ) {
				completions.push_back( a.first.mid( ctxLen - pfxLen ) + " " );
			}
		}
	}

	static HString const SEPARATORS( "/\\" );
	tokens_t tokens( split_quotes_tilda( context_ ) );
	HString const context( ! tokens.is_empty() ? tokens.back() : "" );
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
	int removedSepCount( static_cast<int>( prefix.get_length() - pfxLen ) );
	substitute_environment( path, ENV_SUBST_MODE::RECURSIVE );
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
				completions.push_back( name + ( f.is_directory() ? PATH_SEP : ' '_ycp ) );
			}
		}
	}
	return ( completions );
	M_EPILOG
}

void HSystemShell::alias( OCommand& command_ ) {
	M_PROLOG
	int argCount( static_cast<int>( command_._tokens.get_size() ) );
	if ( argCount == 1 ) {
		command_ << left;
		for ( aliases_t::value_type const& a : _aliases ) {
			command_ << setw( 8 ) << a.first;
			for ( HString const& s : a.second ) {
				command_ << ( s.is_empty() ? " " : "" ) << s;
			}
			command_ << endl;
		}
		command_ << right;
	} else if ( argCount == 3 ) {
		aliases_t::const_iterator a( _aliases.find( command_._tokens.back() ) );
		command_ << a->first << " ";
		if ( a != _aliases.end() ) {
			for ( HString const& s : a->second ) {
				command_ << ( s.is_empty() ? " " : "" ) << s;
			}
			command_ << endl;
		}
	} else {
		_aliases[command_._tokens[2]] = tokens_t( command_._tokens.begin() + 4, command_._tokens.end() );
	}
	return;
	M_EPILOG
}

void HSystemShell::unalias( OCommand& command_ ) {
	M_PROLOG
	int argCount( static_cast<int>( command_._tokens.get_size() ) );
	if ( argCount < 3 ) {
		cerr << "unalias: Missing parameter!" << endl;
	}
	for ( HString const& t : command_._tokens ) {
		if ( ! t.is_empty() ) {
			_aliases.erase( t );
		}
	}
	return;
	M_EPILOG
}

void HSystemShell::cd( OCommand& command_ ) {
	M_PROLOG
	int argCount( static_cast<int>( command_._tokens.get_size() ) );
	if ( argCount > 3 ) {
		cerr << "cd: Too many arguments!" << endl;
		return;
	}
	if ( ( argCount == 1 ) && ! HOME_PATH ) {
		cerr << "cd: Home path not set." << endl;
		return;
	}
	HString path( argCount > 1 ? command_._tokens.back() : HOME_PATH );
	int dirStackSize( static_cast<int>( _dirStack.get_size() ) );
	if ( ( path == "-" ) && ( dirStackSize > 1 ) ) {
		path = _dirStack[dirStackSize - 2];
	}
	if (
		( path.get_length() > 1 )
		&& ( path.front() == '=' )
		&& is_digit( path[1] )
		&& ( path.find_other_than( character_class<CHARACTER_CLASS::DIGIT>().data(), 1 ) == HString::npos )
	) {
		int idx( lexical_cast<int>( path.substr( 1 ) ) );
		if ( idx < dirStackSize ) {
			path = _dirStack[( dirStackSize - idx ) - 1];
		}
	}
	try {
		path.trim_right( "/\\" );
		filesystem::chdir( path );
		set_env( "PWD", path, true );
		dir_stack_t::iterator it( find( _dirStack.begin(), _dirStack.end(), path ) );
		if  ( it != _dirStack.end() ) {
			_dirStack.erase( it );
		}
		_dirStack.push_back( path );
	} catch ( HException const& e ) {
		cerr << e.what() << endl;
	}
	return;
	M_EPILOG
}

void HSystemShell::setenv( OCommand& command_ ) {
	M_PROLOG
	int argCount( static_cast<int>( command_._tokens.get_size() ) );
	if ( argCount < 3 ) {
		cerr << "setenv: Missing parameter!" << endl;
	} else if ( argCount > 5 ) {
		cerr << "setenv: Too many parameters!" << endl;
	} else {
		set_env( command_._tokens[2], argCount > 4 ? command_._tokens[4] : HString() );
	}
	return;
	M_EPILOG
}

void HSystemShell::unsetenv( OCommand& command_ ) {
	M_PROLOG
	int argCount( static_cast<int>( command_._tokens.get_size() ) );
	if ( argCount < 3 ) {
		cerr << "unsetenv: Missing parameter!" << endl;
	}
	for ( HString const& t : command_._tokens ) {
		if ( ! t.is_empty() ) {
			unset_env( t );
		}
	}
	return;
	M_EPILOG
}

bool HSystemShell::is_command( yaal::hcore::HString const& str_ ) const {
	M_PROLOG
	bool isCommand( false );
	tokens_t exploded( explode( str_ ) );
	if ( ! ( exploded.is_empty() || exploded.front().is_empty() ) ) {
		HString& cmd( exploded.front() );
		cmd.trim();
		HFSItem path( cmd );
		isCommand = ( _aliases.count( cmd ) > 0 ) || ( _builtins.count( cmd ) > 0 ) || ( _systemCommands.count( cmd ) > 0 ) || ( !! path && path.is_executable() );
	}
	return ( isCommand );
	M_EPILOG
}

bool HSystemShell::do_try_command( yaal::hcore::HString const& str_ ) {
	M_PROLOG
	return ( do_is_valid_command( str_ ) );
	M_EPILOG
}

bool HSystemShell::do_is_valid_command( yaal::hcore::HString const& str_ ) const {
	M_PROLOG
	chains_t chains( split_chains( str_ ) );
	tokens_t exploded;
	for ( tokens_t const& tokens : chains ) {
		if ( tokens.is_empty() ) {
			continue;
		}
		bool head( true );
		for ( HString const& token : tokens ) {
			if ( head ) {
				exploded = explode( token );
				if ( exploded.is_empty() || exploded.front().is_empty() ) {
					continue;
				}
				HString& cmd( exploded.front() );
				cmd.trim();
				HFSItem path( cmd );
				if ( ( _aliases.count( cmd ) > 0 ) || ( _builtins.count( cmd ) > 0 ) || ( _systemCommands.count( cmd ) > 0 ) || ( !! path && path.is_executable() ) ) {
					return ( true );
				}
			}
			head = ( token == SHELL_AND ) || ( token == SHELL_OR ) || ( token == SHELL_PIPE );
		}
	}
	return ( false );
	M_EPILOG
}

}

