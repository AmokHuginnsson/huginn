/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <yaal/hcore/hcore.hxx>
#include <yaal/hcore/system.hxx>
#include <yaal/tools/filesystem.hxx>

#include "config.hxx"

#ifdef USE_REPLXX
static char const REPL_ignore_start[] = "";
static char const REPL_ignore_end[] = "";
#elif defined( USE_EDITLINE )
static char const REPL_ignore_start[] = { 1, 0 };
static char const REPL_ignore_end[] = { 1, 0 };
#else
#	include <readline/readline.h>
#	include <readline/history.h>
static char const REPL_ignore_start[] = { RL_PROMPT_START_IGNORE, 0 };
static char const REPL_ignore_end[] = { RL_PROMPT_END_IGNORE, 0 };
#undef SPACE
#endif

#include <yaal/hcore/macro.hxx>
M_VCSID( "$Id: " __ID__ " $" )
M_VCSID( "$Id: " __TID__ " $" )
#include "prompt.hxx"
#include "setup.hxx"
#include "colorize.hxx"
#include "quotes.hxx"
#include "systemshell.hxx"

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;
using namespace yaal::tools::filesystem;
using namespace yaal::tools::huginn;

namespace huginn {

HPromptRenderer::HPromptRenderer( void )
	: _lineNo( 0 )
	, _clock()
	, _buffer()
	, _utf8ConversionCache() {
	return;
}

HPromptRenderer::~HPromptRenderer( void ) {
	return;
}

namespace {

inline void condColor( hcore::HString& prompt_, char const* color_ ) {
	if ( ! setup._noColor ) {
		prompt_.append( REPL_ignore_start ).append( color_ ).append( REPL_ignore_end );
	}
	return;
}

}

void HPromptRenderer::make_prompt( yaal::hcore::HString const* promptTemplate_, HSystemShell* shell_ ) {
	M_PROLOG
	hcore::HString promptTemplate( *promptTemplate_ );
	substitute_environment( promptTemplate, ENV_SUBST_MODE::RECURSIVE );
	bool special( false );
	_buffer.clear();
	for ( code_point_t cp : promptTemplate ) {
		if ( cp == '%' ) {
			special = true;
			continue;
		}
		if ( ! special ) {
			_buffer.push_back( cp );
			continue;
		}
		switch ( cp.get() ) {
			case ( 'k' ): condColor( _buffer, *ansi::black );         break;
			case ( 'K' ): condColor( _buffer, *ansi::gray );          break;
			case ( 'r' ): condColor( _buffer, *ansi::red );           break;
			case ( 'R' ): condColor( _buffer, *ansi::brightred );     break;
			case ( 'g' ): condColor( _buffer, *ansi::green );         break;
			case ( 'G' ): condColor( _buffer, *ansi::brightgreen );   break;
			case ( 'y' ): condColor( _buffer, *ansi::brown );         break;
			case ( 'Y' ): condColor( _buffer, *ansi::yellow );        break;
			case ( 'b' ): condColor( _buffer, *ansi::blue );          break;
			case ( 'B' ): condColor( _buffer, *ansi::brightblue );    break;
			case ( 'm' ): condColor( _buffer, *ansi::magenta );       break;
			case ( 'M' ): condColor( _buffer, *ansi::brightmagenta ); break;
			case ( 'c' ): condColor( _buffer, *ansi::cyan );          break;
			case ( 'C' ): condColor( _buffer, *ansi::brightcyan );    break;
			case ( 'w' ): condColor( _buffer, *ansi::lightgray );     break;
			case ( 'W' ): condColor( _buffer, *ansi::white );         break;
			case ( '*' ): condColor( _buffer, *ansi::bold );          break;
			case ( '_' ): condColor( _buffer, *ansi::underline );     break;
			case ( 'p' ): condColor( _buffer, ansi_color( GROUP::PROMPT ) );      break;
			case ( 'P' ): condColor( _buffer, ansi_color( GROUP::PROMPT_MARK ) ); break;
			case ( 'q' ): condColor( _buffer, ansi_color( GROUP::LOCAL_HOST ) );  break;
			case ( 'Q' ): condColor( _buffer, ansi_color( GROUP::REMOTE_HOST ) ); break;
			case ( 'x' ): condColor( _buffer, *ansi::reset );         break;
			case ( '%' ): _buffer.append( "%" ); break;
			case ( 'i' ): _buffer.append( _lineNo ); break;
			case ( 'l' ): /* fall through */
			case ( 'n' ): /* fall through */
			case ( 'u' ): _buffer.append( system::get_user_name( system::get_user_id() ) ); break;
			case ( 'h' ): {
				hcore::HString h( system::get_host_name() );
				int long dotPos( h.find( '.'_ycp ) );
				if ( dotPos != hcore::HString::npos ) {
					h.erase( dotPos );
				}
				_buffer.append( h );
			} break;
			case ( 'H' ): _buffer.append( system::get_host_name() ); break;
			case ( 't' ): _buffer.append( now_local().set_format( _iso8601TimeFormat_ ).string() ); break;
			case ( 'd' ): _buffer.append( now_local().set_format( _iso8601DateFormat_ ).string() ); break;
			case ( 'D' ): _buffer.append( now_local().set_format( _iso8601DateTimeFormat_ ).string() ); break;
			case ( 'j' ): {
				if ( ! shell_ ) {
					break;
				}
				_buffer.append( shell_->job_count() );
			} break;
			case ( 'T' ): {
				time::duration_t d( _clock.get_time_elapsed( time::UNIT::NANOSECOND ) );
				_buffer.append( time::duration_to_string( d, time::scale( d), time::UNIT_FORM::ABBREVIATED ) );
			} break;
			case ( '#' ): _buffer.append( "$" ); break;
			case ( '~' ): {
				char const* PWD( ::getenv( "PWD" ) );
				filesystem::path_t curDir( PWD ? PWD : filesystem::current_working_directory() );
				hcore::HString homePath( system::home_path() );
#ifdef __MSVCXX__
				homePath.lower().replace( "\\", "/" );
				curDir.lower().replace( "\\", "/" );
#endif
				if ( curDir.starts_with( homePath ) ) {
					curDir.replace( 0, homePath.get_length(), "~" );
				}
				_buffer.append( curDir ).append( "/" );
			} break;
		}
		special = false;
	}
	_buffer = unescape_system( yaal::move( _buffer ) );
	return;
	M_EPILOG
}

yaal::hcore::HString const& HPromptRenderer::rendered_prompt( void ) const {
	return ( _buffer );
}

HPrompt::HPrompt( void )
	: HPromptRenderer()
	, _repl() {
	M_PROLOG
	static int const DEFAULT_MAX_HISTORY_SIZE( 4000 );
	_repl.set_max_history_size( DEFAULT_MAX_HISTORY_SIZE );
	M_EPILOG
}

HPrompt::~HPrompt( void ) {
	M_PROLOG
	return;
	M_DESTRUCTOR_EPILOG
}

HRepl& HPrompt::repl( void ) {
	return ( _repl );
}

bool HPrompt::input( yaal::hcore::HString& line_, yaal::hcore::HString const* promptTemplate_ ) {
	make_prompt( promptTemplate_ ? promptTemplate_ : &setup._prompt, dynamic_cast<HSystemShell*>( _repl.shell() ) );
	_utf8ConversionCache.assign( _buffer );
	bool gotInput( _repl.input( line_, _utf8ConversionCache.c_str() ) );
	if ( gotInput && ! line_.is_empty() ) {
		++ _lineNo;
	}
	_clock.reset();
	return ( gotInput );
}

}

