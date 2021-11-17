/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <cstdlib>
#include <csignal>

#ifndef __MSVCXX__
#	include <unistd.h>
#endif

#include <yaal/hcore/hcore.hxx>
#include <yaal/hcore/hlog.hxx>
#include <yaal/tools/hfsitem.hxx>
#include <yaal/tools/signals.hxx>
#include <yaal/tools/hterminal.hxx>
#include <yaal/tools/huginn/helper.hxx>

M_VCSID( "$Id: " __ID__ " $" )
M_VCSID( "$Id: " __TID__ " $" )

#include "systemshell.hxx"
#include "shell/command.hxx"
#include "shell/job.hxx"
#include "quotes.hxx"
#include "braceexpansion.hxx"
#include "colorize.hxx"
#include "settings.hxx"
#include "setup.hxx"
#include "shell/capture.hxx"
#include "shell/util.hxx"

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
static sighandler_t const FWD_SIG_DFL = SIG_DFL;
#pragma GCC diagnostic pop

#endif

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;
using namespace yaal::tools::util;
using namespace yaal::tools::string;

namespace huginn {

namespace {

HStreamInterface::ptr_t const& ensure_valid( HStreamInterface::ptr_t const& stream_ ) {
	if ( ! stream_->is_valid() ) {
		throw HRuntimeException( static_cast<HFile const*>( stream_.raw() )->get_error() );
	}
	return stream_;
}

}

HSystemShell::HSystemShell( HLineRunner& lr_, HRepl& repl_, int argc_, char** argv_ )
	: _lineRunner( lr_ )
	, _repl( repl_ )
	, _systemCommands()
	, _systemSuperUserCommands()
	, _builtins()
	, _aliases()
	, _keyBindings()
	, _setoptHandlers()
	, _superUserPaths()
	, _dirStack()
	, _prefixCommands()
	, _ignoredFiles( "^.*~$" )
	, _tracePrompt( "+ " )
	, _jobs()
	, _activelySourced()
	, _activelySourcedStack()
	, _failureMessages()
	, _previousOwner( -1 )
	, _trace( false )
	, _background( false )
	, _loaded( false )
	, _argvs()
	, _mutex( HMutex::TYPE::RECURSIVE ) {
	M_PROLOG
	if ( argc_ > 0 ) {
		_argvs.emplace( argv_, argv_ + argc_ );
	}
	attach_terminal();
	session_start();
	load_init();
	register_commands();
	learn_system_commands();
	set_environment();
	HString historyPath( DEFAULT::HISTORY_PATH );
	substitute_environment( historyPath, ENV_SUBST_MODE::RECURSIVE );
	if ( setup._historyPath.is_empty() || ( setup._historyPath == historyPath ) ) {
		setup._historyPath.assign( "${" HOME_ENV_VAR "}/.hgnsh_history" );
		substitute_environment( setup._historyPath, ENV_SUBST_MODE::RECURSIVE );
	}
	_loaded = true;
	return;
	M_EPILOG
}

void HSystemShell::attach_terminal( void ) {
	M_PROLOG
	if ( ! setup._interactive ) {
		return;
	}
#ifndef __MSVCXX__
	if ( ! is_a_tty( STDIN_FILENO ) ) {
		return;
	}
	int pgid( -1 );
	while ( true ) {
		int owner( tcgetpgrp( STDIN_FILENO ) );
		if ( _previousOwner == -1 ) {
			_previousOwner = owner;
		}
		if ( system::kill( -owner, 0 ) == -1 ) {
			_background = true;
			break;
		}
		pgid = getpgrp();
		if ( ( owner == -1 ) && ( errno == ENOTTY ) ) {
			_background = true;
			break;
		}
		if ( owner == pgid ) {
			break;
		}
		system::kill( -pgid, SIGTTIN );
	}
#endif
	return;
	M_EPILOG
}

void HSystemShell::register_commands( void ) {
	M_PROLOG
	_builtins.insert( make_pair( "alias",    &HSystemShell::alias       ) );
	_builtins.insert( make_pair( "bg",       &HSystemShell::bg          ) );
	_builtins.insert( make_pair( "bindkey",  &HSystemShell::bind_key    ) );
	_builtins.insert( make_pair( "call",     &HSystemShell::call_huginn ) );
	_builtins.insert( make_pair( "cd",       &HSystemShell::cd          ) );
	_builtins.insert( make_pair( "dirs",     &HSystemShell::dir_stack   ) );
	_builtins.insert( make_pair( "eval",     &HSystemShell::eval        ) );
	_builtins.insert( make_pair( "exec",     &HSystemShell::exec        ) );
	_builtins.insert( make_pair( "exit",     &HSystemShell::exit        ) );
	_builtins.insert( make_pair( "fg",       &HSystemShell::fg          ) );
	_builtins.insert( make_pair( "help",     &HSystemShell::help        ) );
	_builtins.insert( make_pair( "history",  &HSystemShell::history     ) );
	_builtins.insert( make_pair( "jobs",     &HSystemShell::jobs        ) );
	_builtins.insert( make_pair( "rehash",   &HSystemShell::rehash      ) );
	_builtins.insert( make_pair( "setenv",   &HSystemShell::setenv      ) );
	_builtins.insert( make_pair( "setopt",   &HSystemShell::setopt      ) );
	_builtins.insert( make_pair( "source",   &HSystemShell::source      ) );
	_builtins.insert( make_pair( "unalias",  &HSystemShell::unalias     ) );
	_builtins.insert( make_pair( "unsetenv", &HSystemShell::unsetenv    ) );
	_setoptHandlers.insert( make_pair( "ignore_filenames", &HSystemShell::setopt_ignore_filenames ) );
	_setoptHandlers.insert( make_pair( "history_path",     &HSystemShell::setopt_history_path ) );
	_setoptHandlers.insert( make_pair( "history_max_size", &HSystemShell::setopt_history_max_size ) );
	_setoptHandlers.insert( make_pair( "super_user_paths", &HSystemShell::setopt_super_user_paths ) );
	_setoptHandlers.insert( make_pair( "trace",            &HSystemShell::setopt_trace ) );
	_setoptHandlers.insert( make_pair( "prefix_commands",  &HSystemShell::setopt_prefix_commands ) );
	_setoptHandlers.insert( make_pair( "--print",          &HSystemShell::setopt_print ) );
	HHuginn& h( *_lineRunner.huginn() );
	tools::huginn::register_function( h, "shell_run", call( &HSystemShell::run_result, this, _1 ), "( *commandStr* ) - run shell command expressed by *commandStr*" );
	tools::huginn::register_function( h, "shell_has", call( &HSystemShell::has_command, this, _1 ), "( *commandName* ) - tells if the shell knows about existance of a command given by the *commandName* on the system" );
	return;
	M_EPILOG
}

void HSystemShell::load_init( void ) {
	M_PROLOG
	if ( ! setup._interactive ) {
		return;
	}
	if ( setup._noDefaultInit ) {
		return;
	}
	filesystem::path_t initPath( make_conf_path( "init.shell" ) );
	hcore::log << "Loading `init.shell` from `" << initPath << "`." << endl;
	_lineRunner.load_session( initPath, false );
	_lineRunner.call( "init_shell", {}, &cerr );
	return;
	M_EPILOG
}

void HSystemShell::load_rc( void ) {
	M_PROLOG
	if ( ! setup._interactive ) {
		return;
	}
	if ( setup._noDefaultInit ) {
		return;
	}
	source_global( "rc.shell" );
	if ( setup._chomp ) {
		source_global( "login" );
	}
	return;
	M_EPILOG
}

void HSystemShell::set_environment( void ) {
	M_PROLOG
#ifdef __MSVCXX__
	char const DEV_NULL[] = "NUL";
#else
	char const DEV_NULL[] = "/dev/null";
#endif
	set_env( "OSTYPE", HOST_OS_TYPE );
	set_env( "DEV_NULL", DEV_NULL );
	char const SHELL_VAR_NAME[] = "SHELL";
	char const* SHELL( ::getenv( SHELL_VAR_NAME ) );
	HString shellName( SHELL ? SHELL : HString() );
	if ( ( shellName.find( "hgnsh" ) == HString::npos ) && ( shellName.find( "huginn" ) == HString::npos ) ) {
		set_env( SHELL_VAR_NAME, setup._programName + ( setup._programName[0] != '-' ? 0 : 1 ) );
	}
	char const USER[] = "USER";
#ifdef __MSVCXX__
	char const* HOMEPATH( ::getenv( "HOMEPATH" ) );
	char const HOME[] = "HOME";
	if ( HOMEPATH && ! ::getenv( HOME ) ) {
		set_env( HOME, system::home_path().replace( "\\", "/" ) );
	}
	char const* USERNAME( ::getenv( "USERNAME" ) );
	if ( USERNAME && ! ::getenv( USER ) ) {
		set_env( USER, USERNAME );
	}
#endif
	if ( ! ::getenv( USER ) ) {
		set_env( USER, system::get_user_name( system::get_user_id() ) );
	}
	load_rc();
	char const* PWD( getenv( "PWD" ) );
	filesystem::path_t cwd( PWD ? PWD : filesystem::current_working_directory() );
	if ( ! PWD ) {
		set_env( "PWD", cwd );
	}
	_dirStack.push_back( cwd );
	return;
	M_EPILOG
}

void HSystemShell::session_start( void ) {
	M_PROLOG
	if ( ! setup._interactive ) {
		return;
	}
#ifndef __MSVCXX__
	if ( is_a_tty( STDOUT_FILENO ) ) {
		_repl.enable_bracketed_paste();
	}
	bool inIsTTY( is_a_tty( STDIN_FILENO ) );
	bool outIsTTY( is_a_tty( STDOUT_FILENO ) );
	bool errIsTTY( is_a_tty( STDERR_FILENO ) );
	if ( inIsTTY || outIsTTY || errIsTTY ) {
		int interactiveAndJobControlSignals[] = {
			SIGINT, SIGQUIT, SIGTSTP, SIGTTIN, SIGTTOU
		};
		for ( int sigNo : interactiveAndJobControlSignals ) {
			M_ENSURE( signal( sigNo, FWD_SIG_IGN ) != FWD_SIG_ERR );
		}
		int pgid( system::getpid() );
		M_ENSURE( ( getsid( pgid ) == pgid ) || ( setpgid( pgid, pgid ) == 0 ) );
		if ( ! _background ) {
			M_ENSURE( tcsetpgrp( inIsTTY ? STDIN_FILENO : ( outIsTTY ? STDOUT_FILENO : STDERR_FILENO ), pgid ) == 0 );
		}
	}
#endif
	char const HGNLVL_VAR_NAME[] = "HGNLVL";
	char const* HGNLVL( ::getenv( HGNLVL_VAR_NAME ) );
	int hgnLvl( 0 );
	try {
		hgnLvl = HGNLVL ? ( lexical_cast<int>( HGNLVL ) + 1 ) : 0;
	} catch ( ... ) {
	}
	set_env( HGNLVL_VAR_NAME, to_string( hgnLvl ) );
	return;
	M_EPILOG
}

void HSystemShell::session_stop( void ) {
	M_PROLOG
	if ( ! setup._interactive ) {
		return;
	}
#ifndef __MSVCXX__
	bool inIsTTY( is_a_tty( STDIN_FILENO ) );
	bool outIsTTY( is_a_tty( STDOUT_FILENO ) );
	bool errIsTTY( is_a_tty( STDERR_FILENO ) );
	if ( inIsTTY || outIsTTY || errIsTTY ) {
		if ( _previousOwner >= 0 ) {
			tcsetpgrp( inIsTTY ? STDIN_FILENO : ( outIsTTY ? STDOUT_FILENO : STDERR_FILENO ), _previousOwner );
		}
		int interactiveAndJobControlSignals[] = {
			SIGINT, SIGQUIT, SIGTSTP, SIGTTIN, SIGTTOU
		};
		for ( int sigNo : interactiveAndJobControlSignals ) {
			M_ENSURE( signal( sigNo, FWD_SIG_DFL ) != FWD_SIG_ERR );
		}
	}
	if ( is_a_tty( STDOUT_FILENO ) ) {
		_repl.disable_bracketed_paste();
	}
#endif
	char const HGNLVL_VAR_NAME[] = "HGNLVL";
	char const* HGNLVL( ::getenv( HGNLVL_VAR_NAME ) );
	int hgnLvl( 0 );
	try {
		hgnLvl = HGNLVL ? ( lexical_cast<int>( HGNLVL ) - 1 ) : -1;
	} catch ( ... ) {
	}
	if ( hgnLvl >= 0 ) {
		set_env( HGNLVL_VAR_NAME, to_string( hgnLvl ) );
	} else {
		unset_env( HGNLVL_VAR_NAME );
	}
	return;
	M_EPILOG
}

HSystemShell::~HSystemShell( void ) {
	if ( ! setup._program && setup._chomp ) {
		try {
			source_global( "logout" );
		} catch ( ... ) {
		}
	}
	session_stop();
}


void HSystemShell::cleanup_jobs( void ) {
	M_PROLOG
	HLock l( _mutex );
	int no( 1 );
	for ( jobs_t::iterator it( _jobs.begin() ); it != _jobs.end(); ) {
		job_t& job( *it );
		if ( ! job->is_direct_evaluation() ) {
			++ it;
			continue;
		}
		HPipedChild::STATUS const& status( job->status() );
		if ( ( status.type != HPipedChild::STATUS::TYPE::RUNNING ) && ( status.type != HPipedChild::STATUS::TYPE::PAUSED ) ) {
			flush_faliures( job );
			cerr << "[" << no << "] Done " << colorize( job->description(), this ) << endl;
			it = _jobs.erase( it );
		} else {
			++ it;
		}
		++ no;
	}
	return;
	M_EPILOG
}

bool HSystemShell::finalized( void ) {
	M_PROLOG
	HLock l( _mutex );
	cleanup_jobs();
	if ( has_huginn_jobs() ) {
		cerr << "Huginn jobs present!" << endl;
		return ( false );
	}
	bool canQuit( true );
	for ( job_t& j : _jobs ) {
		if ( ! j->can_orphan() ) {
			canQuit = false;
			break;
		}
	}
	if ( canQuit ) {
		for ( job_t& j : _jobs ) {
			j->orphan();
		}
		_jobs.clear();
	}
	jobs_t::size_type jobCount( _jobs.get_size() );
	if ( jobCount > 1 ) {
		cerr << "There are " << jobCount << " jobs still running!" << endl;
	} else if ( jobCount > 0 ) {
		cerr << "There is 1 job still running!" << endl;
	}
	return ( jobCount == 0 );
	M_EPILOG
}

void HSystemShell::source_global( char const* name_ ) {
	M_PROLOG
	try {
		filesystem::path_t initPath( make_conf_path( name_ ) );
		hcore::log << "Loading `" << name_ << "` from `" << initPath << "`." << endl;
		do_source( tokens_t( { filesystem::normalize_path( initPath ) } ) );
	} catch ( HException const& ) {
	}
	return;
	M_EPILOG
}

void HSystemShell::do_source( tokens_t const& argv_ ) {
	M_PROLOG
	HLock l( _mutex );
	filesystem::path_t const& path( argv_.front() );
	static char const HGNSH_SOURCE[] = "HGNSH_SOURCE";
	set_env( HGNSH_SOURCE, path );
	_argvs.push( argv_ );
	HScopeExitCall sec(
		HScopeExitCall::call_t(
			[this, path]() {
				if ( ! _activelySourcedStack.is_empty() ) {
					set_env( HGNSH_SOURCE, _activelySourcedStack.top() );
				} else {
					unset_env( HGNSH_SOURCE );
				}
				_argvs.pop();
			}
		)
	);
	HFile shellScript( path, HFile::OPEN::READING );
	if ( ! shellScript ) {
		throw HRuntimeException( shellScript.get_error() );
	}
	run_script( shellScript, path );
	return;
	M_EPILOG
}

int HSystemShell::do_run_script( yaal::hcore::HStreamInterface& shellScript_, yaal::hcore::HString const& path_ ) {
	M_PROLOG
	if ( ! _activelySourced.insert( path_ ).second ) {
		throw HRuntimeException( "Recursive `source` of '"_ys.append( path_ ).append( "' script detected." ) );
	}
	_activelySourcedStack.push( path_ );
	HScopeExitCall sec(
		HScopeExitCall::call_t(
			[this, path_]() {
				_activelySourcedStack.pop();
				_activelySourced.erase( path_ );
			}
		)
	);
	HString line;
	int lineNo( 1 );
	HString code;
	int exitStatus( 0 );
	while ( getline( shellScript_, line ).good() ) {
		code.append( line );
		if ( ! line.is_empty() && ( line.back() == '\\'_ycp ) ) {
			code.pop_back();
			code.push_back( ' '_ycp );
			++ lineNo;
			continue;
		}
		try {
			_failureMessages.clear();
			exitStatus = run_line( code, EVALUATION_MODE::DIRECT ).exit_status().value;
			code.clear();
			if ( ! _failureMessages.is_empty() ) {
				cerr << path_ << ":" << lineNo << ": " << string::join( _failureMessages, " " ) << endl;
			}
			_failureMessages.clear();
		} catch ( HException const& e ) {
			cerr << "code: `" << code << "`" << endl;
			cerr << path_ << ":" << lineNo << ": " << e.what() << endl;
			throw;
		}
		++ lineNo;
	}
	return exitStatus;
	M_EPILOG
}

void HSystemShell::learn_system_commands( void ) {
	M_PROLOG
	HLock l( _mutex );
	char const* PATH_ENV( ::getenv( "PATH" ) );
	if ( ! PATH_ENV ) {
		return;
	}
	tokens_t paths( split<>( PATH_ENV, PATH_ENV_SEP ) );
	reverse( paths.begin(), paths.end() );
	learn_system_commands( _systemCommands, paths );
	paths = _superUserPaths;
	reverse( paths.begin(), paths.end() );
	learn_system_commands( _systemSuperUserCommands, paths );
	return;
	M_EPILOG
}

void HSystemShell::learn_system_commands( system_commands_t& commands_, yaal::tools::filesystem::paths_t const& paths_ ) {
	M_PROLOG
	for ( filesystem::path_t const& p : paths_ ) {
		try {
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
				commands_[name] = p;
			}
		} catch ( HFSItemException const& ) {
		}
	}
	return;
	M_EPILOG
}

