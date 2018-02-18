/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <yaal/hcore/hcore.hxx>
#include <yaal/hcore/hfile.hxx>
#include <yaal/tools/ansi.hxx>
#include <yaal/tools/stringalgo.hxx>
#include <yaal/tools/huginn/packagefactory.hxx>

#include <cstring>
#include <cstdio>

#include "config.hxx"

#ifdef USE_REPLXX
#	define REPL_ignore_start ""
#	define REPL_ignore_end ""
#elif defined( USE_EDITLINE )
#	define REPL_ignore_start ""
#	define REPL_ignore_end ""
#else
#	include <readline/readline.h>
#	include <readline/history.h>
static char const REPL_ignore_start[] = { RL_PROMPT_START_IGNORE, 0 };
static char const REPL_ignore_end[] = { RL_PROMPT_END_IGNORE, 0 };
#endif

M_VCSID( "$Id: " __ID__ " $" )
#include "interactive.hxx"
#include "linerunner.hxx"
#include "meta.hxx"
#include "setup.hxx"
#include "colorize.hxx"
#include "symbolicnames.hxx"
#include "shell.hxx"
#include "quotes.hxx"
#include "repl.hxx"
#include "settings.hxx"

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;
using namespace yaal::tools::huginn;

namespace yaal { namespace tools { namespace huginn {
bool is_builtin( yaal::hcore::HString const& );
}}}

