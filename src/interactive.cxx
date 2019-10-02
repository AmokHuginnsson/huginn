/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <yaal/hcore/hcore.hxx>
#include <yaal/hcore/hfile.hxx>
#include <yaal/hcore/system.hxx>
#include <yaal/tools/ansi.hxx>
#include <yaal/tools/stringalgo.hxx>
#include <yaal/tools/filesystem.hxx>
#include <yaal/tools/huginn/integer.hxx>
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
using namespace yaal::tools::huginn;

namespace yaal { namespace tools { namespace huginn {
bool is_keyword( yaal::hcore::HString const& );
bool is_builtin( yaal::hcore::HString const& );
bool is_reserved( yaal::hcore::HString const& );
}}}

namespace huginn {

namespace {

bool is_literal( yaal::hcore::HString const& symbol_ ) {
	return ( ( symbol_ == "none" ) || ( symbol_ == "true" ) || ( symbol_ == "false" ) );
}

COLOR::color_t symbol_color( yaal::hcore::HString const& symbol_ ) {
	COLOR::color_t c( COLOR::ATTR_DEFAULT );
	if ( is_literal( symbol_ ) ) {
		c = COLOR::FG_BRIGHTMAGENTA;
	} else if ( is_keyword( symbol_ ) ) {
		c = COLOR::FG_YELLOW;
	} else if ( is_builtin( symbol_ ) ) {
		c = COLOR::FG_BRIGHTGREEN;
	} else if ( is_upper( symbol_.front() ) ) {
		c = COLOR::FG_BROWN;
	}
	return ( c );
}

HRepl::completions_t completion_words( yaal::hcore::HString&& context_, yaal::hcore::HString&& prefix_, int& contextLen_, CONTEXT_TYPE& contextType_, void* data_ ) {
	M_PROLOG
	HRepl* repl( static_cast<HRepl*>( data_ ) );
	HRepl::completions_t completions;
	do {
		context_.trim_left();
		int long dotIdx( prefix_.find_last( '.'_ycp ) );
		int long backSlashIdx( prefix_.find_last( '\\'_ycp ) );
		if ( ( backSlashIdx != HString::npos ) && ( ( dotIdx == HString::npos ) || ( backSlashIdx > dotIdx ) ) ) {
			HString symbolPrefix( prefix_.substr( backSlashIdx ) );
			char const* symbolicName( symbol_from_name( symbolPrefix ) );
			if ( symbolicName ) {
				completions.emplace_back( symbolicName );
				break;
			} else {
				symbolic_names_t sn( symbol_name_completions( symbolPrefix ) );
				if ( ! sn.is_empty() ) {
					for ( yaal::hcore::HString const& n : sn ) {
						completions.emplace_back( n );
					}
					break;
				}
			}
		}
		if ( in_quotes( context_ ) ) {
			break;
		}
		static HString const import( "import" );
		static HString const from( "from" );
		if ( context_.find( import ) == 0 ) {
			tools::huginn::HPackageFactory& pf( tools::huginn::HPackageFactory::get_instance() );
			for ( tools::huginn::HPackageFactory::creators_t::value_type const& p : pf ) {
				if ( ( prefix_.is_empty() && ( context_.get_length() <= ( import.get_length() + 1 ) ) ) || ( p.first.find( prefix_ ) == 0 ) ) {
					completions.emplace_back( p.first + " as " );
				}
			}
			break;
		} else if ( import.find( context_ ) == 0 ) {
			completions.emplace_back( "import " );
		} else if ( context_.find( from ) == 0 ) {
			tools::huginn::HPackageFactory& pf( tools::huginn::HPackageFactory::get_instance() );
			for ( tools::huginn::HPackageFactory::creators_t::value_type const& p : pf ) {
				if ( ( prefix_.is_empty() && ( context_.get_length() <= ( from.get_length() + 1 ) ) ) || ( p.first.find( prefix_ ) == 0 ) ) {
					completions.emplace_back( p.first + " import " );
				}
			}
			break;
		} else if ( from.find( context_ ) == 0 ) {
			completions.emplace_back( "from " );
		} if ( context_.find( "//" ) == 0 ) {
			if ( context_.find( "//set " ) == 0 ) {
				HString symbolPrefix( context_.substr( 6 ) );
				for ( rt_settings_t::value_type const& s : rt_settings() ) {
					if ( symbolPrefix.is_empty() || ( s.first.find( symbolPrefix ) == 0 ) ) {
						completions.emplace_back( to_string( s.first ).append( '=' ) );
					}
				}
				break;
			} else if ( context_.find( "//doc " ) == HString::npos ) {
				HString symbolPrefix( context_.substr( 2 ) );
				for ( yaal::hcore::HString const& n : magic_names() ) {
					if ( symbolPrefix.is_empty() || ( n.find( symbolPrefix ) == 0 ) ) {
						completions.emplace_back( "//"_ys.append( n ).append( ' ' ) );
					}
				}
				contextLen_ += 2;
				break;
			}
		}
		if ( repl->shell() && !! setup._shell && setup._shell->is_empty() ) {
			HString shellPrefix( context_ );
			int shellContextLen( context_length( shellPrefix, CONTEXT_TYPE::SHELL ) );
			shellPrefix.shift_left( shellPrefix.get_length() - shellContextLen );
			HRepl::completions_t shellCompletions( repl->shell()->gen_completions( context_, shellPrefix ) );
			for ( HRepl::HCompletion const& sc : shellCompletions ) {
				completions.emplace_back( sc );
			}
			if ( ! completions.is_empty() ) {
				contextType_ = CONTEXT_TYPE::SHELL;
				contextLen_ = shellContextLen;
				break;
			}
		}
		HString symbol;
		HString dot;
		int long len( prefix_.get_length() );
		if ( dotIdx != HString::npos ) {
			symbol.assign( prefix_, 0, dotIdx );
			prefix_.shift_left( dotIdx + 1 );
			len -= ( dotIdx + 1 );
			dot.assign( "." );
		}
		bool inDocContext( context_.find( "//doc " ) == 0 );
		HLineRunner::words_t words(
			! symbol.is_empty() ? repl->line_runner()->dependent_symbols( symbol, inDocContext ) : repl->line_runner()->words( inDocContext )
		);
		HString tn( ! symbol.is_empty() ? repl->line_runner()->symbol_type_name( symbol ) : "" );
		tn.append( dot );
		HString buf;
		for ( HString const& w : words ) {
			if ( ! prefix_.is_empty() && ( prefix_ != w.left( len ) ) ) {
				continue;
			}
			if ( symbol.is_empty() ) {
				completions.emplace_back( dot + w, symbol_color( w ) );
				continue;
			}
			buf.assign( symbol ).append( dot ).append( w );
			HString const& doc( repl->line_runner()->doc( tn + w, true ) );
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

inline void condColor( HString& prompt_, char const* color_ ) {
	if ( ! setup._noColor ) {
		prompt_.append( REPL_ignore_start ).append( color_ ).append( REPL_ignore_end );
	}
	return;
}

void make_prompt( char* prompt_, int size_, int no_ ) {
	M_PROLOG
	HString promptTemplate( setup._prompt );
	substitute_environment( promptTemplate, ENV_SUBST_MODE::RECURSIVE );
	HString prompt;
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
			case ( 'p' ): condColor( prompt, ansi_color( GROUP::PROMPT ) ); break;
			case ( 'P' ): condColor( prompt, ansi_color( GROUP::PROMPT_MARK ) ); break;
			case ( 'x' ): condColor( prompt, *ansi::reset );         break;
			case ( '%' ): prompt.append( "%" ); break;
			case ( 'i' ): prompt.append( no_ ); break;
			case ( 'l' ): /* fall through */
			case ( 'n' ): /* fall through */
			case ( 'u' ): prompt.append( system::get_user_name( system::get_user_id() ) ); break;
			case ( 'h' ): {
				HString h( system::get_host_name() );
				int long dotPos( h.find( '.'_ycp ) );
				if ( dotPos != HString::npos ) {
					h.erase( dotPos );
				}
				prompt.append( h );
			} break;
			case ( 'H' ): prompt.append( system::get_host_name() ); break;
			case ( '#' ): prompt.append( "$" ); break;
			case ( '~' ): {
				char const* PWD( ::getenv( "PWD" ) );
				char const* HOME_PATH( ::getenv( HOME_ENV_VAR ) );
				filesystem::path_t curDir( PWD ? PWD : filesystem::current_working_directory() );
				if ( HOME_ENV_VAR && ( curDir.find( HOME_PATH ) == 0 ) ) {
					curDir.replace( 0, static_cast<int>( strlen( HOME_PATH ) ), "~" );
				}
				prompt.append( curDir ).append( "/" );
			} break;
		}
		special = false;
	}
	HUTF8String utf8( prompt );
	strncpy( prompt_, utf8.c_str(), static_cast<size_t>( size_ ) - 1 );
	return;
	M_EPILOG
}

HString colorize( HHuginn::value_t const& value_, HHuginn* huginn_ ) {
	M_PROLOG
	HString res;
	HString strRes( code( value_, huginn_ ) );
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
	int lineNo( 0 );
	HLineRunner lr( "*interactive session*" );
	HRepl repl;
	shell_t shell(
		!! setup._shell
			? ( setup._shell->is_empty() ? shell_t( make_resource<HSystemShell>( lr, repl ) ) : shell_t( make_resource<HForwardingShell>() ) )
			: shell_t()
	);
	repl.set_hint_delay( !!shell ? 300 : 0 );
	repl.set_shell( shell.raw() );
	repl.set_line_runner( &lr );
	repl.set_completer( &completion_words );
	repl.set_history_path( setup._historyPath );
	int retVal( 0 );
	HString line;
	HUTF8String colorized;
	HString scheme( setup._colorScheme );
	if ( ! setup._noDefaultInit ) {
		char const* HUGINN_INIT( getenv( "HUGINN_INIT" ) );
		lr.load_session( HUGINN_INIT ? HUGINN_INIT : setup._sessionDir + "/init", false );
		lr.call( "init", {}, &cerr );
		if ( !! setup._shell ) {
			lr.call( "init_shell", {}, &cerr );
		}
	}
	lr.load_session( setup._sessionDir + "/" + setup._session, true );
	if ( ! scheme.is_empty() ) {
		set_color_scheme( setup._colorScheme = scheme );
	}
	if ( ! setup._quiet ) {
		banner();
	}
	while ( setup._interactive ) {
		if ( !! setup._shell ) {
			lr.call( "pre_prompt", {}, &cerr );
		}
		make_prompt( prompt, PROMPT_SIZE, lineNo );
		if ( ! repl.input( line, prompt ) ) {
			break;
		}
		if ( line.is_empty() || ( ( line.get_length() == 1 ) && ( line.front() == '\\' ) ) ) {
			continue;
		}
		++ lineNo;
		if ( meta( lr, line ) ) {
			/* Done in meta(). */
		} else if ( !! setup._shell && shell->try_command( line ) ) {
			shell->run( line );
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
					repl.print( colorized.c_str() );
				}
			} else {
				cerr << lr.err() << endl;
			}
		} else {
			cerr << lr.err() << endl;
		}
	}
	if ( setup._interactive ) {
		repl.print( "" );
	}
	filesystem::create_directory( setup._sessionDir, 0700 );
	lr.save_session( setup._sessionDir + "/" + setup._session );
	return ( retVal );
	M_EPILOG
}

}