HShell::HLineResult HSystemShell::do_run( yaal::hcore::HString const& line_ ) {
	M_PROLOG
	HLineResult lineResult;
	try {
		lineResult = run_line( line_, EVALUATION_MODE::DIRECT );
	} catch ( HException const& e ) {
		cerr << e.what() << endl;
	}
	return lineResult;
	M_EPILOG
}

void HSystemShell::run_bound( yaal::hcore::HString const& line_ ) {
	M_PROLOG
	run( line_ );
	return;
	M_EPILOG
}

int HSystemShell::run_result( yaal::hcore::HString const& line_ ) {
	M_PROLOG
	return ( run( line_ ).exit_status().value );
	M_EPILOG
}

bool HSystemShell::has_command( yaal::hcore::HString const& name_ ) const {
	M_PROLOG
	HLock l( _mutex );
	bool hasCommand( ( _systemCommands.count( name_ ) > 0 ) || ( _systemSuperUserCommands.count( name_ ) > 0 ) );
	return hasCommand;
	M_EPILOG
}

void HSystemShell::run_substituted( yaal::hcore::HString const& line_, HCapture* capture_ ) {
	M_PROLOG
	run_line( line_, EVALUATION_MODE::COMMAND_SUBSTITUTION, capture_ );
	return;
	M_EPILOG
}

