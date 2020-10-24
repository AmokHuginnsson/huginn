/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <yaal/hcore/hcore.hxx>
#include <yaal/hcore/hfile.hxx>
#include <yaal/hcore/hlog.hxx>
#include <yaal/hcore/system.hxx>
#include <yaal/tools/ansi.hxx>
#include <yaal/tools/stringalgo.hxx>
#include <yaal/tools/filesystem.hxx>
#include <yaal/tools/huginn/integer.hxx>
#include <yaal/tools/huginn/helper.hxx>
#include <yaal/tools/huginn/packagefactory.hxx>

#include <cstring>
#include <cstdio>

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
#endif

M_VCSID( "$Id: " __ID__ " $" )
#include "interactive.hxx"
#include "linerunner.hxx"
#include "meta.hxx"
#include "setup.hxx"
#include "colorize.hxx"
#include "symbolicnames.hxx"
#include "systemshell.hxx"
#include "forwardingshell.hxx"
#include "quotes.hxx"
#include "repl.hxx"
#include "settings.hxx"

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;
using namespace yaal::tools::filesystem;
using namespace yaal::tools::huginn;

namespace huginn {

namespace {

bool is_literal( yaal::hcore::HString const& symbol_ ) {
	return ( ( symbol_ == "none" ) || ( symbol_ == "true" ) || ( symbol_ == "false" ) );
}

COLOR::color_t symbol_color( yaal::hcore::HString const& symbol_ ) {
	COLOR::color_t c( COLOR::ATTR_DEFAULT );
	if ( is_literal( symbol_ ) ) {
		c = color( GROUP::LITERALS );
	} else if ( is_keyword( symbol_ ) ) {
		c = color( GROUP::KEYWORDS );
	} else if ( is_builtin( symbol_ ) ) {
		c = color( GROUP::BUILTINS );
	} else if ( is_upper( symbol_.front() ) ) {
		c = color( GROUP::CLASSES );
	}
	return ( c );
}

HRepl::completions_t completion_words( yaal::hcore::HString&& context_, yaal::hcore::HString&& prefix_, int& contextLen_, CONTEXT_TYPE& contextType_, void* data_, bool hints_ ) {
	M_PROLOG
	HRepl* repl( static_cast<HRepl*>( data_ ) );
	HRepl::completions_t completions;
	do {
		context_.trim_left();
		int long dotIdx( prefix_.find_last( '.'_ycp ) );
		int long backSlashIdx( prefix_.find_last( '\\'_ycp ) );
		if ( ( backSlashIdx != hcore::HString::npos ) && ( ( dotIdx == hcore::HString::npos ) || ( backSlashIdx > dotIdx ) ) ) {
			hcore::HString symbolPrefix( prefix_.substr( backSlashIdx ) );
			char const* symbolicName( symbol_from_name( symbolPrefix ) );
			if ( symbolicName ) {
				completions.emplace_back( symbolicName );
				contextLen_ -= static_cast<int>( prefix_.get_length() - symbolPrefix.get_length() );
				break;
			} else {
				symbolic_names_t sn( symbol_name_completions( symbolPrefix ) );
				if ( ! sn.is_empty() ) {
					for ( yaal::hcore::HString const& n : sn ) {
						completions.emplace_back( n );
					}
					contextLen_ -= static_cast<int>( prefix_.get_length() - symbolPrefix.get_length() );
					break;
				}
			}
		}
		if ( in_quotes( context_ ) ) {
			break;
		}
		static hcore::HString const import( "import" );
		static hcore::HString const from( "from" );
		if ( context_.starts_with( import ) ) {
			tools::huginn::HPackageFactory& pf( tools::huginn::HPackageFactory::get_instance() );
			for ( tools::huginn::HPackageFactory::creators_t::value_type const& p : pf ) {
				if ( ( prefix_.is_empty() && ( context_.get_length() <= ( import.get_length() + 1 ) ) ) || ( p.first.find( prefix_ ) == 0 ) ) {
					completions.emplace_back( p.first + " as " );
				}
			}
			break;
		} else if ( import.find( context_ ) == 0 ) {
			completions.emplace_back( "import " );
		} else if ( context_.starts_with( from ) ) {
			tools::huginn::HPackageFactory& pf( tools::huginn::HPackageFactory::get_instance() );
			for ( tools::huginn::HPackageFactory::creators_t::value_type const& p : pf ) {
				if ( ( prefix_.is_empty() && ( context_.get_length() <= ( from.get_length() + 1 ) ) ) || ( p.first.find( prefix_ ) == 0 ) ) {
					completions.emplace_back( p.first + " import " );
				}
			}
			break;
		} else if ( from.starts_with( context_ ) ) {
			completions.emplace_back( "from " );
		} if ( context_.starts_with( "//" ) ) {
			if ( context_.starts_with( "//set " ) ) {
				hcore::HString symbolPrefix( context_.substr( 6 ) );
				for ( rt_settings_t::value_type const& s : rt_settings() ) {
					if ( symbolPrefix.is_empty() || ( s.first.find( symbolPrefix ) == 0 ) ) {
						completions.emplace_back( to_string( s.first ).append( '=' ) );
					}
				}
				break;
			} else if ( context_.starts_with( "//history " ) ) {
				static hcore::HString const historySubCommads[] = { "clear", "sync" };
				hcore::HString symbolPrefix( context_.substr( 10 ) );
				for ( hcore::HString const& historySubCommad : historySubCommads ) {
					if ( symbolPrefix.is_empty() || historySubCommad.starts_with( symbolPrefix ) ) {
						completions.emplace_back( historySubCommad );
					}
				}
				break;
			} else if ( context_.find( "//doc " ) == hcore::HString::npos ) {
				hcore::HString symbolPrefix( context_.substr( 2 ) );
				for ( yaal::hcore::HString const& n : magic_names() ) {
					if ( symbolPrefix.is_empty() || ( n.find( symbolPrefix ) == 0 ) ) {
						completions.emplace_back( "//"_ys.append( n ).append( ' ' ) );
					}
				}
				contextLen_ += 2;
				break;
			}
		}
		HSystemShell* systemShell( dynamic_cast<HSystemShell*>( repl->shell() ) );
		if ( systemShell ) {
			try {
				hcore::HString shellPrefix( context_ );
				int shellContextLen( context_length( shellPrefix, CONTEXT_TYPE::SHELL ) );
				shellPrefix.shift_left( shellPrefix.get_length() - shellContextLen );
				HRepl::completions_t shellCompletions( systemShell->gen_completions( context_, shellPrefix, hints_ ) );
				for ( HRepl::HCompletion const& sc : shellCompletions ) {
					completions.emplace_back( sc );
				}
				if ( ! completions.is_empty() ) {
					contextType_ = CONTEXT_TYPE::SHELL;
					contextLen_ = shellContextLen;
					break;
				}
				if ( systemShell->has_huginn_jobs() ) {
					break;
				}
			} catch (HException const&) {
				break;
			}
		}
		hcore::HString symbol;
		hcore::HString dot;
		int long len( prefix_.get_length() );
		if ( dotIdx != hcore::HString::npos ) {
			symbol.assign( prefix_, 0, dotIdx );
			prefix_.shift_left( dotIdx + 1 );
			len -= ( dotIdx + 1 );
			dot.assign( "." );
		}
		bool inDocContext( context_.find( "//doc " ) == 0 );
		HLineRunner::words_t words(
			! symbol.is_empty() ? repl->line_runner()->dependent_symbols( symbol, inDocContext ) : ( ! systemShell ? repl->line_runner()->words( inDocContext ) : HLineRunner::words_t{} )
		);
		hcore::HString tn( ! symbol.is_empty() ? repl->line_runner()->symbol_type_name( symbol ) : "" );
		tn.append( dot );
		hcore::HString buf;
		for ( hcore::HString const& w : words ) {
			if ( ! prefix_.is_empty() && ( prefix_ != w.left( len ) ) ) {
				continue;
			}
			if ( symbol.is_empty() ) {
				completions.emplace_back( dot + w, symbol_color( w ) );
				continue;
			}
			buf.assign( symbol ).append( dot ).append( w );
			hcore::HString const& doc( repl->line_runner()->doc( tn + w, true ) );
			if ( ! doc.is_empty() ) {
				int offset( 0 );
				if ( doc.front() == '*' ) {
					offset += 2;
				}
				offset += static_cast<int>( w.get_length() );
				if ( ( offset < doc.get_length() ) && ( doc[offset] == '(' ) ) {
					buf.append( "(" );
					++ offset;
					if ( ( offset < doc.get_length() ) && ( doc[offset] == ')' ) ) {
						buf.append( ")" );
					}
				}
			}
			completions.emplace_back( buf, symbol_color( buf ) );
		}
	} while ( false );
	sort( completions.begin(), completions.end() );
	completions.erase( unique( completions.begin(), completions.end() ), completions.end() );
	return ( completions );
	M_EPILOG
}

inline void condColor( hcore::HString& prompt_, char const* color_ ) {
	if ( ! setup._noColor ) {
		prompt_.append( REPL_ignore_start ).append( color_ ).append( REPL_ignore_end );
	}
	return;
}

void make_prompt( char* prompt_, int size_, int no_, HSystemShell* shell_ ) {
	M_PROLOG
	hcore::HString promptTemplate( setup._prompt );
	substitute_environment( promptTemplate, ENV_SUBST_MODE::RECURSIVE );
	hcore::HString prompt;
	bool special( false );
	for ( code_point_t cp : promptTemplate ) {
		if ( cp == '%' ) {
			special = true;
			continue;
		}
		if ( ! special ) {
			prompt.push_back( cp );
			continue;
		}
		switch ( cp.get() ) {
			case ( 'k' ): condColor( prompt, *ansi::black );         break;
			case ( 'K' ): condColor( prompt, *ansi::gray );          break;
			case ( 'r' ): condColor( prompt, *ansi::red );           break;
			case ( 'R' ): condColor( prompt, *ansi::brightred );     break;
			case ( 'g' ): condColor( prompt, *ansi::green );         break;
			case ( 'G' ): condColor( prompt, *ansi::brightgreen );   break;
			case ( 'y' ): condColor( prompt, *ansi::brown );         break;
			case ( 'Y' ): condColor( prompt, *ansi::yellow );        break;
			case ( 'b' ): condColor( prompt, *ansi::blue );          break;
			case ( 'B' ): condColor( prompt, *ansi::brightblue );    break;
			case ( 'm' ): condColor( prompt, *ansi::magenta );       break;
			case ( 'M' ): condColor( prompt, *ansi::brightmagenta ); break;
			case ( 'c' ): condColor( prompt, *ansi::cyan );          break;
			case ( 'C' ): condColor( prompt, *ansi::brightcyan );    break;
			case ( 'w' ): condColor( prompt, *ansi::lightgray );     break;
			case ( 'W' ): condColor( prompt, *ansi::white );         break;
			case ( '*' ): condColor( prompt, *ansi::bold );          break;
			case ( '_' ): condColor( prompt, *ansi::underline );     break;
			case ( 'p' ): condColor( prompt, ansi_color( GROUP::PROMPT ) );      break;
			case ( 'P' ): condColor( prompt, ansi_color( GROUP::PROMPT_MARK ) ); break;
			case ( 'q' ): condColor( prompt, ansi_color( GROUP::LOCAL_HOST ) );  break;
			case ( 'Q' ): condColor( prompt, ansi_color( GROUP::REMOTE_HOST ) ); break;
			case ( 'x' ): condColor( prompt, *ansi::reset );         break;
			case ( '%' ): prompt.append( "%" ); break;
			case ( 'i' ): prompt.append( no_ ); break;
			case ( 'l' ): /* fall through */
			case ( 'n' ): /* fall through */
			case ( 'u' ): prompt.append( system::get_user_name( system::get_user_id() ) ); break;
			case ( 'h' ): {
				hcore::HString h( system::get_host_name() );
				int long dotPos( h.find( '.'_ycp ) );
				if ( dotPos != hcore::HString::npos ) {
					h.erase( dotPos );
				}
				prompt.append( h );
			} break;
			case ( 'H' ): prompt.append( system::get_host_name() ); break;
			case ( 't' ): prompt.append( now_local().set_format( _iso8601TimeFormat_ ).string() ); break;
			case ( 'd' ): prompt.append( now_local().set_format( _iso8601DateFormat_ ).string() ); break;
			case ( 'D' ): prompt.append( now_local().set_format( _iso8601DateTimeFormat_ ).string() ); break;
			case ( 'j' ): {
				if ( ! shell_ ) {
					break;
				}
				prompt.append( shell_->job_count() );
			} break;
			case ( '#' ): prompt.append( "$" ); break;
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
				prompt.append( curDir ).append( "/" );
			} break;
		}
		special = false;
	}
	HUTF8String utf8( unescape_system( yaal::move( prompt ) ) );
	strncpy( prompt_, utf8.c_str(), static_cast<size_t>( size_ ) - 1 );
	return;
	M_EPILOG
}

hcore::HString colorize( HHuginn::value_t const& value_, HHuginn* huginn_ ) {
	M_PROLOG
	hcore::HString res;
	hcore::HString strRes( code( value_, huginn_ ) );
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
	static int const PROMPT_SIZE( 1024 );
	char prompt[PROMPT_SIZE];
	HLineRunner lr( "*interactive session*" );
	if ( ! setup._noDefaultInit ) {
		filesystem::path_t initPath( make_conf_path( "init" ) );
		hcore::log << "Loading `init` from `" << initPath << "`." << endl;
		lr.load_session( initPath, false );
		lr.call( "init", {}, &cerr );
	}
	HRepl repl;
	shell_t shell(
		!! setup._shell
			? ( setup._shell->is_empty() ? shell_t( make_resource<HSystemShell>( lr, repl ) ) : shell_t( make_resource<HForwardingShell>() ) )
			: shell_t()
	);
	HSystemShell* systemShell( dynamic_cast<HSystemShell*>( shell.get() ) );
	repl.set_hint_delay( !!shell ? 300 : 0 );
	repl.set_shell( shell.raw() );
	repl.set_line_runner( &lr );
	repl.set_completer( &completion_words );
	repl.set_history_path( setup._historyPath );
	static int const DEFAULT_MAX_HISTORY_SIZE( 4000 );
	repl.set_max_history_size( DEFAULT_MAX_HISTORY_SIZE );
	repl.enable_bracketed_paste();
	repl.load_history();
	lr.load_session( setup._sessionDir + PATH_SEP + setup._session, true );
	hcore::HString scheme( setup._colorScheme );
	if ( ! scheme.is_empty() ) {
		set_color_scheme( setup._colorScheme = scheme );
	}
	if ( ! ( setup._quiet || setup.is_system_shell() ) ) {
		banner( &repl );
	}
	int retVal( 0 );
	HUTF8String colorized;
	hcore::HString line;
	int lineNo( 0 );
	while ( setup._interactive ) {
		lr.mend_interrupt();
		if ( !! setup._shell && ! ( systemShell && systemShell->has_huginn_jobs() ) ) {
			unset_env( VOLATILE_PROMPT_INFO_VAR_NAME );
			lr.call( "pre_prompt", {}, &cerr );
		}
		make_prompt( prompt, PROMPT_SIZE, lineNo, systemShell );
		if ( ! repl.input( line, prompt ) && ( ! systemShell || systemShell->finalized() ) ) {
			break;
		}
		if ( line.is_empty() || ( ( line.get_length() == 1 ) && ( line.front() == '\\' ) ) ) {
			continue;
		}
		++ lineNo;
		if ( meta( lr, line, &repl ) ) {
			/* Done in meta(). */
		} else if ( !! setup._shell && shell->try_command( line ) ) {
			retVal = shell->run( line ).exit_status().value;
		} else if ( systemShell && systemShell->has_huginn_jobs() ) {
			cerr << "Huginn jobs present!" << endl;
		} else if ( lr.add_line( unescape_huginn_code( line ), true ) ) {
			HHuginn::value_t res( lr.execute() );
			if ( !! res && lr.use_result() && ( res->type_id() == HHuginn::TYPE::INTEGER ) ) {
				retVal = static_cast<int>( static_cast<HInteger*>( res.raw() )->value() );
			} else {
				retVal = 0;
			}
			if ( !! res ) {
				if ( lr.use_result() && ( line.back() != ';' ) ) {
					colorized = colorize( res, lr.huginn() );
					repl.print( "%s\n", colorized.c_str() );
				}
			} else {
				cerr << lr.err() << endl;
			}
		} else {
			cerr << lr.err() << endl;
			if ( systemShell ) {
				systemShell->command_not_found( line );
			}
		}
	}
	if ( setup._interactive ) {
		repl.print( "\n" );
	}
	filesystem::create_directory( setup._sessionDir, DIRECTORY_MODIFICATION::RECURSIVE );
	lr.save_session( setup._sessionDir + "/" + setup._session );
	return ( retVal );
	M_EPILOG
}

}

