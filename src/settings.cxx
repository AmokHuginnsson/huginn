/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <cstdlib>

#include <yaal/hcore/base.hxx>
#include <yaal/hcore/system.hxx>
#include <yaal/tools/tools.hxx>
#include <yaal/tools/stringalgo.hxx>
#include <yaal/tools/filesystem.hxx>
M_VCSID( "$Id: " __ID__ " $" )
M_VCSID( "$Id: " __TID__ " $" )
#include "settings.hxx"
#include "setup.hxx"
#include "colorize.hxx"

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;

namespace huginn {

OSettingObserver settingsObserver;

OSettingObserver::OSettingObserver( void )
	: _maxCallStackSize( _huginnMaxCallStack_ )
	, _modulePath()
	, _colorScheme() {
}

void apply_setting( yaal::tools::HHuginn& huginn_, yaal::hcore::HString const& setting_ ) {
	M_PROLOG
	HString setting( setting_ );
	int long sepIdx( setting.find( '='_ycp ) );
	if ( sepIdx != HString::npos ) {
		HString name( setting.left( sepIdx ) );
		HString value( setting.mid( sepIdx + 1 ) );
		name.trim();
		if ( ( name != "prompt" ) && ( name != "shell_prompt" ) ) {
			value.trim();
		}
		if ( name == "max_call_stack_size" ) {
			huginn_.set_max_call_stack_size( settingsObserver._maxCallStackSize = lexical_cast<int>( value ) );
		} else if ( name == "error_context" ) {
			if ( value == "hidden" ) {
				setup._errorContext = ERROR_CONTEXT::HIDDEN;
			} else if ( value == "visible" ) {
				setup._errorContext = ERROR_CONTEXT::VISIBLE;
			} else if ( value == "short" ) {
				setup._errorContext = ERROR_CONTEXT::SHORT;
			} else {
				throw HRuntimeException( "unknown error_context setting: "_ys.append( value ) );
			}
		} else if ( name == "color_scheme" ) {
			try {
				if ( setup._colorSchemeSource != SETTING_SOURCE::COMMAND_LINE ) {
					set_color_scheme( value );
					setup._colorSchemeSource = SETTING_SOURCE::SESSION;
				}
				settingsObserver._colorScheme = value;
			} catch ( HException const& ) {
				throw HRuntimeException( "unknown color scheme setting: "_ys.append( value ) );
			}
		} else if ( name == "session" ) {
			HString path( setup._sessionDir + "/" + value );
			if ( ! value.is_empty() ) {
				if ( filesystem::exists( path ) && ! filesystem::is_regular_file( path ) ) {
					throw HRuntimeException( "Given session file is not a regular file: "_ys.append( path ) );
				}
			}
			setup._session = value;
		} else if ( name == "module_path" ) {
			settingsObserver._modulePath = string::split( value, ":" );
			for ( HString& p : settingsObserver._modulePath ) {
				if ( p.find( "~/" ) == 0 ) {
					p.replace( 0, 1, "${HOME}" );
				}
			}
			for ( yaal::tools::filesystem::path_t const& path : setup._modulePath ) {
				if ( find( settingsObserver._modulePath.begin(), settingsObserver._modulePath.end(), path ) == settingsObserver._modulePath.end() ) {
					settingsObserver._modulePath.push_back( path );
				}
			}
		} else if ( name == "prompt" ) {
			if ( ! setup._shell ) {
				setup._prompt = value;
			}
		} else if ( name == "shell_prompt" ) {
			if ( !! setup._shell ) {
				setup._prompt = value;
			}
		} else {
			throw HRuntimeException( "unknown setting: "_ys.append( name ) );
		}
	} else {
		throw HRuntimeException( "unknown setting: "_ys.append( setting ) );
	}
	return;
	M_EPILOG
}

inline char const* error_context_to_string( ERROR_CONTEXT errorContext_ ) {
	char const* ecs( "invalid" );
	switch ( errorContext_ ) {
		case ( ERROR_CONTEXT::VISIBLE ): ecs = "visible"; break;
		case ( ERROR_CONTEXT::HIDDEN ):  ecs = "hidden";  break;
		case ( ERROR_CONTEXT::SHORT ):   ecs = "short";   break;
	}
	return ( ecs );
}

rt_settings_t rt_settings( bool all_ ) {
	rt_settings_t rts( {
		{ "max_call_stack_size", to_string( settingsObserver._maxCallStackSize ) },
		{ "error_context", error_context_to_string( setup._errorContext ) },
		{ "session", setup._session },
		{ "module_path", string::join( settingsObserver._modulePath, ":" ) }
	} );
	if ( ! settingsObserver._colorScheme.is_empty() ) {
		rts.insert( make_pair( "color_scheme", settingsObserver._colorScheme ) );
	} else if ( ( setup._colorSchemeSource != SETTING_SOURCE::COMMAND_LINE ) && ! setup._colorScheme.is_empty() ) {
		rts.insert( make_pair( "color_scheme", setup._colorScheme ) );
	}
	if ( all_ || ( setup._prompt != setup.default_prompt() ) ) {
		rts.insert( make_pair( !! setup._shell ? "shell_prompt" : "prompt", setup._prompt ) );
	}
	return ( rts );
}

yaal::tools::filesystem::path_t make_conf_path( char const* name_ ) {
	M_PROLOG
	HString envName( "HUGINN_" );
	envName.append( name_ ).replace( ".", "_" ).upper();
	HUTF8String utf8( envName );
	char const* SCRIPT_PATH( getenv( utf8.c_str() ) );
	filesystem::path_t initPath;
	if ( SCRIPT_PATH ) {
		initPath.assign( SCRIPT_PATH );
	} else {
		filesystem::paths_t trialPaths;
		filesystem::path_t trial;
		/* session dir */
		trial.assign( setup._sessionDir ).append( "/" ).append( name_ );
		trialPaths.push_back( yaal::move( trial ) );
		/* global system configuration */
		trial.assign( SYSCONFDIR ).append( "/huginn/" ).append( name_ );
		trialPaths.push_back( yaal::move( trial ) );
#ifdef __MSVCXX__
		/* located along side an executable file */
		trial.assign( system::get_self_exec_path() ).replace( "\\", "/" );
		filesystem::path_t::size_type sepPos( trial.find_last( '/'_ycp ) );
		if ( sepPos != filesystem::path_t::npos ) {
			trial.erase( sepPos + 1 ).append( name_ );
			trialPaths.push_back( yaal::move( trial ) );
		}
#	ifdef __DEBUG__
		if ( ! _projectRoot_.is_empty() ) {
			trial.assign( _projectRoot_ ).append( "/src/" ).append( name_ );
			trialPaths.push_back( trial );
		}
#	endif
#endif
		for ( filesystem::path_t const& tp : trialPaths ) {
			if ( filesystem::exists( tp ) ) {
				initPath.assign( tp );
				break;
			}
#ifdef __DEBUG__
			trial.assign( tp ).append( ".sample" );
			if ( filesystem::exists( trial ) ) {
				initPath.assign( trial );
				break;
			}
#endif
		}
	}
	return ( initPath );
	M_EPILOG
}

}