HSystemShell::HLineResult HSystemShell::run_line( yaal::hcore::HString const& line_, EVALUATION_MODE evaluationMode_, HCapture* capture_ ) {
	M_PROLOG
	HString line( line_ );
	line.trim_left();
	if ( line.is_empty() || ( line.front() == '#' ) ) {
		return ( HLineResult( true ) );
	}
	substitute_from_history( line );
	chains_t chains( split_chains( line, evaluationMode_ ) );
	HLineResult lineResult;
	for ( OChain& c : chains ) {
		if ( c._background && ( evaluationMode_ == EVALUATION_MODE::COMMAND_SUBSTITUTION ) ) {
			throw HRuntimeException( "Background jobs in command substitution are forbidden." );
		}
		if ( c._tokens.is_empty() ) {
			continue;
		}
		lineResult = run_chain( c._tokens, c._background, capture_, evaluationMode_, &c == &chains.back() );
	}
	return lineResult;
	M_EPILOG
}

HSystemShell::HLineResult HSystemShell::run_chain( tokens_t const& tokens_, bool background_, HCapture* capture_, EVALUATION_MODE evaluationMode_, bool last_ ) {
	M_PROLOG
	tokens_t pipe;
	bool skip( false );
	HScopeExitCall sec( call( &HSystemShell::cleanup_jobs, this ) );
	for ( HString const& t : tokens_ ) {
		if ( skip ) {
			if ( t == SHELL_OR ) {
				skip = false;
			}
			continue;
		}
		if ( ( t != SHELL_AND ) && ( t != SHELL_OR ) ) {
			pipe.push_back( t );
			continue;
		}
		if ( pipe.is_empty() ) {
			throw HRuntimeException( "Invalid null command." );
		}
		HLineResult pr( run_pipe( pipe, false, capture_, evaluationMode_, true, last_ ) );
		pipe.clear();
		if ( ! pr.valid_shell() ) {
			return pr;
		}
		if ( ( t == SHELL_AND ) && ! pr.success() ) {
			skip = true;
			continue;
		}
		if ( ( t == SHELL_OR ) && pr.success() ) {
			return pr;
		}
	}
	if ( skip ) {
		return ( HLineResult() );
	}
	if ( pipe.is_empty() ) {
		throw HRuntimeException( "Invalid null command." );
	}
	return ( run_pipe( pipe, background_, capture_, evaluationMode_, false, last_ ) );
	M_EPILOG
}

