/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <cstdlib>
#include <cstdio>
#include <cstring>

#include "config.hxx"

#include <yaal/hcore/hprogramoptionshandler.hxx>
#include <yaal/hcore/hlog.hxx>
#include <yaal/hcore/hcore.hxx>
#include <yaal/tools/stringalgo.hxx>
#include <yaal/tools/util.hxx>
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

bool can_have_argument( HProgramOptionsHandler const& po_, HString opt_ ) {
	M_PROLOG
	bool canHaveArgument( false );
	opt_.trim_left( "-" );
	for ( HProgramOptionsHandler::HOption const& opt : po_.get_options() ) {
		if (
			( ( opt_ == opt.long_form() ) || ( ! opt_.is_empty() && ( static_cast<char>( opt_.front().get() ) == opt.short_form() ) ) )
			&& ( opt.switch_type() != HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		) {
			canHaveArgument = true;
			break;
		}
	}
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
			"The \\`huginn\\` program is an executor for Huginn programming language,"
			" it allows execution of Huginn scripts, it provides an interactive REPL interface"
			" for the language, it also works as Jupyter's kernel core."
		);
	bool help( false );
	bool conf( false );
	bool vers( false );
	bool brightBackground( false );
	po(
		HProgramOptionsHandler::HOption()
		.long_form( "log-path" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::REQUIRED )
		.description( "path pointing to file for application logs" )
		.argument_name( "path" )
		.default_value( "huginn.log" )
		.recipient(	setup._logPath )
	)(
		HProgramOptionsHandler::HOption()
		.short_form( 'L' )
		.long_form( "generate-logs" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description( "generate log file" )
		.recipient( setup._generateLogs )
	)(
		HProgramOptionsHandler::HOption()
		.short_form( 'D' )
		.long_form( "dump-state" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description( "dump VM state to log file before execution" )
		.recipient( setup._dumpState )
	)(
		HProgramOptionsHandler::HOption()
		.long_form( "lint" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description( "parse and compile program source to verify its static correctness but do not execute it" )
		.recipient( setup._lint )
	)(
		HProgramOptionsHandler::HOption()
		.long_form( "rapid-start" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description( "disable language grammar verification normally performed once per runner start" )
		.recipient( setup._rapidStart )
	)(
		HProgramOptionsHandler::HOption()
		.long_form( "no-argv" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description( "do not pass program arguments to `main()` function" )
		.recipient( setup._noArgv )
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
		.long_form( "be-sloppy" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description( "disable strict correctness checks, e.g.: allow unused variables" )
		.recipient( setup._beSloppy )
	)(
		HProgramOptionsHandler::HOption()
		.short_form( 'E' )
		.long_form( "embedded" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description( "program is embedded in larger text, discard garbage until first line matching *^#!.\\*huginn.\\** is found" )
		.recipient( setup._embedded )
	)(
		HProgramOptionsHandler::HOption()
		.long_form( "native-lines" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description( "count only native lines of program source" )
		.recipient( setup._nativeLines )
	)(
		HProgramOptionsHandler::HOption()
		.short_form( 'i' )
		.long_form( "interactive" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description( "start interactive mode, run all lines entered so far as it would be surrounded by `main()` loop" )
		.recipient( setup._interactive )
	)(
		HProgramOptionsHandler::HOption()
		.short_form( 'J' )
		.long_form( "jupyter" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description( "work as a backend for Jupyter kernel" )
		.recipient( setup._jupyter )
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
		.short_form( 'c' )
		.long_form( "command" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::REQUIRED )
		.description( "one-liner program passed in as string" )
		.recipient( setup._program )
		.setter(
			[]( HString const& program_ ) {
				setup._program = program_;
				setup._hasProgram = true;
				return;
			}
		)
		.argument_name( "code" )
	)(
		HProgramOptionsHandler::HOption()
		.short_form( 'N' )
		.long_form( "no-default-imports" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description( "do not enable default imports in one-liner mode" )
		.recipient( setup._noDefaultImports )
	)(
		HProgramOptionsHandler::HOption()
		.long_form( "no-color" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description( "do not use colorful output" )
		.recipient( setup._noColor )
	)(
		HProgramOptionsHandler::HOption()
		.long_form( "bright-background" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description( "terminal uses bright background, use dark color theme" )
		.recipient( brightBackground )
	)(
		HProgramOptionsHandler::HOption()
		.long_form( "history-file" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::REQUIRED )
		.description( "path to the file where history of interactive session should be stored" )
		.argument_name( "path" )
		.default_value( "${" HOME_ENV_VAR "}/.huginn_history" )
		.recipient( setup._historyPath )
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
		.short_form( 'v' )
		.long_form( "verbose" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description( "print more information" )
		.recipient( setup._verbose )
	)(
		HProgramOptionsHandler::HOption()
		.short_form( 'h' )
		.long_form( "help" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description( "display this help and stop" )
		.recipient( help )
	)(
		HProgramOptionsHandler::HOption()
		.short_form( 'W' )
		.long_form( "dump-configuration" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description( "dump current configuration" )
		.recipient( conf )
	)(
		HProgramOptionsHandler::HOption()
		.short_form( 'V' )
		.long_form( "version" )
		.switch_type( HProgramOptionsHandler::HOption::ARGUMENT::NONE )
		.description( "output version information and stop" )
		.recipient( vers )
	);
	int argc( argc_ );
	for ( int i( 1 ); i < argc_; ++ i ) {
		if (
			( ( argv_[i][0] == '-' ) && ( argv_[i][1] == '\0' ) )
			|| ( ( argv_[i][0] != '-' ) && ( ( i == 0 ) || ! can_have_argument( po, argv_[i - 1] ) ) )
		) {
			argc = i;
			break;
		}
	}
	po.process_rc_file( "", set_variables );
	int unknown( 0 );
	po.process_command_line( argc, argv_, &unknown );
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
	setup._background = brightBackground ? BACKGROUND::LIGHT : BACKGROUND::DARK;
	return ( argc );
	M_EPILOG
}


}

