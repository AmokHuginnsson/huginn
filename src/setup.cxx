/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <cstdio>

#include <yaal/tools/util.hxx>
#include <yaal/hcore/duration.hxx>
#include <yaal/hcore/hfile.hxx>
#include <yaal/hcore/hcore.hxx>
M_VCSID( "$Id: " __ID__ " $" )
#include "setup.hxx"
#include "colorize.hxx"

using namespace yaal::hcore;

namespace huginn {

char const VOLATILE_PROMPT_INFO_VAR_NAME[] = "VOLATILE_PROMPT_INFO";

namespace DEFAULT {

char const* HISTORY_PATH = "${" HOME_ENV_VAR "}/.huginn_history";

}

namespace {
static time::duration_t const CENTURY( time::duration( 520, time::UNIT::WEEK ) );
}

yaal::i64_t const OSetup::CENTURY_IN_MILLISECONDS( time::in_units<time::UNIT::MILLISECOND>( CENTURY ) );

OSetup::OSetup( void )
	: _quiet( false )
	, _verbose( false )
	, _dumpState( false )
	, _embedded( false )
	, _lint( false )
	, _tags( false )
	, _nativeLines( false )
	, _rapidStart( false )
	, _interactive( false )
	, _jupyter( false )
	, _noDefaultImports( false )
	, _noDefaultInit( false )
	, _noArgv( false )
	, _beSloppy( false )
	, _optimize( false )
	, _noColor( false )
	, _streamEditor( false )
	, _streamEditorSilent( false )
	, _chomp( false )
	, _autoSplit( false )
	, _aliasImports( false )
	, _colorSchemeSource( SETTING_SOURCE::NONE )
	, _errorContext( ERROR_CONTEXT::SHORT )
	, _inplace()
	, _program()
	, _shell()
	, _prompt()
	, _colorScheme()
	, _fieldSeparator( " " )
	, _session()
	, _sessionDir()
	, _modulePath()
	, _historyPath()
	, _genDocs()
	, _programName( nullptr )
	, _logPath() {
	return;
}

void OSetup::test_setup( int argc_ ) {
	M_PROLOG
	int errNo( 1 );
	if ( ( argc_ == 0 ) && ! ( _jupyter || _program || _streamEditor || _streamEditorSilent ) ) {
		_interactive = true;
	}
	if ( _quiet && _verbose ) {
		yaal::tools::util::failure( errNo,
			_( "quiet and verbose options are exclusive\n" )
		);
	}
	++ errNo;
	if ( _verbose ) {
		clog.reset_owned( make_pointer<HFile>( stdout, HFile::OWNERSHIP::EXTERNAL ) );
	}
	if ( _nativeLines && ! _embedded ) {
		yaal::tools::util::failure( errNo,
			_( "native lines makes sense only with embedded\n" )
		);
	}
	++ errNo;
	if ( _lint && _beSloppy ) {
		yaal::tools::util::failure( errNo,
			_( "lint and be-sloppy options are mutually exclusive\n" )
		);
	}
	++ errNo;
	if ( _interactive && ( _lint || _embedded || _nativeLines || _streamEditor || _streamEditorSilent ) ) {
		yaal::tools::util::failure( errNo,
			_( "interactive mode is exclusive with other modes of operation\n" )
		);
	}
	++ errNo;
	if ( _program && ( _interactive || _lint || _embedded || _nativeLines || _jupyter ) ) {
		yaal::tools::util::failure( errNo,
			_( "one-liner code mode is exclusive with other modes of operation\n" )
		);
	}
	++ errNo;
	if ( _jupyter && ( _interactive || _lint || _embedded || _nativeLines || _streamEditor || _streamEditorSilent ) ) {
		yaal::tools::util::failure( errNo,
			_( "Jupyter backend mode is exclusive with other modes of operation\n" )
		);
	}
	++ errNo;
	if ( _interactive && _beSloppy ) {
		yaal::tools::util::failure( errNo,
			_( "sloppy compiler mode is always selected for interactive mode\n" )
		);
	}
	++ errNo;
	if ( _program && _beSloppy ) {
		yaal::tools::util::failure( errNo,
			_( "sloppy compiler mode is always selected for one-liner code mode\n" )
		);
	}
	++ errNo;
	if ( _noDefaultImports && ! _program ) {
		yaal::tools::util::failure( errNo,
			_( "default imports setting can be only used in one-liner mode\n" )
		);
	}
	++ errNo;
	if ( _noDefaultInit && ! ( _interactive || _jupyter ) ) {
		yaal::tools::util::failure( errNo,
			_( "default `init` setting can be only used in interactive or Jupyter mode\n" )
		);
	}
	++ errNo;
	if ( ! _genDocs.is_empty() && ( _interactive || _program || _lint || _jupyter ) ) {
		yaal::tools::util::failure( errNo,
			_( "gens-docs is not usable with real execution mode nor with lint mode\n" )
		);
	}
	++ errNo;
	if ( _noColor && ! _colorScheme.is_empty() ) {
		yaal::tools::util::failure( errNo,
			_( "setting color scheme makes no sense with disabled colors\n" )
		);
	}
	++ errNo;
	if ( _streamEditor && _streamEditorSilent ) {
		yaal::tools::util::failure( errNo,
			_( "stream editor mode (**-p**) and silent stream editor mode (**-n**) are mutually exclusive\n" )
		);
	}
	++ errNo;
	if ( ( _streamEditor || _streamEditorSilent ) && ! _program ) {
		yaal::tools::util::failure( errNo,
			_( "stream editor mode (**-p**) or silent stream editor mode (**-n**) require program code (**-e**)\n" )
		);
	}
	++ errNo;
	if ( _inplace && ! ( _streamEditor || _streamEditorSilent ) ) {
		yaal::tools::util::failure( errNo,
			_( "in-place (**-i**) switch makes sense only for stream editor mode (**-p**) or silent stream editor mode (**-n**)\n" )
		);
	}
	++ errNo;
	if ( _inplace && ( argc_ == 0 ) ) {
		yaal::tools::util::failure( errNo,
			_( "in-place (**-i**) switch makes sense only when files to be processed are present\n" )
		);
	}
	++ errNo;
	if ( _autoSplit && ! ( _streamEditor || _streamEditorSilent ) ) {
		yaal::tools::util::failure( errNo,
			_( "auto-split (**-a**) switch makes sense only for stream editor mode (**-n**)\n" )
		);
	}
	++ errNo;
	if ( _tags && ( _interactive || _jupyter || _lint || _streamEditor || _streamEditorSilent || _program || ! _genDocs.is_empty() ) ) {
		yaal::tools::util::failure( errNo,
			_( "tags (**-t**) switch is exclusive with other mode switches\n" )
		);
	}
	++ errNo;
	if ( _aliasImports && ! _program ) {
		yaal::tools::util::failure( errNo,
			_( "alias-imports (**-A**) switch makes sense only in one-liner mode\n" )
		);
	}
	/* Normalize switches. */
	if ( _streamEditorSilent ) {
		_streamEditorSilent = false;
		_streamEditor = true;
		_quiet = true;
	}
	try {
		++ errNo;
		set_color_scheme( ! _colorScheme.is_empty() ? _colorScheme : "dark-background" );
	} catch ( HException const& ) {
		yaal::tools::util::failure( errNo,
			_( "invalid color scheme\n" )
		);
	}
	/*
	 * black        kK
	 * red          rR
	 * green        gG
	 * brown/yellow yY
	 * blue         bB
	 * magenta      mM
	 * cyan         cC
	 * white        wW
	 * bold         *
	 * underline    _
	 * prompt       pP
	 * reset        x
	 * history no   i
	 * login        lnu
	 * host         hH
	 * working dir  ~
	 * privileges   #
	 */
	if ( _prompt.is_empty() ) {
		_prompt = default_prompt();
	}
	if ( is_system_shell() && ( _programName[0] == '-' ) ) {
		_chomp = true;
	}
	return;
	M_EPILOG
}

yaal::hcore::HString OSetup::default_prompt( void ) const {
	HString prompt;
	if ( ! _shell ) {
		prompt.assign( "%phuginn[%P%i%p]>%x " );
	} else {
		char color( ( getenv( "REMOTEHOST" ) || getenv( "REMOTE_HOST" ) || getenv( "SSH_CONNECTION" ) || getenv( "SSH_CLIENT" ) ) ? 'Q' : 'q' );
		prompt.assign( "[%" ).append( color ).append( "%l@%h%x]${TERM_ID}%B%~%x${" ).append( VOLATILE_PROMPT_INFO_VAR_NAME ).append( "}%# " );
	}
	return ( prompt );
}

bool OSetup::is_system_shell( void ) const {
	HString pn( _programName );
	char const huginnShell[] = "/huginn-shell";
	char const hgnsh[] = "/hgnsh";
	char const huginnShellLogin[] = "-huginn-shell";
	char const hgnshLogin[] = "-hgnsh";
	return (
		( pn == ( huginnShell + 1 ) )
		|| ( pn == ( hgnsh + 1 ) )
		|| ( pn.ends_with( huginnShell ) )
		|| ( pn.ends_with( hgnsh ) )
		|| ( pn == huginnShellLogin )
		|| ( pn == hgnshLogin )
	);
}

}