HSystemShell::HLineResult HSystemShell::run_pipe( tokens_t& tokens_, bool background_, HCapture* capture_, EVALUATION_MODE evaluationMode_, bool predecessor_, bool lastChain_ ) {
	M_PROLOG
	commands_t commands;
	commands.emplace_back( make_resource<OCommand>( *this ) );
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
		if ( ( redir != REDIR::NONE ) && commands.back()->_tokens.is_empty() ) {
			throw HRuntimeException( "Invalid null command." );
		}
		if ( ( redir == REDIR::PIPE ) || ( redir == REDIR::PIPE_ERR ) ) {
			if ( ! ( outPath.is_empty() || moveErr ) ) {
				throw HRuntimeException( "Ambiguous output redirect." );
			}
			if ( commands.back()->_tokens.is_empty() || ( ( tokens_.end() - it ) < 2 ) ) {
				throw HRuntimeException( "Invalid null command." );
			}
			if ( ! errPath.is_empty() && ( redir == REDIR::PIPE_ERR ) ) {
				throw HRuntimeException( "Ambiguous error redirect." );
			}
			if ( ( redir == REDIR::PIPE_ERR ) && moveErr ) {
				throw HRuntimeException( "Error stream already redirected." );
			}
			OCommand& previous( *commands.back() );
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
			previous.set_out_pipe( p, ! moveErr, moveErr || ( redir == REDIR::PIPE_ERR ) );
			commands.emplace_back( make_resource<OCommand>( *this ) );
			OCommand& next( *commands.back() );
			next.set_in_pipe( yaal::move( p ) );
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
				util::unescape( outPath, cxx_escape_table() );
			} else if ( ( redir == REDIR::ERR ) || ( redir == REDIR::APP_ERR ) ) {
				if ( ! errPath.is_empty() || joinErr ) {
					throw HRuntimeException( "Ambiguous error redirect." );
				}
				appendErr = redir == REDIR::APP_ERR;
				errPath.assign( *it );
				util::unescape( errPath, cxx_escape_table() );
			} else if ( redir == REDIR::IN ) {
				if ( ! inPath.is_empty() || ( commands.get_size() > 1 ) ) {
					throw HRuntimeException( "Ambiguous input redirect." );
				}
				inPath.assign( *it );
				util::unescape( inPath, cxx_escape_table() );
			}
			previousRedir = REDIR::NONE;
			continue;
		}
		commands.back()->_tokens.push_back( *it );
		previousRedir = redir;
	}
	if ( ! inPath.is_empty() ) {
		commands.front()->_in = ensure_valid(
			make_pointer<HFile>( expand( yaal::move( inPath ) ), HFile::OPEN::READING )
		);
	}
	if ( ! outPath.is_empty() ) {
		commands.back()->_out = ensure_valid(
			make_pointer<HFile>( expand( yaal::move( outPath ) ), appendOut ? HFile::OPEN::WRITING | HFile::OPEN::APPEND : HFile::OPEN::WRITING )
		);
	}
	if ( ! errPath.is_empty() ) {
		commands.back()->_err = ensure_valid(
			make_pointer<HFile>( expand( yaal::move( errPath ) ), appendErr ? HFile::OPEN::WRITING | HFile::OPEN::APPEND : HFile::OPEN::WRITING )
		);
	} else if ( joinErr ) {
		commands.back()->_err = commands.back()->_out;
	}
	job_t job( make_resource<HJob>( *this, yaal::move( commands ), capture_, evaluationMode_, predecessor_, lastChain_ ) );
	HJob& j( *job );
	if ( setup._interactive && ! background_ ) {
		HLock l( _mutex );
		_repl.disable_bracketed_paste();
	}
	bool validShell( j.start( background_ || _background ) );
	if ( background_ ) {
		HLock l( _mutex );
		_jobs.emplace_back( yaal::move( job ) );
		return ( HLineResult() );
	}
	HLineResult sr( validShell, j.wait_for_finish() );
	if ( setup._interactive ) {
		HLock l( _mutex );
		_repl.enable_bracketed_paste();
	}
	if ( sr.exit_status().type == HPipedChild::STATUS::TYPE::PAUSED ) {
		HLock l( _mutex );
		_jobs.emplace_back( yaal::move( job ) );
	} else {
		flush_faliures( job );
	}
	return sr;
	M_EPILOG
}

