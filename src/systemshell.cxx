/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <cstdlib>
#include <csignal>

#ifndef __MSVCXX__
#	include <unistd.h>
#endif

#include <yaal/hcore/hcore.hxx>
#include <yaal/hcore/hrawfile.hxx>
#include <yaal/hcore/system.hxx>
#include <yaal/tools/hfsitem.hxx>
#include <yaal/tools/hhuginn.hxx>
#include <yaal/tools/signals.hxx>
#include <yaal/tools/streamtools.hxx>
#include <yaal/tools/hterminal.hxx>
#include <yaal/tools/huginn/value.hxx>
#include <yaal/tools/huginn/list.hxx>
#include <yaal/tools/huginn/helper.hxx>

M_VCSID( "$Id: " __ID__ " $" )
M_VCSID( "$Id: " __TID__ " $" )

#include "systemshell.hxx"
#include "quotes.hxx"
#include "colorize.hxx"
#include "setup.hxx"

#ifndef __MSVCXX__

#ifndef HAVE_SIGHANDLER_T
#ifdef HAVE___SIGHANDLER_T
typedef __sighandler_t* sighandler_t;
#elif defined ( HAVE_SIG_PF )
typedef SIG_PF sighandler_t;
#elif defined ( __HOST_OS_TYPE_DARWIN__ )
typedef void (*sighandler_t)( int );
#else /* #elif defined ( __HOST_OS_TYPE_DARWIN__ ) */
#error No signal handler type definition available.
#endif /* #else #elif defined ( __HOST_OS_TYPE_DARWIN__ ) */
#endif /* #ifndef HAVE_SIGHANDLER_T */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
static sighandler_t const FWD_SIG_ERR = SIG_ERR;
static sighandler_t const FWD_SIG_IGN = SIG_IGN;
#pragma GCC diagnostic pop

#endif

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;
using namespace yaal::tools::util;
using namespace yaal::tools::string;

