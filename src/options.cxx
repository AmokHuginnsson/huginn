/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <cstdlib>
#include <cstdio>
#include <cstring>

#include "config.hxx"

#include <yaal/tools/util.hxx>
#include <yaal/hcore/hprogramoptionshandler.hxx>
#include <yaal/hcore/hlog.hxx>
#include <yaal/hcore/hcore.hxx>
#include <yaal/tools/stringalgo.hxx>
#include <yaal/config.hxx>
M_VCSID( "$Id: " __ID__ " $" )

#include "options.hxx"
#include "setup.hxx"
#include "commit_id.hxx"

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;
using namespace yaal::tools::util;

namespace huginn {

namespace {

bool set_variables( HString& option_, HString& value_ ) {
	static bool const HUGINN_RC_DEBUG( !! ::getenv( "HUGINN_RC_DEBUG" ) );
	if ( HUGINN_RC_DEBUG ) {
		cout << "option: [" << option_ << "], value: [" << value_ << "]" << endl;
	}
	return ( true );
}


void version( void ) {
	cout << PACKAGE_STRING << ( setup._verbose ? " " COMMIT_ID : "" ) << endl;
	if ( setup._verbose ) {
		cout << "yaal " << yaal_version( true ) << endl;
	}
}

bool must_have_argument( HProgramOptionsHandler const& po_, char const* opt_ ) {
	M_PROLOG
	bool canHaveArgument( false );
	do {
		if ( *opt_ != '-' ) {
			break;
		}
		++ opt_;
		if ( *opt_ == '-' ) {
			++ opt_;
			for ( HProgramOptionsHandler::HOption const& opt : po_.get_options() ) {
				if (
					( opt_ == opt.long_form() )	&& ( opt.switch_type() == HProgramOptionsHandler::HOption::ARGUMENT::REQUIRED )
				) {
					canHaveArgument = true;
					break;
				}
			}
		} else {
			int len( static_cast<int>( strlen( opt_ ) ) );
			if ( len == 0 ) {
				break;
			}
			int optIdx( len - 1 );
			for ( HProgramOptionsHandler::HOption const& opt : po_.get_options() ) {
				if (
					( opt_[optIdx] == opt.short_form() ) && ( opt.switch_type() == HProgramOptionsHandler::HOption::ARGUMENT::REQUIRED )
				) {
					canHaveArgument = true;
					break;
				}
			}
		}
	} while ( false );
	return ( canHaveArgument );
	M_EPILOG
}

}

/* Set all the option flags according to the switches specified.
   Return the index of the first non-option argument.                    */
int handle_program_options( int argc_, char** argv_ ) {
	M_PROLOG
	HProgramOptionsHandler po;
	HOptionInfo info( po );
	info
		.name( setup._programName )
		.intro( "Huginn programming language executor" )
		.description(
			"The \\`huginn\\` program is an executor for Huginn programming language, it:\n\n"
			"- allows execution of Huginn scripts\n"
			"- can be used as a stream editor\n"
			"- provides an interactive REPL interface for the language\n"
			"- can work as Jupyter's kernel core\n"
			"- can be used as an interactive system shell\n"
			"- can generate *tags* and *documentation* from Huginn sources\n\n"
			"Executing \\`huginn\\` on a real terminal and without _script_ parameter"
			" or any mode switches (**-e**, **-J**) starts REPL interface."
		);
	bool help( false );
	bool conf( false );
	bool vers( false );
	OSetup::string_opt_t colorScheme;
	po(
		HProgramOptionsHandler::HOption()
		.short_form( 'a' )
		.long_form( "auto-split" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description( "turn on auto-split mode for stream editor (**-n**), field separtor can be set with **-F**" )
		.recipient( setup._autoSplit )
	)(
		HProgramOptionsHandler::HOption()
		.short_form( 'A' )
		.long_form( "alias-imports" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description( "use aliased form of `import` statement (`import Package as _alias_;`) in one-liner mode" )
		.recipient( setup._aliasImports )
	)(
		HProgramOptionsHandler::HOption()
		.long_form( "be-sloppy" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description( "disable strict correctness checks, e.g.: allow unused variables" )
		.recipient( setup._beSloppy )
	)(
		HProgramOptionsHandler::HOption()
		.long_form( "color-scheme" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::REQUIRED )
		.description( "color scheme to use for syntax highlighting" )
		.recipient( colorScheme )
		.argument_name( "name" )
	)(
		HProgramOptionsHandler::HOption()
		.short_form( 'c' )
		.long_form( "command" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::REQUIRED )
		.description( "one-liner program passed in as string" )
		.recipient( setup._program )
		.argument_name( "code" )
	)(
		HProgramOptionsHandler::HOption()
		.short_form( 'D' )
		.long_form( "dump-state" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description( "dump VM state to log file before execution" )
		.recipient( setup._dumpState )
	)(
		HProgramOptionsHandler::HOption()
		.short_form( 'e' )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::REQUIRED )
		.description( "an alias for **-c**, **--command**" )
		.recipient( setup._program )
		.argument_name( "code" )
	)(
		HProgramOptionsHandler::HOption()
		.short_form( 'E' )
		.long_form( "embedded" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description( "program is embedded in larger text, discard garbage until first line matching *^#!.\\*huginn.\\** is found" )
		.recipient( setup._embedded )
	)(
		HProgramOptionsHandler::HOption()
		.short_form( 'F' )
		.long_form( "field-separator" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::REQUIRED )
		.description( "set field separator for auto-split (**-a**) mode in stream editor (**-n**)" )
		.argument_name( "sep" )
		.recipient( setup._fieldSeparator )
	)(
		HProgramOptionsHandler::HOption()
		.long_form( "gen-docs" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::OPTIONAL )
		.description( "generate documentation from program source" )
		.argument_name( "path" )
		.setter(
			[]( HString const& value_ ) {
				setup._genDocs = ! value_.is_empty() ? value_ : "-";
			}
		)
	)(
		HProgramOptionsHandler::HOption()
		.short_form( 'h' )
		.long_form( "help" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description( "display this help and stop" )
		.recipient( help )
	)(
		HProgramOptionsHandler::HOption()
		.long_form( "history-file" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::REQUIRED )
		.description( "path to the file where history of interactive session should be stored" )
		.argument_name( "path" )
		.default_value( DEFAULT::HISTORY_PATH )
		.recipient( setup._historyPath )
	)(
		HProgramOptionsHandler::HOption()
		.short_form( 'i' )
		.long_form( "in-place" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::OPTIONAL )
		.description( "stream editor mode modifies input files \"in place\", possibly leaving backup files behind" )
		.argument_name( "bck" )
		.default_value( "" )
		.recipient( setup._inplace )
	)(
		HProgramOptionsHandler::HOption()
		.short_form( 'J' )
		.long_form( "jupyter" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description( "work as a backend for Jupyter kernel" )
		.recipient( setup._jupyter )
	)(
		HProgramOptionsHandler::HOption()
		.short_form( 'l' )
		.long_form( "chomp" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description(
			"strip new line characters from lines filtered by stream editor mode (**-n** or **-p**), in shell mode act as if invoked as a login shell"
		)
		.recipient( setup._chomp )
	)(
		HProgramOptionsHandler::HOption()
		.long_form( "lint" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description( "parse and compile program source to verify its static correctness but do not execute it" )
		.recipient( setup._lint )
	)(
		HProgramOptionsHandler::HOption()
		.short_form( 'L' )
		.long_form( "log-path" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::REQUIRED )
		.description( "path pointing to file for application logs" )
		.argument_name( "path" )
		.recipient(	setup._logPath )
	)(
		HProgramOptionsHandler::HOption()
		.short_form( 'M' )
		.long_form( "module-path" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::REQUIRED )
		.description( "a colon separated list of directories that should be searched for sub-modules" )
		.argument_name( "path1:path2:..." )
		.setter(
			[]( HString const& value_ ) {
				setup._modulePath = string::split<HHuginn::paths_t>( value_, ":", HTokenizer::SKIP_EMPTY );
			}
		)
		.getter(
			[]( void ) -> yaal::hcore::HString {
				return ( string::join( setup._modulePath, ":" ) );
			}
		)
	)(
		HProgramOptionsHandler::HOption()
		.short_form( 'n' )
		.long_form( "sed-n" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description(
			"filter all files (or standard input) through code given with **-e** _code_,"
			" like this _code_ would be inside `while ( ( _\\__ = input() ) != none ) { _code_ }` loop,"
			" analogous to **sed -n** or **perl -n**"
		)
		.recipient( setup._streamEditorSilent )
	)(
		HProgramOptionsHandler::HOption()
		.long_form( "native-lines" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description( "count only native lines of program source" )
		.recipient( setup._nativeLines )
	)(
		HProgramOptionsHandler::HOption()
		.long_form( "no-argv" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description( "do not pass program arguments to `main()` function" )
		.recipient( setup._noArgv )
	)(
		HProgramOptionsHandler::HOption()
		.long_form( "no-color" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description( "do not use colorful output" )
		.recipient( setup._noColor )
	)(
		HProgramOptionsHandler::HOption()
		.short_form( 'N' )
		.long_form( "no-default-imports" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description( "do not enable default imports in one-liner mode" )
		.recipient( setup._noDefaultImports )
	)(
		HProgramOptionsHandler::HOption()
		.long_form( "no-default-init" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description( "do not load default `init` file on startup" )
		.recipient( setup._noDefaultInit )
	)(
		HProgramOptionsHandler::HOption()
		.short_form( 'O' )
		.long_form( "optimize" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description( "optimize program execution by removing _assert_ statements" )
		.recipient( setup._optimize )
	)(
		HProgramOptionsHandler::HOption()
		.short_form( 'p' )
		.long_form( "sed" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description(
			"filter all files (or standard input) through code given with **-e** _code_,"
			" like this _code_ would be inside"
			" `while ( ( _\\__ = input() ) != none ) { _\\__ = _code_ ; print( \"{}\\\\n\".format( _\\__ ) ); }`"
			" loop, analogous to **sed** or **perl -p**"
		)
		.recipient( setup._streamEditor )
	)(
		HProgramOptionsHandler::HOption()
		.short_form( 'q' )
		.long_form( "quiet" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description( "inhibit usual output" )
		.recipient( setup._quiet )
	)(
		HProgramOptionsHandler::HOption()
		.short_form( 'q' )
		.long_form( "silent" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description( "inhibit usual output" )
		.recipient( setup._quiet )
	)(
		HProgramOptionsHandler::HOption()
		.long_form( "rapid-start" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description( "disable language grammar verification normally performed once per runner start" )
		.recipient( setup._rapidStart )
	)(
		HProgramOptionsHandler::HOption()
		.short_form( 's' )
		.long_form( "shell" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::OPTIONAL )
		.description( "execute shell commands from interactive mode" )
		.argument_name( "path" )
		.default_value( "" )
		.recipient(	setup._shell )
	)(
		HProgramOptionsHandler::HOption()
		.short_form( 'S' )
		.long_form( "session" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::REQUIRED )
		.description( "use session persistence, load previous session on startup, save current session on exit" )
		.argument_name( "name" )
		.default_value( "default" )
		.recipient(	setup._session )
	)(
		HProgramOptionsHandler::HOption()
		.long_form( "session-directory" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::REQUIRED )
		.description( "directory for storing session persistence files" )
		.argument_name( "path" )
		.default_value( "${" HOME_ENV_VAR "}/.huginn/" )
		.recipient(	setup._sessionDir )
	)(
		HProgramOptionsHandler::HOption()
		.short_form( 't' )
		.long_form( "tags" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description( "generate tags file" )
		.recipient( setup._tags )
	)(
		HProgramOptionsHandler::HOption()
		.short_form( 'v' )
		.long_form( "verbose" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description( "print more information" )
		.recipient( setup._verbose )
	)(
		HProgramOptionsHandler::HOption()
		.short_form( 'V' )
		.long_form( "version" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description( "output version information and stop" )
		.recipient( vers )
	)(
		HProgramOptionsHandler::HOption()
		.short_form( 'W' )
		.long_form( "dump-configuration" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description( "dump current configuration" )
		.recipient( conf )
	);
	/*
	 * Find where huginn program parameters begin,
	 * as opposed to where huginn executor parameters begin.
	 *
	 * huginn --be-sloppy -v script.hgn --option
	 *        ^
	 *        |              ^
	 *        |              |
	 *        |              +-- huginn program parameters
	 *        |
	 *        +-- huginn executor parameters
	 */
	int argc( argc_ );
	for ( int i( 1 ); i < argc_; ++ i ) {
		if (
			( ( argv_[i][0] == '-' ) && ( argv_[i][1] == '\0' ) )
			|| ( ( argv_[i][0] != '-' ) && ! must_have_argument( po, argv_[i - 1] ) )
		) {
			argc = i;
			break;
		}
	}
	if ( setup.is_system_shell() ) {
		setup._shell = HString();
	}
	po.process_rc_file( "", set_variables );
	if ( !! colorScheme ) {
		setup._colorScheme = *colorScheme;
		setup._colorSchemeSource = SETTING_SOURCE::RC;
		OSetup::string_opt_t().swap( colorScheme );
	}
	int unknown( 0 );
	po.process_command_line( argc, argv_, &unknown );
	if ( !! colorScheme ) {
		setup._colorScheme = *colorScheme;
		setup._colorSchemeSource = SETTING_SOURCE::COMMAND_LINE;
	}
	if ( help || conf || vers || ( unknown > 0 ) ) {
		if ( help || ( unknown > 0 ) ) {
			info.color( ! setup._noColor ).markdown( setup._verbose );
			util::show_help( info );
		} else if ( conf ) {
			util::dump_configuration( info );
		} else if ( vers ) {
			version();
		}
		HLog::disable_auto_rehash();
		throw unknown;
	}
	return ( argc );
	M_EPILOG
}


}