void HSystemShell::flush_faliures( job_t const& job_ ) {
	M_PROLOG
	HLock l( _mutex );
	tokens_t const& failureMessages( job_->failure_messages() );
	if ( _activelySourced.is_empty() ) {
		for ( yaal::hcore::HString const& failureMessage : failureMessages ) {
			cerr << failureMessage << endl;
		}
	} else {
		_failureMessages.insert( _failureMessages.end(), failureMessages.begin(), failureMessages.end() );
	}
	return;
	M_EPILOG
}

tokens_t HSystemShell::interpolate( yaal::hcore::HString const& token_, EVALUATION_MODE evaluationMode_, OCommand* command_ ) {
	tokens_t exploded( evaluationMode_ != EVALUATION_MODE::TRIAL ? brace_expansion( token_ ) : tokens_t{ token_ } );
	tokens_t interpolated;
	HString const globChars( "*?[" );
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
		bool wantGlob( false );
		bool argAtSubsituted( false );
		for ( yaal::hcore::HString& token : tokens ) {
			QUOTES quotes( str_to_quotes( token ) );
			if ( ! command_ && ( ( quotes == QUOTES::EXEC_SOURCE ) || ( quotes == QUOTES::EXEC_SINK ) ) ) {
				throw HRuntimeException( "Process substitution is not allowed in this contexct." );
			}
			if ( quotes != QUOTES::NONE ) {
				strip_quotes( token );
			}
			if ( ( evaluationMode_ == EVALUATION_MODE::DIRECT ) && ( ( quotes == QUOTES::EXEC ) || ( quotes == QUOTES::EXEC_SOURCE ) || ( quotes == QUOTES::EXEC_SINK ) ) ) {
				capture_t capture( make_pointer<HCapture>( quotes ) );
				if ( quotes != QUOTES::EXEC ) {
					capture->set_call( call( &HSystemShell::run_substituted, this, token, capture.raw() ) );
				} else {
					run_line( token, EVALUATION_MODE::COMMAND_SUBSTITUTION, capture.raw() );
				}
				token.assign( capture->buffer() );
				if ( command_ ) {
					command_->add_capture( capture );
				}
			}
			if ( quotes != QUOTES::SINGLE ) {
				substitute_from_shell( token, quotes );
				util::hide( token, ARG_AT );
				substitute_environment( token, ENV_SUBST_MODE::RECURSIVE );
				util::unhide( token, ARG_AT );
			}
			if ( ( evaluationMode_ == EVALUATION_MODE::DIRECT ) && ( quotes == QUOTES::DOUBLE ) ) {
				substitute_command( token );
			}
			if ( quotes == QUOTES::DOUBLE ) {
				argAtSubsituted = substitute_arg_at( interpolated, param, token );
				continue;
			}
			if ( quotes == QUOTES::SINGLE ) {
				token.replace( "\\", "\\\\" ).replace( "*", "\\*" ).replace( "?", "\\?" );
				param.append( token );
				continue;
			}
			substitute_variable( token );
			tokens_t words( split_quoted( token ) );
			if ( words.get_size() > 1 ) {
				wantGlob = wantGlob || ( words.front().find_one_of( globChars ) != HString::npos );
				param.append( yaal::move( words.front() ) );
				apply_glob( interpolated, yaal::move( param ), wantGlob );
				for ( tokens_t::iterator it( words.begin() + 1 ), end( words.end() - 1 ); it != end; ++ it ) {
					apply_glob( interpolated, yaal::move( *it ), wantGlob );
				}
				param.assign( yaal::move( words.back() ) );
				wantGlob = param.find_one_of( globChars ) != HString::npos;
			} else {
				wantGlob = wantGlob || ( token.find_one_of( globChars ) != HString::npos );
				param.append( yaal::move( token ) );
			}
		}
		if ( ! ( argAtSubsituted && param.is_empty() ) ) {
			apply_glob( interpolated, yaal::move( param ), wantGlob );
		}
	}
	return interpolated;
}