namespace huginn {

namespace {

HLineRunner::words_t completion_words( yaal::hcore::HString&& context_, yaal::hcore::HString&& prefix_, void* data_ ) {
	M_PROLOG
	HRepl* repl( static_cast<HRepl*>( data_ ) );
	HLineRunner::words_t completions;
	do {
		context_.trim_left();
		int long dotIdx( prefix_.find_last( '.'_ycp ) );
		int long backSlashIdx( prefix_.find_last( '\\'_ycp ) );
		if ( ( backSlashIdx != HString::npos ) && ( ( dotIdx == HString::npos ) || ( backSlashIdx > dotIdx ) ) ) {
			HString symbolPrefix( prefix_.substr( backSlashIdx ) );
			char const* symbolicName( symbol_from_name( symbolPrefix ) );
			if ( symbolicName ) {
				completions.push_back( symbolicName );
				break;
			} else {
				symbolic_names_t sn( symbol_name_completions( symbolPrefix ) );
				if ( ! sn.is_empty() ) {
					for ( yaal::hcore::HString const& n : sn ) {
						completions.push_back( n );
					}
					break;
				}
			}
		}
		if ( in_quotes( context_ ) ) {
			break;
		}
		static HString const import( "import" );
		if ( context_.find( "import" ) == 0 ) {
			tools::huginn::HPackageFactory& pf( tools::huginn::HPackageFactory::get_instance() );
			for ( tools::huginn::HPackageFactory::creators_t::value_type const& p : pf ) {
				if ( p.first.find( prefix_ ) == 0 ) {
					completions.push_back( p.first + " as " );
				}
			}
		} else if ( import.find( context_ ) == 0 ) {
			completions.push_back( "import " );
		} if ( context_.find( "//" ) == 0 ) {
			if ( context_.find( "//set " ) == 0 ) {
				HString symbolPrefix( context_.substr( 6 ) );
				for ( rt_settings_t::value_type const& s : rt_settings() ) {
					if ( symbolPrefix.is_empty() || ( s.first.find( symbolPrefix ) == 0 ) ) {
						completions.push_back( to_string( s.first ).append( '=' ) );
					}
				}
				break;
			} else if ( context_.find( "//doc " ) == HString::npos ) {
				HString symbolPrefix( context_.substr( 2 ) );
				for ( yaal::hcore::HString const& n : magic_names() ) {
					if ( symbolPrefix.is_empty() || ( n.find( symbolPrefix ) == 0 ) ) {
						completions.push_back( "//"_ys.append( n ).append( ' ' ) );
					}
				}
				break;
			}
		}
		int long len( prefix_.get_length() );
		int long ctxLen( context_.get_length() );
		if ( repl->shell() && !! setup._shell && setup._shell->is_empty() ) {
			for ( HShell::system_commands_t::value_type const& sc : repl->shell()->system_commands() ) {
				if ( ! context_.is_empty() && ( sc.first.find( context_ ) == 0 ) ) {
					completions.push_back( sc.first.mid( ctxLen - len ) + " " );
				}
			}
			for ( HShell::builtins_t::value_type const& b : repl->shell()->builtins() ) {
				if ( ! context_.is_empty() && ( b.first.find( context_ ) == 0 ) ) {
					completions.push_back( b.first.mid( ctxLen - len ) + " " );
				}
			}
			for ( HShell::aliases_t::value_type const& a : repl->shell()->aliases() ) {
				if ( ! context_.is_empty() && ( a.first.find( context_ ) == 0 ) ) {
					completions.push_back( a.first.mid( ctxLen - len ) + " " );
				}
			}
			for ( yaal::hcore::HString const& f : repl->shell()->filename_completions( context_, prefix_ ) ) {
				completions.push_back( f );
			}
		}
		HString symbol;
		HString dot;
		if ( dotIdx != HString::npos ) {
			symbol.assign( prefix_, 0, dotIdx );
			prefix_.shift_left( dotIdx + 1 );
			len -= ( dotIdx + 1 );
			dot.assign( "." );
		}
		bool inDocContext( context_.find( "//doc " ) == 0 );
		HLineRunner::words_t const& words(
			! symbol.is_empty() ? repl->line_runner()->dependent_symbols( symbol, inDocContext ) : repl->line_runner()->words( inDocContext )
		);
		HString buf;
		for ( HString const& w : words ) {
			if ( ! prefix_.is_empty() && ( prefix_ != w.left( len ) ) ) {
				continue;
			}
			if ( symbol.is_empty() ) {
				completions.push_back( dot + w );
			} else {
				buf.assign( symbol ).append( dot ).append( w ).append( "(" );
				completions.push_back( buf );
			}
		}
	} while ( false );
	sort( completions.begin(), completions.end() );
	return ( completions );
	M_EPILOG
}

inline char const* condColor( char const*
#ifndef USE_EDITLINE
	color_
#endif
) {
#ifndef USE_EDITLINE
	return ( ! setup._noColor ? color_ : "" );
#else
	return ( "" );
#endif
}

void make_prompt( char* prompt_, int size_, int& no_ ) {
	snprintf(
		prompt_,
		static_cast<size_t>( size_ ),
		"%s%s%shuginn[%s%s%s%d%s%s%s]> %s%s%s",
		condColor( REPL_ignore_start ),
		condColor( ansi_color( GROUP::PROMPT ) ),
		condColor( REPL_ignore_end ),
		condColor( REPL_ignore_start ),
		condColor( ansi_color( GROUP::PROMPT_MARK ) ),
		condColor( REPL_ignore_end ),
		no_,
		condColor( REPL_ignore_start ),
		condColor( ansi_color( GROUP::PROMPT ) ),
		condColor( REPL_ignore_end ),
		condColor( REPL_ignore_start ),
		condColor( *ansi::reset ),
		condColor( REPL_ignore_end )
	);
	++ no_;
}

HString colorize( HHuginn::value_t const& value_, HHuginn* huginn_ ) {
	M_PROLOG
	HString res;
	HString strRes( to_string( value_, huginn_ ) );
	if ( ! setup._noColor ) {
		switch ( value_->type_id().get() ) {
			case ( static_cast<int>( HHuginn::TYPE::INTEGER ) ):
			case ( static_cast<int>( HHuginn::TYPE::BOOLEAN ) ):
			case ( static_cast<int>( HHuginn::TYPE::CHARACTER ) ):
			case ( static_cast<int>( HHuginn::TYPE::REAL ) ):
			case ( static_cast<int>( HHuginn::TYPE::NUMBER ) ):
			case ( static_cast<int>( HHuginn::TYPE::STRING ) ):
			case ( static_cast<int>( HHuginn::TYPE::NONE ) ): {
				res.append( ansi_color( GROUP::LITERALS ) );
			} break;
			case ( static_cast<int>( HHuginn::TYPE::FUNCTION_REFERENCE ) ): {
				if ( is_builtin( strRes ) ) {
					res.append( ansi_color( GROUP::BUILTINS ) );
				} else if ( strRes == "Exception" ) {
					res.append( ansi_color( GROUP::CLASSES ) );
				}
			} break;
			default: {
				strRes = ::huginn::colorize( strRes );
			}
		}
	}
	res.append( strRes );
	if ( ! setup._noColor ) {
		res.append( *ansi::reset );
	}
	return ( res );
	M_EPILOG
}

}

int interactive_session( void ) {
	M_PROLOG
	if ( ! setup._quiet ) {
		banner();
	}
	static int const PROMPT_SIZE( 128 );
	char prompt[PROMPT_SIZE];
	int lineNo( 0 );
	make_prompt( prompt, PROMPT_SIZE, lineNo );
	HLineRunner lr( "*interactive session*" );
	shell_t shell( !! setup._shell && setup._shell->is_empty() ? make_resource<HShell>( lr ) : shell_t() );
	HRepl repl;
	repl.set_shell( shell.raw() );
	repl.set_line_runner( &lr );
	repl.set_completer( &completion_words );
	repl.set_history_path( setup._historyPath );
	int retVal( 0 );
	HString line;
	HUTF8String colorized;
	lr.load_session();
	while ( setup._interactive && repl.input( line, prompt ) ) {
		if ( line.is_empty() ) {
			continue;
		}
		if ( meta( lr, line ) ) {
			/* Done in meta(). */
		} else if ( lr.add_line( line ) ) {
			HHuginn::value_t res( lr.execute() );
			if ( !! res && lr.use_result() && ( res->type_id() == HHuginn::TYPE::INTEGER ) ) {
				retVal = static_cast<int>( static_cast<HHuginn::HInteger*>( res.raw() )->value() );
			} else {
				retVal = 0;
			}
			if ( !! res ) {
				if ( lr.use_result() && ( line.back() != ';' ) ) {
					colorized = colorize( res, lr.huginn() );
					repl.print( colorized.c_str() );
				}
			} else {
				cerr << lr.err() << endl;
			}
		} else if ( ! setup._shell || ! shell->run( line ) ) {
			cerr << lr.err() << endl;
		}
		make_prompt( prompt, PROMPT_SIZE, lineNo );
	}
	if ( setup._interactive ) {
		repl.print( "" );
	}
	lr.save_session();
	return ( retVal );
	M_EPILOG
}

}

