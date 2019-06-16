/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <yaal/hcore/base.hxx>
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
	, _modulePath() {
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
				set_color_scheme( value );
				setup._colorScheme = value;
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

rt_settings_t rt_settings( void ) {
	rt_settings_t rts( {
		{ "max_call_stack_size", to_string( settingsObserver._maxCallStackSize ) },
		{ "error_context", error_context_to_string( setup._errorContext ) },
		{ "session", setup._session },
		{ "module_path", string::join( settingsObserver._modulePath, ":" ) }
	} );
	if ( ! setup._colorScheme.is_empty() ) {
		rts.insert( make_pair( "color_scheme", setup._colorScheme ) );
	}
	if ( setup._prompt != setup.default_prompt() ) {
		rts.insert( make_pair( !! setup._shell ? "shell_prompt" : "prompt", setup._prompt ) );
	}
	return ( rts );
}

}