tokens_t HSystemShell::denormalize( tokens_t& tokens_, EVALUATION_MODE evaluationMode_, OCommand* command_ ) {
	M_PROLOG
	tokens_t tmp;
	tokens_t result;
	bool expandExec( _builtins.count( tokens_.front() ) == 0 );
	for ( tokens_t::iterator it( tokens_.begin() ), end( tokens_.end() ); it != end; ) {
		HString const& tok( *it );
		if ( ( it == tokens_.begin() ) && tok.starts_with( "\\" ) ) {
			++ it;
			continue;
		}
		tmp = interpolate( tok, evaluationMode_, command_ );
		if ( expandExec && tok.starts_with( "$(" ) && tok.ends_with( ")" ) ) {
			tokens_.erase( it );
			tokens_.insert( it, tmp.begin(), tmp.end() );
			advance( it, tmp.get_size() );
			end = tokens_.end();
		} else {
			++ it;
		}
		result.insert( result.end(), tmp.begin(), tmp.end() );
	}
	while ( ! result.is_empty() && result.front().is_empty() ) {
		result.erase( result.begin() );
	}
	while ( ! tokens_.is_empty() && tokens_.front().is_empty() ) {
		tokens_.erase( tokens_.begin() );
	}
	return result;
	M_EPILOG
}