namespace huginn {

void denormalize_path( filesystem::path_t& path_ ) {
	if ( ! path_.is_empty() && ( path_.front() == '~' ) ) {
		path_.replace( 0, 1, system::home_path() );
	}
}

namespace {

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

tokens_t tokenize_shell_tilda( yaal::hcore::HString const& str_ ) {
	M_PROLOG
	tokens_t tokens( tokenize_shell( str_ ) );
	for ( HString& t : tokens ) {
		denormalize_path( t );
	}
	return ( tokens );
	M_EPILOG
}

HSystemShell::chains_t split_chains( yaal::hcore::HString const& str_ ) {
	M_PROLOG
	tokens_t tokens( tokenize_shell_tilda( str_ ) );
	typedef yaal::hcore::HArray<tokens_t> chains_t;
	chains_t chains;
	chains.push_back( tokens_t() );
	for ( HString const& t : tokens ) {
		if ( t == ";" ) {
			if ( ! chains.back().is_empty() ) {
				chains.push_back( tokens_t() );
			}
			continue;
		} else if ( ! t.is_empty() && ( t.front() == '#' ) ) {
			break;
		}
		chains.back().push_back( t );
	}
	while ( ! chains.is_empty() && chains.back().is_empty() ) {
		chains.pop_back();
	}
	return ( chains );
	M_EPILOG
}

void unescape_huginn_command( HSystemShell::OCommand& command_ ) {
	M_PROLOG
	for ( yaal::hcore::HString& s : command_._tokens ) {
		s = unescape_huginn_code( s );
	}
	return;
	M_EPILOG
}

void unescape_shell_command( tokens_t& tokens_ ) {
	M_PROLOG
	for ( yaal::hcore::HString& s : tokens_ ) {
		util::unescape( s, executing_parser::_escapes_ );
	}
	return;
	M_EPILOG
}

yaal::hcore::HString stringify_command( HSystemShell::tokens_t const& tokens_, int skip_ = 0 ) {
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

HStreamInterface::ptr_t const& ensure_valid( HStreamInterface::ptr_t const& stream_ ) {
	if ( ! stream_->is_valid() ) {
		throw HRuntimeException( static_cast<HFile const*>( stream_.raw() )->get_error() );
	}
	return ( stream_ );
}

void capture_output( HStreamInterface::ptr_t stream_, HString& out_ ) {
	HChunk c;
	int long totalRead( 0 );
	try {
		int long toRead( system::get_page_size() );
		while ( true ) {
			c.realloc( totalRead + toRead, HChunk::STRATEGY::EXACT );
			int long nRead( stream_->read( c.get<char>() + totalRead, toRead ) );
			if ( nRead <= 0 ) {
				break;
			}
			totalRead += nRead;
			if ( nRead == toRead ) {
				toRead *= 2;
			}
		}
	} catch ( ... ) {
		/* Ignore all exceptions cause we are in the thread. */
	}
	out_.assign( c.get<char>(), totalRead );
	return;
}

char const* status_type_to_str( HPipedChild::STATUS const& status_ ) {
	char const* s( nullptr );
	switch ( status_.type ) {
		case ( HPipedChild::STATUS::TYPE::UNSPAWNED ): s = "Unspawned"; break;
		case ( HPipedChild::STATUS::TYPE::FINISHED  ): s = "Finished "; break;
		case ( HPipedChild::STATUS::TYPE::PAUSED    ): s = "Suspended"; break;
		case ( HPipedChild::STATUS::TYPE::RUNNING   ): s = "Running  "; break;
		case ( HPipedChild::STATUS::TYPE::ABORTED   ): s = "Aborted  "; break;
	}
	return ( s );
}

}

void HSystemShell::OCommand::run_builtin( builtin_t const& builtin_ ) {
	M_PROLOG
	try {
		builtin_( *this );
		_status.type = HPipedChild::STATUS::TYPE::FINISHED;
		_status.value = 0;
	} catch ( HException const& e ) {
		cerr << e.what() << endl;
		_status.type = HPipedChild::STATUS::TYPE::FINISHED;
		_status.value = 1;
	}
	return;
	M_EPILOG
}

void HSystemShell::OCommand::run_huginn( HLineRunner& lineRunner_ ) {
	M_PROLOG
	try {
		lineRunner_.execute();
		lineRunner_.huginn()->set_input_stream( cin );
		lineRunner_.huginn()->set_output_stream( cout );
		lineRunner_.huginn()->set_error_stream( cerr );
		_status.type = HPipedChild::STATUS::TYPE::FINISHED;
		_status.value = 0;
	} catch ( HException const& e ) {
		cerr << e.what() << endl;
		_status.type = HPipedChild::STATUS::TYPE::FINISHED;
		_status.value = 1;
	}
	return;
	M_EPILOG
}

yaal::tools::HPipedChild::STATUS HSystemShell::OCommand::finish( void ) {
	M_PROLOG
	_in.reset();
	HPipedChild::STATUS s;
	if ( !! _child ) {
		s = _child->wait();
		if ( s.type == HPipedChild::STATUS::TYPE::PAUSED ) {
			_child->restore_parent_term();
			return ( s );
		}
		_child.reset();
	} else if ( !! _thread ) {
		_thread->finish();
		_thread.reset();
		s = _status;
	} else {
		s.type = HPipedChild::STATUS::TYPE::FINISHED;
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

HSystemShell::HJob::HJob( HSystemShell& systemShell_, commands_t&& commands_, EVALUATION_MODE evaluationMode_ )
	: _systemShell( systemShell_ )
	, _description( make_desc( commands_ ) )
	, _commands( yaal::move( commands_ ) )
	, _leader( HPipedChild::PROCESS_GROUP_LEADER )
	, _evaluationMode( evaluationMode_ )
	, _capturePipe()
	, _captureThread()
	, _captureBuffer()
	, _sec( call( &HJob::stop_capture, this ) ) {
	if ( _evaluationMode == EVALUATION_MODE::COMMAND_SUBSTITUTION ) {
		_capturePipe = make_resource<HPipe>();
		_captureThread = make_resource<HThread>();
		_commands.back()._out = _capturePipe->in();
		_captureThread->spawn( call( &capture_output, _capturePipe->out(), ref( _captureBuffer ) ) );
	}
	return;
}

yaal::hcore::HString HSystemShell::HJob::make_desc( commands_t const& commands_ ) const {
	M_PROLOG
	HString desc;
	for ( OCommand const& c : commands_ ) {
		if ( ! desc.is_empty() ) {
			desc.append( " | " );
		}
		desc.append( stringify_command( c._tokens ) );
	}
	return ( desc );
	M_EPILOG
}

bool HSystemShell::HJob::start( void ) {
	M_PROLOG
	bool validShell( false );
	for ( OCommand& c : _commands ) {
		validShell = _systemShell.spawn( c, _leader, &c == &_commands.back(), _evaluationMode ) || validShell;
		if ( ( _leader == HPipedChild::PROCESS_GROUP_LEADER ) && !! c._child ) {
			_leader = c._child->get_pid();
		}
	}
	return ( validShell );
	M_EPILOG
}

HPipedChild::STATUS HSystemShell::HJob::wait_for_finish( void ) {
	M_PROLOG
	bool captureHuginn( !! _commands.back()._thread );
	HPipedChild::STATUS exitStatus;
	for ( OCommand& c : _commands ) {
		exitStatus = c.finish();
		if ( exitStatus.type == HPipedChild::STATUS::TYPE::PAUSED ) {
			cerr << "Suspended " << exitStatus.value << endl;
		} else if ( exitStatus.type != HPipedChild::STATUS::TYPE::FINISHED ) {
			cerr << "Abort " << exitStatus.value << endl;
		} else if ( exitStatus.value != 0 ) {
			cout << "Exit " << exitStatus.value << endl;
		}
	}
	if ( _evaluationMode == EVALUATION_MODE::COMMAND_SUBSTITUTION ) {
		_captureThread->finish();
		if ( captureHuginn ) {
			_captureBuffer.append( to_string( _systemShell._lineRunner.huginn()->result(), _systemShell._lineRunner.huginn() ) );
		}
	}
	return ( exitStatus );
	M_EPILOG
}

yaal::tools::HPipedChild::STATUS const& HSystemShell::HJob::status( void ) {
	M_PROLOG
	return ( _commands.back()._child->get_status() );
	M_EPILOG
}

bool HSystemShell::HJob::is_system_command( void ) const {
	return ( ! _commands.is_empty() && !! _commands.back()._child );
}

void HSystemShell::HJob::do_continue( void ) {
	system::kill( -_leader, SIGCONT );
	return ( _commands.back()._child->do_continue() );
}

void HSystemShell::HJob::bring_to_foreground( void ) {
	return ( _commands.back()._child->bring_to_foreground() );
}

void HSystemShell::HJob::stop_capture( void ) {
	M_PROLOG
	if ( !! _capturePipe && _capturePipe->in()->is_valid() ) {
		const_cast<HRawFile*>( static_cast<HRawFile const*>( _capturePipe->in().raw() ) )->close();
	}
	return;
	M_EPILOG
}

HSystemShell::HSystemShell( HLineRunner& lr_, HRepl& repl_ )
	: _lineRunner( lr_ )
	, _repl( repl_ )
	, _systemCommands()
	, _builtins()
	, _aliases()
	, _keyBindings()
	, _setoptHandlers()
	, _dirStack()
	, _substitutions()
	, _ignoredFiles( "^.*~$" )
	, _loaded( false )
	, _jobs() {
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
			M_ENSURE( signal( sigNo, FWD_SIG_IGN ) != FWD_SIG_ERR );
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
	_builtins.insert( make_pair( "setopt", call( &HSystemShell::setopt, this, _1 ) ) );
	_builtins.insert( make_pair( "unsetenv", call( &HSystemShell::unsetenv, this, _1 ) ) );
	_builtins.insert( make_pair( "bindkey", call( &HSystemShell::bind_key, this, _1 ) ) );
	_builtins.insert( make_pair( "dirs", call( &HSystemShell::dir_stack, this, _1 ) ) );
	_builtins.insert( make_pair( "rehash", call( &HSystemShell::rehash, this, _1 ) ) );
	_builtins.insert( make_pair( "history", call( &HSystemShell::history, this, _1 ) ) );
	_builtins.insert( make_pair( "jobs", call( &HSystemShell::jobs, this, _1 ) ) );
	_builtins.insert( make_pair( "bg", call( &HSystemShell::bg, this, _1 ) ) );
	_builtins.insert( make_pair( "fg", call( &HSystemShell::fg, this, _1 ) ) );
	_setoptHandlers.insert( make_pair( "ignore_filenames", &HSystemShell::setopt_ignore_filenames ) );
	learn_system_commands();
	load_init();
	char const* PWD( getenv( "PWD" ) );
	filesystem::path_t cwd( PWD ? PWD : filesystem::current_working_directory() );
	if ( ! PWD ) {
		set_env( "PWD", cwd );
	}
	_dirStack.push_back( cwd );
	_loaded = true;
	return;
	M_EPILOG
}

void HSystemShell::cleanup_jobs( void ) {
	M_PROLOG
	for ( jobs_t::iterator it( _jobs.begin() ); it != _jobs.end(); ) {
		if ( ! (*it)->is_system_command() ) {
			++ it;
			continue;
		}
		HPipedChild::STATUS const& status( (*it)->status() );
		if ( ( status.type != HPipedChild::STATUS::TYPE::RUNNING ) && ( status.type != HPipedChild::STATUS::TYPE::PAUSED ) ) {
			it = _jobs.erase( it );
		} else {
			++ it;
		}
	}
	return;
	M_EPILOG
}

void HSystemShell::load_init( void ) {
	M_PROLOG
	char const* HUGINN_INIT_SHELL( getenv( "HUGINN_INIT_SHELL" ) );
	filesystem::path_t initPath( HUGINN_INIT_SHELL ? HUGINN_INIT_SHELL : setup._sessionDir + PATH_SEP + "init.shell" );
	HFile init( initPath, HFile::OPEN::READING );
	if ( ! init ) {
		return;
	}
	HString line;
	int lineNo( 1 );
	while ( getline( init, line ).good() ) {
		try {
			run_line( line, EVALUATION_MODE::DIRECT );
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
		ok = run_line( line_, EVALUATION_MODE::DIRECT );
	} catch ( HException const& e ) {
		cerr << e.what() << endl;
	}
	return ( ok );
	M_EPILOG
}

void HSystemShell::run_bound( yaal::hcore::HString const& line_ ) {
	M_PROLOG
	run( line_ );
	return;
	M_EPILOG
}

bool HSystemShell::run_line( yaal::hcore::HString const& line_, EVALUATION_MODE evaluationMode_ ) {
	M_PROLOG
	HString line( line_ );
	line.trim_left();
	if ( line.is_empty() || ( line.front() == '#' ) ) {
		return ( true );
	}
	chains_t chains( split_chains( line ) );
	bool ok( false );
	for ( tokens_t& t : chains ) {
		ok = run_chain( t, evaluationMode_ ) || ok;
	}
	cleanup_jobs();
	return ( ok );
	M_EPILOG
}

bool HSystemShell::run_chain( tokens_t const& tokens_, EVALUATION_MODE evaluationMode_ ) {
	M_PROLOG
	tokens_t pipe;
	for ( HString const& t : tokens_ ) {
		if ( ( t != SHELL_AND ) && ( t != SHELL_OR ) ) {
			pipe.push_back( t );
			continue;
		}
		if ( pipe.is_empty() ) {
			throw HRuntimeException( "Invalid null command." );
		}
		OSpawnResult pr( run_pipe( pipe, evaluationMode_ ) );
		pipe.clear();
		if ( ! pr._validShell ) {
			return ( false );
		}
		if ( ( t == SHELL_AND ) && ( pr._exitStatus.value != 0 ) ) {
			return ( false );
		}
		if ( ( t == SHELL_OR ) && ( pr._exitStatus.value == 0 ) ) {
			return ( true );
		}
	}
	if ( pipe.is_empty() ) {
		throw HRuntimeException( "Invalid null command." );
	}
	OSpawnResult pr( run_pipe( pipe, evaluationMode_ ) );
	return ( pr._validShell );
	M_EPILOG
}

HSystemShell::OSpawnResult HSystemShell::run_pipe( tokens_t& tokens_, EVALUATION_MODE evaluationMode_ ) {
	M_PROLOG
	commands_t commands;
	commands.push_back( OCommand{} );
	HString inPath;
	HString outPath;
	HString errPath;
	bool appendOut( false );
	bool appendErr( false );
	bool joinErr( false );
	bool moveErr( false );
	REDIR previousRedir( REDIR::NONE );
	for ( tokens_t::iterator it( tokens_.begin() ); it != tokens_.end(); ++ it ) {
		REDIR redir( str_to_redir( *it ) );
		if ( ( redir == REDIR::PIPE ) || ( redir == REDIR::PIPE_ERR ) ) {
			if ( ! ( outPath.is_empty() || moveErr ) ) {
				throw HRuntimeException( "Ambiguous output redirect." );
			}
			if ( commands.back()._tokens.is_empty() || ( ( tokens_.end() - it ) < 2 ) ) {
				throw HRuntimeException( "Invalid null command." );
			}
			if ( ! errPath.is_empty() && ( redir == REDIR::PIPE_ERR ) ) {
				throw HRuntimeException( "Ambiguous error redirect." );
			}
			if ( ( redir == REDIR::PIPE_ERR ) && moveErr ) {
				throw HRuntimeException( "Error stream already redirected." );
			}
			OCommand& previous( commands.back() );
			M_ASSERT( ! previous._out || moveErr );
			if ( ! outPath.is_empty() ) {
				previous._out = make_pointer<HFile>( outPath, appendOut ? HFile::OPEN::WRITING | HFile::OPEN::APPEND : HFile::OPEN::WRITING );
				outPath.clear();
			}
			if ( ! errPath.is_empty() ) {
				previous._err = make_pointer<HFile>( errPath, appendErr ? HFile::OPEN::WRITING | HFile::OPEN::APPEND : HFile::OPEN::WRITING );
				errPath.clear();
			}
			HPipe::ptr_t p( make_pointer<HPipe>() );
			if ( moveErr ) {
				M_ASSERT( ! previous._err );
				previous._err = p->in();
			} else if ( redir == REDIR::PIPE_ERR ) {
				previous._out = p->in();
				previous._err = p->in();
			} else {
				M_ASSERT( ! previous._out );
				previous._out = p->in();
			}
			commands.push_back( OCommand{} );
			OCommand& next( commands.back() );
			next._in = p->out();
			next._pipe = p;
			previousRedir = redir;
			moveErr = false;
			continue;
		}
		if ( redir == REDIR::ERR_OUT ) {
			if ( moveErr || ! errPath.is_empty() ) {
				throw HRuntimeException( "Error stream already moved." );
			}
			if ( outPath.is_empty() ) {
				throw HRuntimeException( "Output stream is not redirected, use >& or |& to combine error and output streams." );
			}
			moveErr = true;
			continue;
		} else if ( redir != REDIR::NONE ) {
			if ( previousRedir != REDIR::NONE ) {
				throw HRuntimeException( "Invalid null command." );
			}
			++ it;
			if ( it == tokens_.end() ) {
				throw HRuntimeException( "Missing name or redirect." );
			}
			if ( str_to_redir( *it ) != REDIR::NONE ) {
				throw HRuntimeException( "Missing name or redirect." );
			}
			if ( ( redir == REDIR::OUT ) || ( redir == REDIR::OUT_ERR ) || ( redir == REDIR::APP_OUT ) || ( redir == REDIR::APP_OUT_ERR ) ) {
				appendOut = ( redir == REDIR::APP_OUT ) || ( redir == REDIR::APP_OUT_ERR );
				joinErr = ( redir == REDIR::OUT_ERR ) || ( redir == REDIR::APP_OUT_ERR );
				if ( moveErr ) {
					throw HRuntimeException( "Output stream is already redirected, use >& to combine error and output streams." );
				}
				if ( ! outPath.is_empty() || ( joinErr && ! errPath.is_empty() ) ) {
					throw HRuntimeException( "Ambiguous output redirect." );
				}
				outPath.assign( *it );
				util::unescape( outPath, executing_parser::_escapes_ );
			} else if ( ( redir == REDIR::ERR ) || ( redir == REDIR::APP_ERR ) ) {
				if ( ! errPath.is_empty() || joinErr ) {
					throw HRuntimeException( "Ambiguous error redirect." );
				}
				appendErr = redir == REDIR::APP_ERR;
				errPath.assign( *it );
				util::unescape( errPath, executing_parser::_escapes_ );
			} else if ( redir == REDIR::IN ) {
				if ( ! inPath.is_empty() || ( commands.get_size() > 1 ) ) {
					throw HRuntimeException( "Ambiguous input redirect." );
				}
				inPath.assign( *it );
				util::unescape( inPath, executing_parser::_escapes_ );
			}
			previousRedir = REDIR::NONE;
			continue;
		}
		commands.back()._tokens.push_back( *it );
		previousRedir = redir;
	}
	if ( ! inPath.is_empty() ) {
		commands.front()._in = ensure_valid( make_pointer<HFile>( inPath, HFile::OPEN::READING ) );
	}
	if ( ! outPath.is_empty() ) {
		commands.back()._out = ensure_valid( make_pointer<HFile>( outPath, appendOut ? HFile::OPEN::WRITING | HFile::OPEN::APPEND : HFile::OPEN::WRITING ) );
	}
	if ( ! errPath.is_empty() ) {
		commands.back()._err = ensure_valid( make_pointer<HFile>( errPath, appendErr ? HFile::OPEN::WRITING | HFile::OPEN::APPEND : HFile::OPEN::WRITING ) );
	} else if ( joinErr ) {
		commands.back()._err = commands.back()._out;
	}
	job_t job( make_resource<HJob>( *this, yaal::move( commands ), evaluationMode_ ) );
	_jobs.emplace_back( yaal::move( job ) );
	bool validShell( _jobs.back()->start() );
	OSpawnResult sr( _jobs.back()->wait_for_finish(), validShell );
	if ( evaluationMode_ == EVALUATION_MODE::COMMAND_SUBSTITUTION ) {
		_substitutions.top().append( _jobs.back()->output() );
	}
	if ( sr._exitStatus.type != HPipedChild::STATUS::TYPE::PAUSED ) {
		_jobs.pop_back();
	}
	return ( sr );
	M_EPILOG
}

bool HSystemShell::spawn( OCommand& command_, int pgid_, bool foreground_, EVALUATION_MODE evaluationMode_ ) {
	M_PROLOG
	resolve_aliases( command_._tokens );
	tokens_t tokens( denormalize( command_._tokens, evaluationMode_ ) );
	if ( ! is_command( tokens.front() ) ) {
		unescape_huginn_command( command_ );
		HString line( string::join( command_._tokens, " " ) );
		if ( _lineRunner.add_line( line, _loaded ) ) {
			command_._thread = make_pointer<HThread>();
			if ( !! command_._in ) {
				_lineRunner.huginn()->set_input_stream( command_._in );
			}
			if ( !! command_._out ) {
				_lineRunner.huginn()->set_output_stream( command_._out );
			}
			if ( !! command_._err ) {
				_lineRunner.huginn()->set_error_stream( command_._err );
			}
			command_._thread->spawn( call( &OCommand::run_huginn, &command_, ref( _lineRunner ) ) );
			return ( true );
		} else {
			cerr << _lineRunner.err() << endl;
		}
		return ( false );
	}
	builtins_t::const_iterator builtin( _builtins.find( tokens.front() ) );
	if ( builtin != _builtins.end() ) {
		command_._thread = make_pointer<HThread>();
		command_._thread->spawn( call( &OCommand::run_builtin, &command_, builtin->second ) );
		return ( true );
	}
	bool ok( true );
	try {
		unescape_shell_command( tokens );
		HString image;
		if ( tokens.is_empty() ) {
			return ( false );
		}
		if ( setup._shell->is_empty() ) {
			image.assign( tokens.front() );
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
		} else {
			image.assign( *setup._shell );
			tokens.clear();
			tokens.push_back( "-c" );
			tokens.push_back( join( command_._tokens, " " ) );
		}
		piped_child_t pc( make_pointer<HPipedChild>( command_._in, command_._out, command_._err ) );
		command_._child = pc;
		pc->spawn(
			image,
			tokens,
			! command_._in ? &cin : nullptr,
			! command_._out ? &cout : nullptr,
			! command_._err ? &cerr : nullptr,
			pgid_,
			foreground_
		);
	} catch ( HException const& e ) {
		cerr << e.what() << endl;
		ok = false;
	}
	return ( ok );
	M_EPILOG
}

tokens_t HSystemShell::interpolate( yaal::hcore::HString const& token_, EVALUATION_MODE evaluationMode_ ) {
	tokens_t exploded( brace_expansion( token_ ) );
	tokens_t interpolated;
	HString param;
	for ( yaal::hcore::HString const& word : exploded ) {
		/*
		 * Each token can have one of the folloing forms:
		 *
		 * abc
		 * "abc"
		 * "abc def"
		 * 'abc'
		 * 'abc def'
		 * ${abc}
		 * $(abc)
		 * $(abc def)
		 * "${abc}"
		 * "$(abc)"
		 * "$(abc def)"
		 * "${abc}def"
		 * "$(abc)def"
		 * "$(abc def)ghi"
		 * "${abc}'def'"
		 * "$(abc)'def'"
		 * "$(abc def)'ghi'"
		 *
		 * or any concatenation of those.
		 */
		tokens_t tokens( tokenize_quotes( word ) );
		param.clear();
		for ( yaal::hcore::HString& token : tokens ) {
			QUOTES quotes( str_to_quotes( token ) );
			if ( quotes != QUOTES::NONE ) {
				strip_quotes( token );
			}
			if ( ( evaluationMode_ == EVALUATION_MODE::DIRECT ) && ( quotes == QUOTES::EXEC ) ) {
				_substitutions.emplace();
				run_line( token, EVALUATION_MODE::COMMAND_SUBSTITUTION );
				token = _substitutions.top();
				_substitutions.pop();
				token.trim();
			}
			if ( quotes != QUOTES::SINGLE ) {
				substitute_environment( token, ENV_SUBST_MODE::RECURSIVE );
			}
			if ( ( evaluationMode_ == EVALUATION_MODE::DIRECT ) && ( quotes == QUOTES::DOUBLE ) ) {
				bool escaped( false );
				bool execStart( false );
				bool inExecQuotes( false );
				HString tmp( yaal::move( token ) );
				HString subst;
				for ( code_point_t c : tmp ) {
					if ( escaped ) {
						( inExecQuotes ? subst : token ).push_back( c );
						escaped = false;
						continue;
					}
					if ( execStart ) {
						inExecQuotes = c == '(';
						execStart = false;
						continue;
					}
					if ( inExecQuotes && ( c == ')' ) ) {
						_substitutions.emplace();
						run_line( subst, EVALUATION_MODE::COMMAND_SUBSTITUTION );
						subst = _substitutions.top();
						_substitutions.pop();
						subst.trim();
						token.append( subst );
						subst.clear();
						inExecQuotes = false;
						continue;
					}
					if ( c == '\\' ) {
						( inExecQuotes ? subst : token ).push_back( c );
						escaped = true;
						continue;
					}
					if ( c == '$' ) {
						execStart = true;
						continue;
					}
					( inExecQuotes ? subst : token ).push_back( c );
				}
			}
			if ( ( quotes != QUOTES::NONE ) && ( quotes != QUOTES::EXEC ) ) {
				param.append( token );
				continue;
			}
			substitute_variable( token );
			tokens_t words( string::split( token, character_class<CHARACTER_CLASS::WHITESPACE>().data(), HTokenizer::DELIMITED_BY_ANY_OF | HTokenizer::SKIP_EMPTY ) );
			if ( words.get_size() > 1 ) {
				param.append( words.front() );
				interpolated.push_back( param );
				interpolated.insert( interpolated.end(), words.begin() + 1, words.end() - 1 );
				param.assign( words.back() );
			} else {
				param.append( token );
			}
		}
		filesystem::paths_t fr( filesystem::glob( param ) );
		if ( ! fr.is_empty() ) {
			interpolated.insert( interpolated.end(), fr.begin(), fr.end() );
		} else {
			interpolated.push_back( param );
		}
	}
	return ( interpolated );
}

tokens_t HSystemShell::denormalize( tokens_t const& tokens_, EVALUATION_MODE evaluationMode_ ) {
	M_PROLOG
	tokens_t tmp;
	tokens_t result;
	for ( HString const& tok : tokens_ ) {
		tmp = interpolate( tok, evaluationMode_ );
		result.insert( result.end(), tmp.begin(), tmp.end() );
	}
	return ( result );
	M_EPILOG
}

void HSystemShell::substitute_variable( yaal::hcore::HString& token_ ) const {
	M_PROLOG
	for ( HIntrospecteeInterface::HVariableView const& vv : _lineRunner.locals() ) {
		if ( ! vv.value() ) {
			continue;
		}
		if ( vv.name() != token_ ) {
			continue;
		}
		if ( setup._shell->is_empty() ) {
			token_.assign( to_string( vv.value(), _lineRunner.huginn() ) );
		} else {
			token_.assign( "'" ).append( to_string( vv.value(), _lineRunner.huginn() ) ).append( "'" );
		}
		break;
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
extern "C" {
extern char** environ;
}
int complete_environment_variable( HRepl::completions_t& completions_, yaal::hcore::HString const& prefix_ ) {
	int added( 0 );
	for ( char** e( environ ); *e; ++ e ) {
		HString envVar( *e );
		HString::size_type eqPos( envVar.find( '='_ycp ) );
		if ( eqPos != HString::npos ) {
			envVar.erase( eqPos );
		}
		if ( envVar.starts_with( prefix_ ) ) {
			completions_.emplace_back( envVar, COLOR::FG_CYAN );
			++ added;
		}
	}
	return ( added );
}
}

bool HSystemShell::fallback_completions( tokens_t const& tokens_, completions_t& completions_ ) const {
	M_PROLOG
	HString context( ( tokens_.get_size() == 1 ) ? tokens_.front() : "" );
	HString prefix( ! tokens_.is_empty() ? tokens_.back() : HString() );
	if ( prefix.starts_with( "${" ) ) {
		HString varName( prefix.substr( 2 ) );
		int added( complete_environment_variable( completions_, varName ) );
		if ( added == 1 ) {
			completions_.back() = HRepl::HCompletion( completions_.back().text() + "}" );
		}
		if ( added > 0 ) {
			return ( true );
		}
	}
	if ( ! context.is_empty() ) {
		for ( system_commands_t::value_type const& sc : _systemCommands ) {
			if ( sc.first.starts_with( context ) ) {
				completions_.emplace_back( sc.first + " ", COLOR::FG_BRIGHTGREEN );
			}
		}
		for ( builtins_t::value_type const& b : _builtins ) {
			if ( b.first.starts_with( context ) ) {
				completions_.emplace_back( b.first + " ", COLOR::FG_RED );
			}
		}
		for ( aliases_t::value_type const& a : _aliases ) {
			if ( a.first.starts_with( context ) ) {
				completions_.emplace_back( a.first + " ", COLOR::FG_BRIGHTCYAN );
			}
		}
	}
	return( false );
	M_EPILOG
}

void HSystemShell::filename_completions( tokens_t const& tokens_, yaal::hcore::HString const& prefix_, FILENAME_COMPLETIONS filenameCompletions_, completions_t& completions_, bool maybeExec_ ) const {
	M_PROLOG
	static HString const SEPARATORS( "/\\" );
	bool wantExec( tokens_.is_empty() || ( ( tokens_.get_size() == 1 ) && maybeExec_ ) );
	HString context( ! tokens_.is_empty() ? tokens_.back() : "" );
	HString prefix(
		! context.is_empty()
		&& ! prefix_.is_empty()
		&& ( SEPARATORS.find( context.back() ) == HString::npos )
			? filesystem::basename( context )
			: ( context == "." ? "." : "" )
	);
	HString path;
	context.erase( context.get_length() - prefix.get_length() );
	if ( filesystem::exists( context ) ) {
		path.assign( context );
	}
	if ( path.is_empty() ) {
		path.assign( "." ).append( PATH_SEP );
	}
	substitute_environment( path, ENV_SUBST_MODE::RECURSIVE );
	HFSItem dir( path );
	if ( ! dir.is_directory() ) {
		return;
	}
	HString name;
	completions_t completions;
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
		name.replace( " ", "\\ " ).replace( "\\t", "\\\\t" );
		completions.emplace_back( name + ( f.is_directory() ? PATH_SEP : ' '_ycp ), file_color( path + name, this ) );
		if ( ignoredThis ) {
			++ ignored;
		}
	}
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
	completions_.insert( completions_.end(), completions.begin(), completions.end() );
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
		for ( HHuginn::value_t const& v : data ) {
			if ( v->type_id() != HHuginn::TYPE::STRING ) {
				continue;
			}
			completions_.push_back( tools::huginn::get_string( v ) );
		}
	} else if ( t == HHuginn::TYPE::STRING ) {
		HString completionAction( tools::huginn::get_string( userCompletions_ ) );
		completionAction.lower();
		if ( ( completionAction == "directories" ) || ( completionAction == "dirs" ) ) {
			filename_completions( tokens_, prefix_, FILENAME_COMPLETIONS::DIRECTORY, completions_ );
		} else if ( completionAction == "files" ) {
			filename_completions( tokens_, prefix_, FILENAME_COMPLETIONS::FILE, completions_ );
		} else if ( completionAction == "executables" ) {
			filename_completions( tokens_, prefix_, FILENAME_COMPLETIONS::EXECUTABLE, completions_ );
		} else if ( completionAction == "commands" ) {
			for ( system_commands_t::value_type const& sc : _systemCommands ) {
				if ( sc.first.starts_with( prefix_ ) ) {
					completions_.push_back( sc.first );
				}
			}
		} else if ( completionAction == "aliases" ) {
			for ( aliases_t::value_type const& a : _aliases ) {
				if ( a.first.starts_with( prefix_ ) ) {
					completions_.push_back( a.first );
				}
			}
		} else if (
			( completionAction == "environmentvariables" )
			|| ( completionAction == "environment_variables" )
			|| ( completionAction == "envvars" )
			|| ( completionAction == "environment" )
		) {
			complete_environment_variable( completions_, prefix_ );
		}
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

HShell::completions_t HSystemShell::do_gen_completions( yaal::hcore::HString const& context_, yaal::hcore::HString const& prefix_ ) const {
	M_PROLOG
	chains_t chains( split_chains( context_ ) );
	tokens_t tokens( ! chains.is_empty() ? chains.back() : tokens_t() );
	for ( tokens_t::iterator it( tokens.begin() ); it != tokens.end(); ) {
		if ( ( *it == SHELL_AND ) || ( *it == SHELL_OR ) || ( *it == SHELL_PIPE ) || ( *it == SHELL_PIPE_ERR ) ) {
			++ it;
			it = tokens.erase( tokens.begin(), it );
		} else {
			++ it;
		}
	}
	tokens.erase( remove( tokens.begin(), tokens.end(), "" ), tokens.end() );
	bool endsWithWhitespace( ! context_.is_empty() && character_class<CHARACTER_CLASS::WHITESPACE>().has( context_.back() ) );
	if ( endsWithWhitespace ) {
		tokens.push_back( "" );
	}
	bool isPrefix( ! tokens.is_empty() && is_prefix( tokens.front() ) );
	HHuginn::value_t userCompletions(
		! ( tokens.is_empty() || ( ( tokens.get_size() == 1 ) && isPrefix && ! endsWithWhitespace ) )
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
		if ( ! fallback_completions( tokens, completions ) ) {
			filename_completions(
				tokens,
				prefix_,
				FILENAME_COMPLETIONS::FILE,
				completions,
				! endsWithWhitespace
			);
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
		HUTF8String utf8;
		tokens_t tokens;
		aliases_t::const_iterator longestAliasNameIt(
			max_element(
				_aliases.begin(),
				_aliases.end(),
				[]( aliases_t::value_type const& left_, aliases_t::value_type const& right_ ) {
					return ( left_.first.get_length() < right_.first.get_length() );
				}
			)
		);
		int longestAliasNameLength( ! _aliases.is_empty() ? static_cast<int>( longestAliasNameIt->first.get_length() ) : 0 );
		for ( aliases_t::value_type const& a : _aliases ) {
			command_ << setw( longestAliasNameLength + 2 ) << a.first;
			command_ << colorize( stringify_command( a.second ), this ) << endl;
		}
		command_ << right;
	} else if ( argCount == 2 ) {
		aliases_t::const_iterator a( _aliases.find( command_._tokens.back() ) );
		command_ << a->first << " ";
		if ( a != _aliases.end() ) {
			command_ << colorize( stringify_command( a->second ), this ) << endl;
		}
	} else {
		_aliases[command_._tokens[1]] = tokens_t( command_._tokens.begin() + 2, command_._tokens.end() );
	}
	return;
	M_EPILOG
}

void HSystemShell::unalias( OCommand& command_ ) {
	M_PROLOG
	int argCount( static_cast<int>( command_._tokens.get_size() ) );
	if ( argCount < 2 ) {
		throw HRuntimeException( "unalias: Missing parameter!" );
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
	if ( argCount > 2 ) {
		throw HRuntimeException( "cd: Too many arguments!" );
	}
	HString hp( system::home_path() );
	if ( ( argCount == 1 ) && hp.is_empty() ) {
		throw HRuntimeException( "cd: Home path not set." );
	}
	HString path( argCount > 1 ? stringify_command( interpolate( command_._tokens.back(), EVALUATION_MODE::DIRECT ) ) : hp );
	int dirStackSize( static_cast<int>( _dirStack.get_size() ) );
	if ( ( path == "-" ) && ( dirStackSize > 1 ) ) {
		path = _dirStack[dirStackSize - 2];
	} else if (
		( path.get_length() > 1 )
		&& ( path.front() == '=' )
		&& is_digit( path[1] )
		&& ( path.find_other_than( character_class<CHARACTER_CLASS::DIGIT>().data(), 1 ) == HString::npos )
	) {
		int idx( lexical_cast<int>( path.substr( 1 ) ) );
		if ( idx < dirStackSize ) {
			path = _dirStack[( dirStackSize - idx ) - 1];
		}
	} else if ( argCount > 1 ) {
		substitute_variable( path );
	}
	path.trim_right( "/\\" );
	HString pwdReal( filesystem::current_working_directory() );
	HString pwd( pwdReal );
	char const* PWD( ::getenv( "PWD" ) );
	if ( PWD ) {
		try {
			HString pwdEnv( filesystem::normalize_path( PWD ) );
			if ( filesystem::is_symbolic_link( pwdEnv ) ) {
				pwdEnv = filesystem::readlink( pwdEnv );
			}
			u64_t idReal( HFSItem( pwdReal ).id() );
			u64_t idEnv( HFSItem( pwdEnv ).id() );
			if ( idEnv == idReal ) {
				pwd.assign( PWD );
			}
		} catch ( ... ) {
		}
	}
	filesystem::chdir( path );
	if ( ! ( path.is_empty() || filesystem::is_absolute( path ) ) ) {
		pwd.append( filesystem::path::SEPARATOR ).append( path );
	} else {
		pwd.assign( path );
	}
	pwd = filesystem::normalize_path( pwd );
	set_env( "PWD", pwd, true );
	dir_stack_t::iterator it( find( _dirStack.begin(), _dirStack.end(), pwd ) );
	if  ( it != _dirStack.end() ) {
		_dirStack.erase( it );
	}
	_dirStack.push_back( pwd );
	return;
	M_EPILOG
}

void HSystemShell::dir_stack( OCommand& command_ ) {
	M_PROLOG
	int argCount( static_cast<int>( command_._tokens.get_size() ) );
	if ( argCount > 1 ) {
		throw HRuntimeException( "dirs: Too many parameters!" );
	}
	int index( 0 );
	for ( dir_stack_t::value_type const& dir : reversed( _dirStack ) ) {
		cout << index << " " << compact_path( dir ) << endl;
		++ index;
	}
	return;
	M_EPILOG
}

void HSystemShell::setenv( OCommand& command_ ) {
	M_PROLOG
	int argCount( static_cast<int>( command_._tokens.get_size() ) );
	if ( argCount < 2 ) {
		throw HRuntimeException( "setenv: Missing parameter!" );
	} else if ( argCount > 3 ) {
		throw HRuntimeException( "setenv: Too many parameters!" );
	} else {
		HString& val( command_._tokens[2] );
		substitute_variable( val );
		set_env( command_._tokens[1], argCount > 2 ? val : HString() );
	}
	return;
	M_EPILOG
}

void HSystemShell::setopt( OCommand& command_ ) {
	M_PROLOG
	int argCount( static_cast<int>( command_._tokens.get_size() ) );
	if ( argCount < 2 ) {
		throw HRuntimeException( "setopt: Missing parameter!" );
	} else {
		HString const& optName( command_._tokens[1] );
		setopt_handlers_t::const_iterator it( _setoptHandlers.find( optName ) );
		if ( it != _setoptHandlers.end() ) {
			command_._tokens.erase( command_._tokens.begin(), command_._tokens.begin() + 2 );
			( this->*(it->second) )( command_._tokens );
		} else {
			throw HRuntimeException( "setopt: unknown option: "_ys.append( optName ).append( "!" ) );
		}
	}
	return;
	M_EPILOG
}

void HSystemShell::setopt_ignore_filenames( tokens_t& values_ ) {
	M_PROLOG
	HString pattern;
	for ( HString& optStrVal : values_ ) {
		substitute_variable( optStrVal );
		if ( optStrVal.is_empty() ) {
			continue;
		}
		if ( ! pattern.is_empty() ) {
			pattern.append( '|' );
		}
		pattern.append( filesystem::glob_to_re( optStrVal ) );
	}
	_ignoredFiles.clear();
	if ( ! pattern.is_empty() ) {
		_ignoredFiles.compile( pattern );
	}
	return;
	M_EPILOG
}

void HSystemShell::unsetenv( OCommand& command_ ) {
	M_PROLOG
	int argCount( static_cast<int>( command_._tokens.get_size() ) );
	if ( argCount < 2 ) {
		throw HRuntimeException( "unsetenv: Missing parameter!" );
	}
	for ( HString const& t : command_._tokens ) {
		if ( ! t.is_empty() ) {
			unset_env( t );
		}
	}
	return;
	M_EPILOG
}

void HSystemShell::bind_key( OCommand& command_ ) {
	M_PROLOG
	int argCount( static_cast<int>( command_._tokens.get_size() ) );
	if ( argCount == 1 ) {
		command_ << left;
		HUTF8String utf8;
		tokens_t tokens;
		key_bindings_t::const_iterator longestKeyNameIt(
			max_element(
				_keyBindings.begin(),
				_keyBindings.end(),
				[]( key_bindings_t::value_type const& left_, key_bindings_t::value_type const& right_ ) {
					return ( left_.first.get_length() < right_.first.get_length() );
				}
			)
		);
		int longestKeyNameLength( ! _keyBindings.is_empty() ? static_cast<int>( longestKeyNameIt->first.get_length() ) : 0 );
		for ( key_bindings_t::value_type const& kb : _keyBindings ) {
			command_ << setw( longestKeyNameLength + 2 ) << kb.first;
			command_ << colorize( kb.second, this ) << endl;
		}
		command_ << right;
	} else if ( argCount == 2 ) {
		key_bindings_t::const_iterator kb( _keyBindings.find( command_._tokens.back() ) );
		if ( kb != _keyBindings.end() ) {
			command_ << command_._tokens.back() << " " << colorize( kb->second, this ) << endl;
		} else {
			throw HRuntimeException( "bindkey: Unbound key: `"_ys.append( command_._tokens.back() ).append( "`" ) );
		}
	} else {
		HString command( stringify_command( command_._tokens, 2 ) );
		HString const& keyName( command_._tokens[1] );
		if ( _repl.bind_key( keyName, call( &HSystemShell::run_bound, this, command ) ) ) {
			_keyBindings[keyName] = command;
		} else {
			throw HRuntimeException( "bindkey: invalid key name: "_ys.append( keyName ) );
		}
	}
	return;
	M_EPILOG
}

void HSystemShell::rehash( OCommand& command_ ) {
	M_PROLOG
	int argCount( static_cast<int>( command_._tokens.get_size() ) );
	if ( argCount > 1 ) {
		throw HRuntimeException( "rehash: Superfluous parameter!" );
	}
	_systemCommands.clear();
	learn_system_commands();
	return;
	M_EPILOG
}

void HSystemShell::history( OCommand& ) {
	M_PROLOG
	return;
	M_EPILOG
}

void HSystemShell::jobs( OCommand& command_ ) {
	M_PROLOG
	int argCount( static_cast<int>( command_._tokens.get_size() ) );
	if ( argCount > 1 ) {
		throw HRuntimeException( "jobs: Superfluous parameter!" );
	}
	int no( 1 );
	for ( job_t& job : _jobs ) {
		if ( ! job->is_system_command() ) {
			continue;
		}
		cout << "[" << no << "] " << status_type_to_str( job->status() ) << " " << colorize( job->desciption(), this ) << endl;
		++ no;
	}
	return;
	M_EPILOG
}

int HSystemShell::get_job_no( char const* cmdName_, OCommand& command_ ) {
	M_PROLOG
	int argCount( static_cast<int>( command_._tokens.get_size() ) );
	if ( argCount > 2 ) {
		throw HRuntimeException( to_string( cmdName_ ).append( ": Superfluous parameter! " ).append( command_._tokens.back() ) );
	}
	int jobNo( 0 );
	if ( argCount > 1 ) {
		jobNo = lexical_cast<int>( command_._tokens.back() ) - 1;
		if ( ( jobNo < 0 ) || ( jobNo >= static_cast<int>( _jobs.get_size() ) ) ) {
			throw HRuntimeException( to_string( cmdName_ ).append( ": Invalid job number! " ).append( command_._tokens.back() ) );
		}
	} else {
		int no( 0 );
		for ( job_t& job : _jobs ) {
			++ no;
			if ( ! job->is_system_command() ) {
				continue;
			}
			if ( job->status().type != HPipedChild::STATUS::TYPE::PAUSED ) {
				continue;
			}
			jobNo = no - 1;
		}
	}
	return ( jobNo );
	M_EPILOG
}

void HSystemShell::bg( OCommand& command_ ) {
	M_PROLOG
	job_t& job( _jobs[get_job_no( "bg", command_ )] );
	if ( job->is_system_command() && ( job->status().type == HPipedChild::STATUS::TYPE::PAUSED ) ) {
		job->do_continue();
	}
	return;
	M_EPILOG
}

void HSystemShell::fg( OCommand& command_ ) {
	M_PROLOG
	job_t& job( _jobs[get_job_no( "fg", command_ )] );
	if ( job->is_system_command() && ( job->status().type == HPipedChild::STATUS::TYPE::PAUSED ) ) {
		job->bring_to_foreground();
		job->do_continue();
		job->wait_for_finish();
	}
	return;
	M_EPILOG
}

bool HSystemShell::is_command( yaal::hcore::HString const& str_ ) {
	M_PROLOG
	bool isCommand( false );
	tokens_t exploded( brace_expansion( str_ ) );
	if ( ! ( exploded.is_empty() || exploded.front().is_empty() ) ) {
		HString& cmd( exploded.front() );
		cmd.trim();
		HFSItem path( cmd );
		isCommand =
			( _aliases.count( cmd ) > 0 )
			|| ( _builtins.count( cmd ) > 0 )
			|| ( _systemCommands.count( cmd ) > 0 )
			|| ( ( cmd.find( '/'_ycp ) != HString::npos ) && !! path && path.is_executable() );
	}
	return ( isCommand );
	M_EPILOG
}

bool HSystemShell::do_try_command( yaal::hcore::HString const& str_ ) {
	M_PROLOG
	return ( do_is_valid_command( str_ ) );
	M_EPILOG
}

bool HSystemShell::do_is_valid_command( yaal::hcore::HString const& str_ ) {
	M_PROLOG
	chains_t chains( split_chains( str_ ) );
	for ( tokens_t& tokens : chains ) {
		if ( tokens.is_empty() ) {
			continue;
		}
		bool head( true );
		try {
			tokens = denormalize( tokens, EVALUATION_MODE::TRIAL );
		} catch ( HException const& ) {
		}
		for ( HString& token : tokens ) {
			if ( head ) {
				token.trim();
				if ( token.is_empty() ) {
					continue;
				}
				HFSItem path( token );
				if ( ( _aliases.count( token ) > 0 ) || ( _builtins.count( token ) > 0 ) || ( _systemCommands.count( token ) > 0 ) || ( !! path && path.is_executable() ) ) {
					return ( true );
				}
			}
			head = ( token == SHELL_AND ) || ( token == SHELL_OR ) || ( token == SHELL_PIPE ) || ( token == SHELL_PIPE_ERR );
		}
	}
	return ( false );
	M_EPILOG
}

HSystemShell::system_commands_t const& HSystemShell::system_commands( void ) const {
	return ( _systemCommands );
}

HSystemShell::aliases_t const& HSystemShell::aliases( void ) const {
	return ( _aliases );
}

HSystemShell::builtins_t const& HSystemShell::builtins( void ) const {
	return ( _builtins );
}

}

