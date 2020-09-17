/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include "config.hxx"

#ifdef __MSVCXX__
#include <process.h>
#define execvp _execvp
#else
#include <unistd.h>
#endif

#include <yaal/hcore/hcore.hxx>
#include <yaal/hcore/hformat.hxx>
#include <yaal/hcore/hprogramoptionshandler.hxx>
#include <yaal/tools/hfsitem.hxx>

M_VCSID( "$Id: " __ID__ " $" )
M_VCSID( "$Id: " __TID__ " $" )

#include "src/systemshell.hxx"
#include "command.hxx"
#include "job.hxx"
#include "src/colorize.hxx"
#include "src/quotes.hxx"
#include "src/setup.hxx"
#include "util.hxx"

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;
using namespace yaal::tools::filesystem;
using namespace yaal::tools::util;
using namespace yaal::tools::string;

namespace {

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

namespace huginn {

void HSystemShell::alias( OCommand& command_ ) {
	M_PROLOG
	HLock l( _mutex );
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
		try {
			for ( aliases_t::value_type const& a : _aliases ) {
				command_ << setw( longestAliasNameLength + 2 ) << a.first;
				command_ << colorize( stringify_command( a.second ), this ) << endl;
			}
			command_ << right;
		} catch ( hcore::HException const& ) {
			command_._out->reset();
		}
	} else if ( argCount == 2 ) {
		aliases_t::const_iterator a( _aliases.find( command_._tokens.back() ) );
		if ( a != _aliases.end() ) {
			command_ << a->first << " " << colorize( stringify_command( a->second ), this ) << endl;
		}
	} else {
		_aliases[command_._tokens[1]] = tokens_t( command_._tokens.begin() + 2, command_._tokens.end() );
	}
	return;
	M_EPILOG
}

void HSystemShell::unalias( OCommand& command_ ) {
	M_PROLOG
	HLock l( _mutex );
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
	HLock l( _mutex );
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
	if ( path.get_length() > 1 ) {
		path.trim_right( "/\\" );
	}
	HString pwdReal;
	try {
		pwdReal.assign( filesystem::current_working_directory() );
#ifdef __MSVCXX__
		pwdReal.lower().replace( "\\", "/" );
#endif
	} catch ( filesystem::HFileSystemException const& ) {
	}
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
	filesystem::paths_t::iterator it( find( _dirStack.begin(), _dirStack.end(), pwd ) );
	if  ( it != _dirStack.end() ) {
		_dirStack.erase( it );
	}
	_dirStack.push_back( pwd );
	return;
	M_EPILOG
}

void HSystemShell::dir_stack( OCommand& command_ ) {
	M_PROLOG
	HLock l( _mutex );
	HProgramOptionsHandler po( "dirs" );
	HOptionInfo info( po );
	info
		.name( "dirs" )
		.intro( "show a directory stack" )
		.description( "List a current directory stack with indices, possibly escaped." )
		.syntax( "[--escape] [--no-index]" )
		.brief( true );
	bool help( false );
	bool noIndex( false );
	bool escape( false );
	po(
		HProgramOptionsHandler::HOption()
		.long_form( "no-index" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description( "dump directory stack entries without an index number" )
		.recipient( noIndex )
	)(
		HProgramOptionsHandler::HOption()
		.long_form( "escape" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description( "escape special characters in each directory stack entry" )
		.recipient( escape )
	)(
		HProgramOptionsHandler::HOption()
		.long_form( "help" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description( "display this help and stop" )
		.recipient( help )
	);
	int unknown( 0 );
	command_._tokens = po.process_command_line( yaal::move( command_._tokens ), &unknown );
	if ( help || unknown ) {
		util::show_help( info );
		if ( unknown > 0 ) {
			throw HRuntimeException( "dirs: unknown parameter!" );
		}
		return;
	}
	int index( 0 );
	yaal::tools::filesystem::paths_t dirStack( _dirStack );
	l.unlock();
	for ( filesystem::path_t const& dir : reversed( dirStack ) ) {
		if ( ! noIndex ) {
			command_ << index << " ";
		}
		command_ << ( escape ? escape_path( compact_path( dir ) ) : compact_path( dir ) ) << endl;
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
		set_env(
			command_._tokens[1],
			argCount > 2
				? stringify_command( interpolate( command_._tokens.back(), EVALUATION_MODE::DIRECT ) )
				: HString()
		);
	}
	return;
	M_EPILOG
}

void HSystemShell::setopt( OCommand& command_ ) {
	M_PROLOG
	HLock l( _mutex );
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
	HLock l( _mutex );
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

void HSystemShell::setopt_history_path( tokens_t& values_ ) {
	M_PROLOG
	HLock l( _mutex );
	if ( values_.get_size() != 1 ) {
		throw HRuntimeException( "setopt history_path option requires exactly one parameter!" );
	}
	setup._historyPath.assign( stringify_command( interpolate( values_.front(), EVALUATION_MODE::DIRECT ) ) );
	_repl.set_history_path( setup._historyPath );
	return;
	M_EPILOG
}

void HSystemShell::setopt_history_max_size( tokens_t& values_ ) {
	M_PROLOG
	HLock l( _mutex );
	if ( values_.get_size() != 1 ) {
		throw HRuntimeException( "setopt history_max_size option requires exactly one parameter!" );
	}
	int historyMaxSize( lexical_cast<int>( values_.front() ) );
	if ( historyMaxSize < 0 ) {
		throw HRuntimeException( "setopt history_max_size: new value must be non-negative ("_ys.append( values_.front() ).append( ")!" ) );
	}
	_repl.set_max_history_size( historyMaxSize );
	return;
	M_EPILOG
}

void HSystemShell::setopt_trace( tokens_t& values_ ) {
	M_PROLOG
	HLock l( _mutex );
	if ( values_.get_size() != 1 ) {
		throw HRuntimeException( "setopt trace option requires exactly one parameter!" );
	}
	_trace = lexical_cast<bool>( values_.front() );
	return;
	M_EPILOG
}

void HSystemShell::setopt_super_user_paths( tokens_t& values_ ) {
	M_PROLOG
	HLock l( _mutex );
	if ( values_.is_empty() ) {
		throw HRuntimeException( "setopt super_user_paths option requires at least one parameter!" );
	}
	_superUserPaths.clear();
	for ( yaal::hcore::HString const& word : values_ ) {
		_superUserPaths.push_back( stringify_command( interpolate( word, EVALUATION_MODE::DIRECT ) ) );
	}
	return;
	M_EPILOG
}

void HSystemShell::setopt_prefix_commands( tokens_t& values_ ) {
	M_PROLOG
	HLock l( _mutex );
	if ( values_.is_empty() ) {
		throw HRuntimeException( "setopt prefix_commands option requires at least one parameter!" );
	}
	_prefixCommands.clear();
	for ( yaal::hcore::HString const& word : values_ ) {
		_prefixCommands.insert( word );
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
	HLock l( _mutex );
	int argCount( static_cast<int>( command_._tokens.get_size() ) );
	bool internal( false );
	bool system( false );
	if ( argCount >= 2 ) {
		internal = command_._tokens[1] == "--interenal";
		system   = command_._tokens[1] == "--system";
	}
	if ( internal || system ) {
		-- argCount;
	}
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
		try {
			int longestKeyNameLength( ! _keyBindings.is_empty() ? static_cast<int>( longestKeyNameIt->first.get_length() ) : 0 );
			for ( key_bindings_t::value_type const& kb : _keyBindings ) {
				command_ << setw( longestKeyNameLength + 2 ) << kb.first;
				command_ << colorize( kb.second, this ) << endl;
			}
			command_ << right;
		} catch ( hcore::HException const& ) {
			command_._out->reset();
		}
	} else if ( argCount == 2 ) {
		key_bindings_t::const_iterator kb( _keyBindings.find( command_._tokens.back() ) );
		if ( kb != _keyBindings.end() ) {
			command_ << command_._tokens.back() << " " << colorize( kb->second, this ) << endl;
		} else {
			throw HRuntimeException( "bindkey: Unbound key: `"_ys.append( command_._tokens.back() ).append( "`" ) );
		}
	} else {
		if ( ! ( internal || system ) ) {
			throw HRuntimeException( "bindkey: bind target not set" );
		}
		HString command;
		if ( argCount == 3 ) {
			command.assign( command_._tokens[3] );
			try {
				QUOTES quotes( str_to_quotes( command ) );
				if ( ( quotes == QUOTES::SINGLE ) || ( quotes == QUOTES::DOUBLE ) ) {
					strip_quotes( command );
				}
			} catch ( HException const& ) {
			}
		} else {
			command.assign( stringify_command( command_._tokens, 3 ) );
		}
		HString const& keyName( command_._tokens[2] );
		if ( internal ) {
			if ( _repl.bind_key( keyName, command ) ) {
				_keyBindings[keyName] = command;
			} else {
				throw HRuntimeException(
					"bindkey: invalid key name: `"_ys
						.append( keyName )
						.append( "` or `repl`s internal functions name: `" )
						.append( command )
						.append( "`" )
				);
			}
		} else if ( _repl.bind_key( keyName, call( &HSystemShell::run_bound, this, command ) ) ) {
			M_ASSERT( system );
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
	HLock l( _mutex );
	int argCount( static_cast<int>( command_._tokens.get_size() ) );
	if ( argCount > 1 ) {
		throw HRuntimeException( "rehash: Superfluous parameter!" );
	}
	_systemCommands.clear();
	learn_system_commands();
	if ( ! _loaded ) {
		return;
	}
	char const* HOME( ::getenv( "HOME" ) );
	if ( ! HOME ) {
		return;
	}
	HString cacheDir;
	cacheDir.assign( HOME ).append( "/.cache/huginn" );
	try {
		filesystem::remove_directory( cacheDir, DIRECTORY_MODIFICATION::RECURSIVE );
	} catch ( ... ) {
	}
	return;
	M_EPILOG
}

void HSystemShell::history( OCommand& command_ ) {
	M_PROLOG
	HLock l( _mutex );
	HProgramOptionsHandler po( "history" );
	HOptionInfo info( po );
	info
		.name( "history" )
		.intro( "a history interface" )
		.description( "Show current history (with indices and colors)." )
		.syntax( "[--indexed] [--timestamps] [--no-color]" )
		.brief( true );
	bool help( false );
	bool noColor( false );
	bool indexed( false );
	bool timestamps( false );
	po(
		HProgramOptionsHandler::HOption()
		.long_form( "indexed" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description( "list history entries with an index number" )
		.recipient( indexed )
	)(
		HProgramOptionsHandler::HOption()
		.long_form( "timestamps" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description( "list history entries with a timestamps for each entry" )
		.recipient( timestamps )
	)(
		HProgramOptionsHandler::HOption()
		.long_form( "no-color" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description( "do not use syntax highlighting for history entries" )
		.recipient( noColor )
	)(
		HProgramOptionsHandler::HOption()
		.long_form( "help" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description( "display this help and stop" )
		.recipient( help )
	);
	int unknown( 0 );
	command_._tokens = po.process_command_line( yaal::move( command_._tokens ), &unknown );
	if ( help || unknown ) {
		util::show_help( info );
		if ( unknown > 0 ) {
			throw HRuntimeException( "history: unknown parameter!" );
		}
		return;
	}
	int idx( 1 );
	HRepl::history_entries_t historyEntries( _repl.history() );
	HFormat format( "%"_ys.append( to_string( _repl.history_size() ).get_length() ).append( "d" ) );
	l.unlock();
	try {
		for ( HRepl::HHistoryEntry const& he : historyEntries ) {
			if ( indexed ) {
				command_ << ( format % idx ).string() << "  ";
			}
			if ( timestamps ) {
				command_ << he.timestamp() << "  ";
			}
			command_ << ( noColor ? he.text() : colorize( he.text(), this ) ) << endl;
			++ idx;
		}
	} catch ( hcore::HException const& ) {
		command_._out->reset();
	}
	return;
	M_EPILOG
}

void HSystemShell::jobs( OCommand& command_ ) {
	M_PROLOG
	HLock l( _mutex );
	int argCount( static_cast<int>( command_._tokens.get_size() ) );
	if ( argCount > 1 ) {
		throw HRuntimeException( "jobs: Superfluous parameter!" );
	}
	int no( 1 );
	for ( job_t& job : _jobs ) {
		if ( ! job->is_direct_evaluation() ) {
			continue;
		}
		command_ << "[" << no << "] " << status_type_to_str( job->status() ) << " " << colorize( job->description(), this ) << endl;
		++ no;
	}
	return;
	M_EPILOG
}

void HSystemShell::bg( OCommand& command_ ) {
	M_PROLOG
	HLock l( _mutex );
	job_t& job( _jobs[get_job_no( "bg", command_, true )] );
	if ( job->status().type == HPipedChild::STATUS::TYPE::PAUSED ) {
		job->do_continue( true );
	}
	return;
	M_EPILOG
}

void HSystemShell::fg( OCommand& command_ ) {
	M_PROLOG
	HLock l( _mutex );
	job_t& job( _jobs[get_job_no( "fg", command_, false )] );
	if ( ( job->status().type == HPipedChild::STATUS::TYPE::PAUSED ) || job->in_background() ) {
		cerr << colorize( job->description(), this ) << endl;
		job->bring_to_foreground();
		job->do_continue( false );
		HPipedChild::STATUS status( job->wait_for_finish() );
		if ( ( status.type != HPipedChild::STATUS::TYPE::RUNNING ) && ( status.type != HPipedChild::STATUS::TYPE::PAUSED ) ) {
			for ( jobs_t::iterator it( _jobs.begin() ), end( _jobs.end() ); it != end; ++ it ) {
				if ( &*it == &job ) {
					_jobs.erase( it );
					break;
				}
			}
		}
	}
	return;
	M_EPILOG
}

void HSystemShell::source( OCommand& command_ ) {
	M_PROLOG
	HLock l( _mutex );
	int argCount( static_cast<int>( command_._tokens.get_size() ) );
	if ( argCount < 2 ) {
		throw HRuntimeException( "source: Too few arguments!" );
	}
	command_._tokens.erase( command_._tokens.begin() );
	do_source( denormalize( command_._tokens, EVALUATION_MODE::DIRECT ) );
	return;
	M_EPILOG
}

void HSystemShell::call_huginn( OCommand& command_ ) {
	M_PROLOG
	HLock l( _mutex );
	int argCount( static_cast<int>( command_._tokens.get_size() ) );
	if ( argCount < 2 ) {
		throw HRuntimeException( "call: Too few arguments!" );
	}
	tokens_t tokens( denormalize( command_._tokens, EVALUATION_MODE::DIRECT ) );
	tokens.erase( tokens.begin() );
	command_._tokens = yaal::move( tokens );
	l.unlock();
	command_.spawn_huginn( true );
	return;
	M_EPILOG
}

void HSystemShell::eval( OCommand& command_ ) {
	M_PROLOG
	HLock l( _mutex );
	int argCount( static_cast<int>( command_._tokens.get_size() ) );
	if ( argCount < 2 ) {
		throw HRuntimeException( "eval: Too few arguments!" );
	}
	tokens_t tokens( denormalize( command_._tokens, EVALUATION_MODE::DIRECT ) );
	HString cmd( stringify_command( tokens, 1 ) );
	run_line( cmd, EVALUATION_MODE::DIRECT );
	return;
	M_EPILOG
}

void HSystemShell::exec( OCommand& command_ ) {
	M_PROLOG
	HLock l( _mutex );
	int argCount( static_cast<int>( command_._tokens.get_size() ) );
	if ( argCount < 2 ) {
		throw HRuntimeException( "exec: Too few arguments!" );
	}
	_repl.save_history();
	tokens_t tokens( denormalize( command_._tokens, EVALUATION_MODE::DIRECT ) );
	tokens.erase( tokens.begin() );
	session_stop();
	try {
		system::exec( tokens.front(), tokens );
	} catch ( ... ) {
		session_start();
		throw;
	}
	M_EPILOG
}

void HSystemShell::exit( OCommand& command_ ) {
	M_PROLOG
	int argCount( static_cast<int>( command_._tokens.get_size() ) );
	if ( argCount > 2 ) {
		throw HRuntimeException( "exit: Too many arguments!" );
	}
	if ( argCount > 1 ) {
		command_._status.value = lexical_cast<int>( command_._tokens.back() );
	}
	setup._interactive = false;
	return;
	M_EPILOG
}

namespace {

char const* context_color( char const* color_ ) {
	return (
		! setup._noColor
			? color_
			: ( setup._verbose ? "__" : "" )
	);
}

char const* context_color( GROUP group_ ) {
	return ( context_color( ansi_color( group_ ) ) );
}

yaal::hcore::HString paint( yaal::hcore::HString&& str_ ) {
	str_
		.replace( "%b", context_color( GROUP::SHELL_BUILTINS ) )
		.replace( "%a", context_color( GROUP::ALIASES ) )
		.replace( "%s", context_color( GROUP::SWITCHES ) )
		.replace( "%d", context_color( GROUP::DIRECTORIES ) )
		.replace( "%l", context_color( GROUP::LITERALS ) )
		.replace( "%e", context_color( GROUP::ENVIRONMENT ) )
		.replace( "%E", context_color( GROUP::ENV_STR ) )
		.replace( "%o", context_color( GROUP::OPERATORS ) )
		.replace( "%t", context_color( GROUP::BUILTINS ) )
		.replace( "%T", context_color( GROUP::CLASSES ) )
		.replace( "%k", context_color( GROUP::KEYWORDS ) )
		.replace( "%c", context_color( GROUP::EXECUTABLES ) )
		.replace( "%%", "%" )
		.replace( "%0", context_color( *ansi::reset ) );
	return ( yaal::move( str_ ) );
}

char const HELP_INDEX[] =
	"%balias%0 %aname%0 command args... - create shell command alias\n"
	"%bbg%0 [%lno%0]                    - put job in the background\n"
	"%bbindkey%0 %stgt%0 keyname %laction%0 - bind given action as key handler\n"
	"%bcall%0 %efunc%0 arg1 arg2 ...    - call given Huginn function %efunc%0 directly\n"
	"%bcd%0 (%d/path/%0|%s-%0|=%ln%0)           - change current working directory\n"
	"%bdirs%0                       - show current directory stack\n"
	"%beval%0 command args...       - evaluate command in this shell context\n"
	"                             after full expansion\n"
	"%bexec%0 command args...       - replace current shell process image\n"
	"                             by executing given command\n"
	"%bexit%0 [%lval%0]                 - exit current shell with given status\n"
	"%bfg%0 [%lno%0]                    - bring job into the foreground\n"
	"%bhelp%0 [%bbuilt-in%0]            - show this help message\n"
	"%bhistory%0 [%s--indexed%0]        - show command history\n"
	"%bjobs%0                       - list currently running jobs\n"
	"%brehash%0                     - re-learn locations of system commands\n"
	"%bsetenv%0 NAME %l\"value\"%0        - set environment variable to given value\n"
	"%bsetopt%0 name values...      - set shell configuration option\n"
	"%bsource%0 paths...            - read and execute shell commands from given files\n"
	"%bunalias%0 %aname%0               - remove given alias\n"
	"%bunsetenv%0 NAMES...          - remove given environment variables\n"
;

char const HELP_ALIAS[] =
	"%balias%0 %aname%0 command args...\n\n"
	"Create or overwrite shell command alias.\n"
;

char const HELP_BG[] =
	"%bbg%0 [%lno%0]\n\n"
	"Put job in the background.\n"
;

char const HELP_BINDKEY[] =
	"%bbindkey%0 %s--target%0 keyname %l\"action\"%0\n\n"
	"Bind given %laction%0 as a key handler, where %s--target%0 is either:\n\n"
	"%s--internal%0 - an %laction%0 is an internal REPL function\n"
	"%s--system%0   - an %laction%0 is an external system command\n"
;

char const HELP_CALL[] =
	"%bcall%0 %efunc%0 arg1 arg2 ...\n\n"
	"Call given Huginn function %efunc%0 directly, passing\n"
	"arg1, arg2... as Huginn `%tstring%0`s as function arguments.\n"
;

char const HELP_CD[] =
	"%bcd%0 (%d/path/%0|%s-%0|=%ln%0)\n\n"
	"Change this shell's current working directory:\n\n"
	"  %bcd%0 %d/path/%0 - change directory to given path\n"
	"  %bcd%0 %s-%0      - change directory to previous working directory\n"
	"  %bcd%0 =%ln%0     - change directory to %ln%0th directory on directory stack\n"
	"  %bcd%0        - change directory to users home directory\n\n"
	"If successful, current working directory is changed\n"
	"and %e${PWD}%0 environment variable is set.\n"
;

char const HELP_DIRS[] =
	"%bdirs%0 [%s--no-index%0] [%s--escape%0]\n\n"
	"Show current directory stack.\n\n"
	"%s--no-index%0 - do not show index of the entry\n"
	"%s--escape%0   - escape special characters in entry\n"
;

char const HELP_EVAL[] =
	"%beval%0 command args...\n\n"
	"Evaluate command in this shell context after performing\n"
	"a full command line expansion, i.e:\n"
	"  - environment variable substitution - %l\"%0%E${VAR}%0%l\"%0\n"
	"  - command substitution - %o$(%0cmd...%o)%0\n"
	"  - huginn value substitution\n"
	"  - brace expansion\n"
	"  - glob pattern expansion\n"
;

char const HELP_EXEC[] =
	"%bexec%0 command args...\n\n"
	"Replace current shell process image by executing given command.\n"
;

char const HELP_EXIT[] =
	"%bexit%0 [%lval%0]\n\n"
	"Exit current shell with given status.\n"
;

char const HELP_FG[] =
	"%bfg%0 [%lno%0]\n\n"
	"Bring job into the foreground.\n"
;

char const HELP_HELP[] =
	"%bhelp%0 [topic]\n\n"
	"Show help on given topic.\n"
;

char const HELP_HISTORY[] =
	"%bhistory%0 [%s--indexed%0] [%s--timestamps%0] [%s--no-color%0]\n\n"
	"Show command history:\n"
	"  %s--indexed%0    - with index numbers\n"
	"  %s--timestamps%0 - with timestamp for each entry\n"
	"  %s--no-color%0   - do not colorize the listing\n"
;

char const HELP_JOBS[] =
	"%bjobs%0\n\n"
	"List currently running jobs.\n"
;

char const HELP_REHASH[] =
	"%brehash%0\n\n"
	"Re-learn locations of system commands found in each directory\n"
	"mentioned in %e${PATH}%0 environment variable.\n"
;

char const HELP_SETENV[] =
	"%bsetenv%0 NAME %l\"value\"%0\n\n"
	"Set environment variable to given value.\n"
;

char const HELP_SETOPT[] =
	"%bsetopt%0 name values...\n\n"
	"Set shell configuration option.\n"
	"Shell options are:\n"
	"  - history_path\n"
	"  - history_max_size\n"
	"  - ignore_filenames\n"
	"  - super_user_paths\n"
	"  - trace\n"
	"  - prefix_commands\n"
;

char const HELP_SOURCE[] =
	"%bsource%0 paths...\n\n"
	"Read and execute shell commands from given files.\n"
;

char const HELP_UNALIAS[] =
	"%bunalias%0 %aname%0\n\n"
	"Remove given alias.\n"
;

char const HELP_UNSETENV[] =
	"%bunsetenv%0 NAMES...\n\n"
	"Remove given environment variables.\n"
;

char const HELP_HISTORY_PATH[] =
	"%bsetopt%0 history_path %d/path/to/file%0\n\n"
	"Set path for REPL's history file.\n"
;

char const HELP_HISTORY_MAX_SIZE[] =
	"%bsetopt%0 history_max_size %ln%0\n\n"
	"Set maximum number of entries preserved in REPL's history.\n"
;

char const HELP_IGNORE_FILENAMES[] =
	"%bsetopt%0 ignore_filenames re_pattern1 re_pattern2 ...\n\n"
	"Ignore filenames matching following regular expression patterns\n"
	"while performing filename completion.\n"
;

char const HELP_SUPER_USER_PATHS[] =
	"%bsetopt%0 super_user_paths %d/path1/%0 %d/other/path/%0\n\n"
	"Set paths where super user commands can be found.\n"
;

char const HELP_TRACE[] =
	"%bsetopt%0 trace (%lon%0|%loff%0)\n\n"
	"Set trace flag - for displaying expanded commands just before their execution.\n"
;

char const HELP_PREFIX_COMMANDS[] =
	"%bsetopt%0 prefix_commands cmd1 cmd2 ...\n\n"
	"Set list of prefix commands.\n"
	"Prefix commands allow expanding aliases passed as arguments to prefix commands, e.g.:\n"
	"%bsetopt%0 prefix_commands %cenv%0\n"
	"%cenv%0 %asome_alias%0 param1 param2 ...\n"
;

}

void HSystemShell::help( OCommand& command_ ) {
	M_PROLOG
	int argCount( static_cast<int>( command_._tokens.get_size() ) );
	if ( argCount > 2 ) {
		throw HRuntimeException( "help: Too many parameters!" );
	}
	if ( argCount < 2 ) {
		command_ << paint( HELP_INDEX ) << flush;
		return;
	}
	HString const& topic( command_._tokens.back() );
	struct Help {
		char const* topic;
		char const* helpStr;
	} helpTopics[] = {
		{ "alias",    HELP_ALIAS },
		{ "bg",       HELP_BG },
		{ "bindkey",  HELP_BINDKEY },
		{ "call",     HELP_CALL },
		{ "cd",       HELP_CD },
		{ "dirs",     HELP_DIRS },
		{ "eval",     HELP_EVAL },
		{ "exec",     HELP_EXEC },
		{ "exit",     HELP_EXIT },
		{ "fg",       HELP_FG },
		{ "help",     HELP_HELP },
		{ "history",  HELP_HISTORY },
		{ "jobs",     HELP_JOBS },
		{ "rehash",   HELP_REHASH },
		{ "setenv",   HELP_SETENV },
		{ "setopt",   HELP_SETOPT },
		{ "source",   HELP_SOURCE },
		{ "unalias",  HELP_UNALIAS },
		{ "unsetenv", HELP_UNSETENV },
		{ "history_path",     HELP_HISTORY_PATH },
		{ "history_max_size", HELP_HISTORY_MAX_SIZE },
		{ "ignore_filenames", HELP_IGNORE_FILENAMES },
		{ "super_user_paths", HELP_SUPER_USER_PATHS },
		{ "trace",            HELP_TRACE },
		{ "prefix_commands",  HELP_PREFIX_COMMANDS }
	};
	for ( Help const& h : helpTopics ) {
		if ( topic == h.topic ) {
			command_ << paint( h.helpStr ) << flush;
			return;
		}
	}
	if ( topic == "topics" ) {
		for ( Help const& h : helpTopics ) {
			command_ << h.topic << endl;
		}
		return;
	}
	throw HRuntimeException( "help: Unknown help topic: `"_ys.append( topic ).append( "`!" ) );
	M_EPILOG
}

}