void HSystemShell::substitute_variable( yaal::hcore::HString& token_ ) const {
	M_PROLOG
	HLock l( _mutex );
	if ( has_huginn_jobs() ) {
		return;
	}
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

void HSystemShell::resolve_aliases( tokens_t& tokens_ ) const {
	M_PROLOG
	HLock l( _mutex );
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

int HSystemShell::get_job_no( char const* cmdName_, OCommand& command_, bool pausedOnly_ ) {
	M_PROLOG
	HLock l( _mutex );
	int argCount( static_cast<int>( command_._tokens.get_size() ) );
	if ( argCount > 2 ) {
		throw HRuntimeException( to_string( cmdName_ ).append( ": Superfluous parameter! " ).append( command_._tokens.back() ) );
	}
	int jobNo( 1 );
	if ( argCount > 1 ) {
		jobNo = lexical_cast<int>( command_._tokens.back() );
	}
	int jobIdx( -1 );
	int jobCount( 0 );
	int idx( 0 );
	for ( job_t& job : _jobs ) {
		++ idx;
		if ( ! job->is_direct_evaluation() ) {
			continue;
		}
		bool notPaused( job->status().type != HPipedChild::STATUS::TYPE::PAUSED );
		if ( pausedOnly_ && notPaused ) {
			continue;
		}
		if ( notPaused && ! job->in_background() ) {
			continue;
		}
		jobIdx = idx - 1;
		++ jobCount;
		if ( jobCount == jobNo ) {
			break;
		}
	}
	if ( ( jobNo < 1 ) || ( ( argCount > 1 ) && ( jobNo > jobCount ) ) ) {
		throw HRuntimeException( to_string( cmdName_ ).append( ": Invalid job number! " ).append( command_._tokens.back() ) );
	}
	if ( jobIdx < 0 ) {
		throw HRuntimeException( to_string( cmdName_ ).append( ": No current job!" ) );
	}
	return jobIdx;
	M_EPILOG
}

bool HSystemShell::is_command( yaal::hcore::HString const& str_ ) {
	M_PROLOG
	HLock l( _mutex );
	if ( str_.is_empty() ) {
		return ( false );
	}
	bool isCommand( false );
	HString cmd( str_ );
	cmd.trim();
	HFSItem path( cmd );
	isCommand =
		( _aliases.count( cmd ) > 0 )
		|| ( _builtins.count( cmd ) > 0 )
		|| ( _systemCommands.count( cmd ) > 0 )
		|| ( _systemSuperUserCommands.count( cmd ) > 0 )
		|| ( ( cmd.find( '/'_ycp ) != HString::npos ) && !! path && path.is_executable() )
		|| cmd.starts_with( "$(" );
	return isCommand;
	M_EPILOG
}

bool HSystemShell::has_huginn_jobs( void ) const {
	M_PROLOG
	HLock l( _mutex );
	for ( job_t const& job : _jobs ) {
		HJob const& j( *job );
		if ( j.has_huginn_jobs() ) {
			return ( true );
		}
	}
	return ( false );
	M_EPILOG
}

bool HSystemShell::do_try_command( yaal::hcore::HString const& str_ ) {
	M_PROLOG
	HLock l( _mutex );
	return ( do_is_valid_command( str_ ) );
	M_EPILOG
}

bool HSystemShell::do_is_valid_command( yaal::hcore::HString const& str_ ) {
	M_PROLOG
	HLock l( _mutex );
	HString::size_type nonWhitePos( str_.find_other_than( character_class<CHARACTER_CLASS::WHITESPACE>().data() ) );
	if ( ( nonWhitePos != HString::npos ) && ( str_[nonWhitePos] == '#'_ycp ) ) {
		return ( true );
	}
	chains_t chains( split_chains( str_, EVALUATION_MODE::TRIAL ) );
	for ( OChain& chain : chains ) {
		if ( chain._tokens.is_empty() ) {
			continue;
		}
		bool head( true );
		for ( HString const& token : chain._tokens ) {
			if ( token.starts_with( "$(" ) ) {
				return ( true );
			}
		}
		try {
			chain._tokens = denormalize( chain._tokens, EVALUATION_MODE::TRIAL );
			if ( chain._tokens.is_empty() ) {
				continue;
			}
		} catch ( HException const& ) {
		}
		for ( HString& token : chain._tokens ) {
			if ( head ) {
				token.trim();
				if ( token.is_empty() ) {
					continue;
				}
				HFSItem path( token );
				if (
					( _aliases.count( token ) > 0 )
					|| ( _builtins.count( token ) > 0 )
					|| ( _systemCommands.count( token ) > 0 )
					|| ( _systemSuperUserCommands.count( token ) > 0 )
					|| ( !! path && path.is_executable() )
				) {
					return ( true );
				}
			}
			head = ( token == SHELL_AND ) || ( token == SHELL_OR ) || ( token == SHELL_PIPE ) || ( token == SHELL_PIPE_ERR );
		}
	}
	return ( false );
	M_EPILOG
}

namespace {

template<typename coll_t>
void get_suggestion( coll_t const& coll_, HString const& line_, int& minDist_, HString& suggestion_ ) {
	for ( typename coll_t::value_type const& e : coll_ ) {
		int dist( string::distance( line_, e.first ) );
		if ( dist < minDist_ ) {
			suggestion_.assign( e.first );
			minDist_ = dist;
		}
	}
}

}

void HSystemShell::command_not_found( yaal::hcore::HString const& line_ ) {
	M_PROLOG
	HString line( line_ );
	line.trim();
	tokens_t tokens( string::split( line, character_class<CHARACTER_CLASS::WHITESPACE>().data(), HTokenizer::DELIMITED_BY_ANY_OF ) );
	if ( tokens.is_empty() ) {
		return;
	}
	line.assign( tokens.front() );
	line.trim();
	if ( line.find_last_one_of( character_class<CHARACTER_CLASS::WORD>().data() ) == HString::npos ) {
		return;
	}
	int minDist( static_cast<int>( line.get_length() ) );
	HString suggestion;
	get_suggestion( _systemCommands, line, minDist, suggestion );
	get_suggestion( _builtins, line, minDist, suggestion );
	get_suggestion( _aliases, line, minDist, suggestion );
	if ( ! suggestion.is_empty() ) {
		cerr << line << ": command not found, did you mean: `" << suggestion << "`?" << endl;
	} else {
		cerr << line << ": command not found!" << endl;
	}
	return;
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

HRepl& HSystemShell::repl( void ) const {
	return ( _repl );
}

HLineRunner& HSystemShell::line_runner( void ) {
	return ( _lineRunner );
}

bool HSystemShell::is_tracing( void ) const {
	return ( _trace );
}

yaal::hcore::HString const& HSystemShell::trace_prompt( void ) const {
	return ( _tracePrompt );
}

bool HSystemShell::loaded( void ) const {
	return ( _loaded );
}

}

