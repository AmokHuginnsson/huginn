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
#include "src/colorize.hxx"
#include "src/quotes.hxx"
#include "src/setup.hxx"
#include "util.hxx"

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;
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
	HString pwdReal;
	try {
		pwdReal.assign( filesystem::current_working_directory() );
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
		command_ << index << " " << compact_path( dir ) << endl;
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

void HSystemShell::setopt_history_path( tokens_t& values_ ) {
	M_PROLOG
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
		HString command;
		if ( argCount == 3 ) {
			command.assign( command_._tokens[2] );
			try {
				QUOTES quotes( str_to_quotes( command ) );
				if ( ( quotes == QUOTES::SINGLE ) || ( quotes == QUOTES::DOUBLE ) ) {
					strip_quotes( command );
				}
			} catch ( HException const& ) {
			}
		} else {
			command.assign( stringify_command( command_._tokens, 2 ) );
		}
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

void HSystemShell::history( OCommand& command_ ) {
	M_PROLOG
	HProgramOptionsHandler po( "history" );
	HOptionInfo info( po );
	info
		.name( "history" )
		.intro( "a history interface" )
		.description( "Show current history (with indices and colors)." )
		.syntax( "[--indexed] [--no-color]" )
		.brief( true );
	bool help( false );
	bool noColor( false );
	bool indexed( false );
	po(
		HProgramOptionsHandler::HOption()
		.long_form( "indexed" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description( "list history entries with an index number" )
		.recipient( indexed )
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
	HFormat format( "%"_ys.append( to_string( _repl.history_size() ).get_length() ).append( "d" ) );
	for ( HString const& l : _repl.history() ) {
		if ( indexed ) {
			command_ << ( format % idx ).string() << "  ";
		}
		command_ << ( noColor ? l : colorize( l, this ) ) << endl;
		++ idx;
	}
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
		command_ << "[" << no << "] " << status_type_to_str( job->status() ) << " " << colorize( job->desciption(), this ) << endl;
		++ no;
	}
	return;
	M_EPILOG
}

void HSystemShell::bg( OCommand& command_ ) {
	M_PROLOG
	job_t& job( _jobs[get_job_no( "bg", command_, true )] );
	if ( job->is_system_command() && ( job->status().type == HPipedChild::STATUS::TYPE::PAUSED ) ) {
		job->do_continue( true );
	}
	return;
	M_EPILOG
}

void HSystemShell::fg( OCommand& command_ ) {
	M_PROLOG
	job_t& job( _jobs[get_job_no( "fg", command_, false )] );
	if ( job->is_system_command() && ( ( job->status().type == HPipedChild::STATUS::TYPE::PAUSED ) || job->in_background() ) ) {
		cerr << colorize( job->desciption(), this ) << endl;
		job->bring_to_foreground();
		job->do_continue( false );
		job->wait_for_finish();
	}
	return;
	M_EPILOG
}

void HSystemShell::source( OCommand& command_ ) {
	M_PROLOG
	int argCount( static_cast<int>( command_._tokens.get_size() ) );
	if ( argCount < 2 ) {
		throw HRuntimeException( "source: Too few arguments!" );
	}
	command_._tokens.erase( command_._tokens.begin() );
	for ( HString t : command_._tokens ) {
		t = stringify_command( interpolate( t, EVALUATION_MODE::DIRECT ) );
		if ( t.is_empty() ) {
			continue;
		}
		do_source( t );
	}
	return;
	M_EPILOG
}

void HSystemShell::eval( OCommand& command_ ) {
	M_PROLOG
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
	int argCount( static_cast<int>( command_._tokens.get_size() ) );
	if ( argCount < 2 ) {
		throw HRuntimeException( "exec: Too few arguments!" );
	}
	tokens_t tokens( denormalize( command_._tokens, EVALUATION_MODE::DIRECT ) );
	int argc( static_cast<int>( tokens.get_size() - 1 ) );
	HResource<char*[]> argvHolder( new char*[argc + 1] );
	char** argv( argvHolder.get() );
	argv[argc] = nullptr;
	typedef yaal::hcore::HArray<HUTF8String> utf8_strings_t;
	utf8_strings_t argvDataHolder;
	argvDataHolder.reserve( argc );
	for ( int i( 0 ); i < argc; ++ i ) {
		argvDataHolder.emplace_back( tokens[i + 1] );
		argv[i] = const_cast<char*>( argvDataHolder.back().c_str() );
	}
	::execvp( argv[0], argv );
	cerr << error_message(errno) << endl;
	return;
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
yaal::hcore::HString paint( yaal::hcore::HString&& str_ ) {
	str_
		.replace( "%b", ! setup._noColor ? ansi_color( GROUP::SHELL_BUILTINS ) : "" )
		.replace( "%a", ! setup._noColor ? ansi_color( GROUP::ALIASES ) : "" )
		.replace( "%s", ! setup._noColor ? ansi_color( GROUP::SWITCHES ) : "" )
		.replace( "%d", ! setup._noColor ? ansi_color( GROUP::DIRECTORIES ) : "" )
		.replace( "%l", ! setup._noColor ? ansi_color( GROUP::LITERALS ) : "" )
		.replace( "%%", "%" )
		.replace( "%0", ! setup._noColor ? *ansi::reset : "" );
	return ( str_ );
}
}

void HSystemShell::help( OCommand& command_ ) {
	M_PROLOG
	int argCount( static_cast<int>( command_._tokens.get_size() ) );
	if ( argCount > 2 ) {
		throw HRuntimeException( "help: Too many parameters!" );
	}
	command_ << paint(
		"%balias%0 %aname%0 command args... - create shell command alias\n"
		"%bbg%0 [%lno%0]                    - put job in the background\n"
		"%bbindkey%0 keyname action     - bind given action as key handler\n"
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
	);
	return;
	M_EPILOG
}

}

