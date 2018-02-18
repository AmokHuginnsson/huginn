/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <cstring>
#include <cstdio>

#include "repl.hxx"
M_VCSID( "$Id: " __ID__ " $" )
M_VCSID( "$Id: " __TID__ " $" )

#include "colorize.hxx"
#include "shell.hxx"
#include "setup.hxx"

#include "config.hxx"

#ifdef USE_REPLXX
#	include <replxx.h>
#	define REPL_load_history replxx_history_load
#	define REPL_save_history replxx_history_save
#	define REPL_add_history replxx_history_add
#	define REPL_ignore_start ""
#	define REPL_ignore_end ""
#	define REPL_get_input replxx_input
#	define REPL_free replxx_free
#	define REPL_print replxx_print
#elif defined( USE_EDITLINE )
#	include <yaal/tools/hterminal.hxx>
#	include <histedit.h>
#	include <signal.h>
#	define _el *reinterpret_cast<EditLine**>( &_elData.get<0>() )
#	define _hist *reinterpret_cast<History**>( &_elData.get<1>() )
#	define _count _elData.get<2>()
#	define _buf _elData.get<3>()
#	define _histBuf static_cast<HistEvent*>( static_cast<void*>( _buf.raw() ) )
#	define REPL_load_history( file ) history( _hist, _histBuf, H_LOAD, file )
#	define REPL_save_history( file ) history( _hist, _histBuf, H_SAVE, file )
#	define REPL_add_history( line ) history( _hist, _histBuf, H_ENTER, line )
#	define REPL_ignore_start ""
#	define REPL_ignore_end ""
#	define REPL_get_input( ... ) const_cast<char*>( el_gets( _el, &_count ) )
#	define REPL_free memory::free0
#	define REPL_print printf
#else
#	include <readline/readline.h>
#	include <readline/history.h>
#	define REPL_load_history read_history
#	define REPL_save_history write_history
#	define REPL_add_history add_history
static char const REPL_ignore_start[] = { RL_PROMPT_START_IGNORE, 0 };
static char const REPL_ignore_end[] = { RL_PROMPT_END_IGNORE, 0 };
#	define REPL_get_input readline
#	define REPL_free memory::free0
#	define REPL_print printf
#endif

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;
using namespace yaal::tools::huginn;

namespace yaal { namespace hcore {
template<>
int long hash<yaal::tools::COLOR::color_t>::operator () ( yaal::tools::COLOR::color_t const& val_ ) const {
	return ( static_cast<int long>( val_ ) );
}
} }

namespace huginn {

namespace {

char const BREAK_CHARS_RAW[] = " \t\n\"\\'`@$><=?:;,|&![{()}]+-*/%^~";
HString const BREAK_CHARS( BREAK_CHARS_RAW );
char const SPECIAL_PREFIXES_RAW[] = "\\/";
HString const SPECIAL_PREFIXES( SPECIAL_PREFIXES_RAW );

#ifdef USE_REPLXX

typedef yaal::hcore::HHashMap<yaal::tools::COLOR::color_t, replxx_color::color> replxx_colors_t;
replxx_colors_t _replxxColors_ = {
	{ COLOR::FG_BLACK,         replxx_color::BLACK },
	{ COLOR::FG_RED,           replxx_color::RED },
	{ COLOR::FG_GREEN,         replxx_color::GREEN },
	{ COLOR::FG_BROWN,         replxx_color::BROWN },
	{ COLOR::FG_BLUE,          replxx_color::BLUE },
	{ COLOR::FG_MAGENTA,       replxx_color::MAGENTA },
	{ COLOR::FG_CYAN,          replxx_color::CYAN },
	{ COLOR::FG_LIGHTGRAY,     replxx_color::LIGHTGRAY },
	{ COLOR::FG_GRAY,          replxx_color::GRAY },
	{ COLOR::FG_BRIGHTRED,     replxx_color::BRIGHTRED },
	{ COLOR::FG_BRIGHTGREEN,   replxx_color::BRIGHTGREEN },
	{ COLOR::FG_YELLOW,        replxx_color::YELLOW },
	{ COLOR::FG_BRIGHTBLUE,    replxx_color::BRIGHTBLUE },
	{ COLOR::FG_BRIGHTMAGENTA, replxx_color::BRIGHTMAGENTA },
	{ COLOR::FG_BRIGHTCYAN,    replxx_color::BRIGHTCYAN },
	{ COLOR::FG_WHITE,         replxx_color::WHITE }
};

void replxx_completion_words( char const* prefix_, int offset_, replxx_completions* completions_, void* data_ ) {
	HString prefix( prefix_ );
	prefix.shift_left( offset_ );
	HLineRunner::words_t completions( static_cast<HRepl*>( data_ )->completion_words( prefix_, yaal::move( prefix ) ) );
	HUTF8String utf8;
	for ( yaal::hcore::HString const& c : completions ) {
		utf8.assign( c );
		replxx_add_completion( completions_, utf8.c_str() );
	}
	return;
}

void find_hints( char const* prefix_, int offset_, replxx_hints* hints_, replxx_color::color* color_, void* data_ ) {
	HRepl* repl( static_cast<HRepl*>( data_ ) );
	HString context( prefix_ );
	HString prefix( prefix_ );
	prefix.shift_left( offset_ );
	if ( prefix.is_empty() || ( prefix == "." ) ) {
		return;
	}
	bool inDocContext( context.find( "//doc " ) == 0 );
	HLineRunner::words_t hints( repl->completion_words( prefix_, HString( prefix ), false ) );
	HUTF8String utf8;
	HString doc;
	for ( yaal::hcore::HString h : hints ) {
		doc.clear();
		h.trim_right( "(" );
		HString ask( h );
		int long dotIdx( ask.find( '.'_ycp ) );
		int long toStrip( 0 );
		if ( dotIdx != HString::npos ) {
			HString obj( repl->line_runner()->symbol_type( ask.left( dotIdx ) ) );
			HString method( ask.mid( dotIdx + 1 ) );
			ask.assign( obj ).append( '.' ).append( method );
			toStrip = method.get_length();
		} else if ( repl->line_runner()->symbol_kind( ask ) != HDescription::SYMBOL_KIND::CLASS ) {
			toStrip = h.get_length();
		} else {
			doc.assign( " - " );
		}
		doc.append( repl->line_runner()->doc( ask, inDocContext ) );
		h.shift_left( prefix.get_length() );
		doc.replace( "*", "" );
		doc.shift_left( toStrip );
		utf8.assign( h.append( doc ) );
		replxx_add_hint( hints_, utf8.c_str() );
	}
	*color_ = _replxxColors_.at( color( GROUP::HINT ) );
	return;
}

void colorize( char const* line_, replxx_color::color* colors_, int size_, void* ) {
	M_PROLOG
	colors_t colors;
	HString line( line_ );
	::huginn::colorize( line, colors );
	for ( int i( 0 ); i < size_; ++ i ) {
		colors_[i] = static_cast<replxx_color::color>( colors[i] );
	}
	return;
	M_EPILOG
}

#elif defined( USE_EDITLINE )

char* el_make_prompt( EditLine* el_ ) {
	void* p( nullptr );
	el_get( el_, EL_CLIENTDATA, &p );
	return ( const_cast<char*>( static_cast<HRepl*>( p )->prompt() ) );
}

int common_prefix_length( HString const& str1_, HString const& str2_, int max_ ) {
	int len( 0 );
	max_ = min( static_cast<int>( str1_.get_length() ), static_cast<int>( str2_.get_length() ), max_ );
	while ( ( len < max_ ) && ( str1_[len] == str2_[len] ) ) {
		++ len;
	}
	return ( len );
}

int complete( EditLine* el_, int ) {
	void* p( nullptr );
	el_get( el_, EL_CLIENTDATA, &p );
	HRepl* repl( static_cast<HRepl*>( p ) );
	LineInfo const* li( el_line( el_ ) );
	HString context( li->buffer, li->cursor - li->buffer );
	int long stemStart( context.find_last_one_of( BREAK_CHARS ) );
	while ( ( stemStart >= 0 ) && ( SPECIAL_PREFIXES.find( context[stemStart] ) != HString::npos ) ) {
		-- stemStart;
	}
	HString prefix( stemStart != HString::npos ? context.substr( stemStart + 1, li->cursor - li->buffer - stemStart ) : context );
	int prefixLen( static_cast<int>( prefix.get_length() ) );
	HLineRunner::words_t completions( repl->completion_words( yaal::move( context ), yaal::move( prefix ) ) );
	HUTF8String utf8;
	HString buf( ! completions.is_empty() ? completions.front() : HString() );
	int commonPrefixLength( meta::max_signed<int>::value );
	int maxLen( 0 );
	for ( HString const& w : completions ) {
		commonPrefixLength = min( common_prefix_length( buf, w, commonPrefixLength ), static_cast<int>( w.get_length() ) );
		maxLen = max( maxLen, static_cast<int>( w.get_length() ) );
	}
	if ( ( commonPrefixLength > prefixLen ) || ( completions.get_size() == 1 ) ) {
		buf.erase( commonPrefixLength );
		if ( ! buf.is_empty() ) {
			el_deletestr( el_, prefixLen );
			el_insertstr( el_, HUTF8String( buf ).c_str() );
		}
	} else {
		REPL_print( "\n" );
		HTerminal t;
		int termWidth( t.exists() ? t.size().second : 0 );
		int colWidth( maxLen + 2 );
		int cols( max( termWidth / colWidth, 1 ) );
		int rows( static_cast<int>( completions.get_size() + cols - 1 ) / cols );
		sort( completions.begin(), completions.end() );
		bool needNl( false );
		for ( int i( 0 ), c( 0 ), WC( static_cast<int>( completions.get_size() ) ); i < WC; ++ c ) {
			int n( ( c % cols ) * rows + c / cols );
			if ( n < WC ) {
				if ( ! setup._noColor ) {
					buf.assign( *ansi::brightmagenta ).append( completions[n], 0, commonPrefixLength ).append( *ansi::reset ).append( completions[n], commonPrefixLength );
				} else {
					buf.assign( completions[n] );
				}
				buf.append( colWidth - completions[n].get_length(), ' '_ycp );
				utf8.assign( buf );
				REPL_print( "%s", utf8.c_str() );
				++ i;
				needNl = true;
			}
			if ( ( c % cols ) == ( cols - 1 ) ) {
				REPL_print( "\n" );
				needNl = false;
			}
		}
		if ( needNl ) {
			REPL_print( "\n" );
		}
	}
	return ( CC_REDISPLAY );
}

#else

HRepl* _repl_( nullptr );

void redisplay( void ) {
	HUTF8String copy( rl_line_buffer );
	rl_redisplay();
	HString line( colorize( rl_line_buffer ) );
	cout << *ansi::save << flush;
	rl_clear_visible_line();
	REPL_print( "%s%s", rl_display_prompt, HUTF8String( line ).c_str() );
	fflush( stdout );
	cout << *ansi::restore << flush;
}

char* rl_completion_words( char const* prefix_, int state_ ) {
	static int index( 0 );
	static HString prefix;
	rl_completion_suppress_append = 1;
	static HLineRunner::words_t words;
	if ( state_ == 0 ) {
		prefix = prefix_;
		words = _repl_->completion_words( rl_line_buffer, yaal::move( prefix ) );
		index = 0;
	}
	char* p( nullptr );
	if ( index < words.get_size() ) {
		p = strdup( HUTF8String( words[index] ).c_str() + ( ( words.get_size() > 0 ) && ( words[index].front() == '/' ) ? 1 : 0 ) );
	}
	++ index;
	return ( p );
}

#endif

}

HRepl::HRepl( void )
	: _elData( make_tuple( static_cast<void*>( 0 ), static_cast<void*>( 0 ), 0, buf_t() ) )
	, _lineRunner( nullptr )
	, _shell( nullptr )
	, _prompt( nullptr )
	, _completer( nullptr )
	, _historyPath() {
#ifdef USE_REPLXX
	replxx_install_window_change_handler();
	replxx_set_word_break_characters( BREAK_CHARS_RAW );
	replxx_set_no_color( setup._noColor ? 1 : 0 );
#elif defined( USE_EDITLINE )
	_buf.reset( new char[ sizeof ( HistEvent ) ] );
	_el = el_init( PACKAGE_NAME, stdin, stdout, stderr );
	_hist = history_init();
	el_set( _el, EL_EDITOR, "emacs" );
	el_set( _el, EL_SIGNAL, SIGWINCH );
	el_set( _el, EL_CLIENTDATA, this );
	el_set( _el, EL_HIST, &history, _hist );
	el_set( _el, EL_PROMPT_ESC, el_make_prompt, 1 );
	el_set( _el, EL_BIND, "\\e[1;5D", "ed-prev-word", nullptr );
	el_set( _el, EL_BIND, "\\e[1;5C", "em-next-word", nullptr );
	el_set( _el, EL_BIND, "\\ep", "ed-search-prev-history", nullptr );
	el_set( _el, EL_BIND, "\\en", "ed-search-next-history", nullptr );
	history( _hist, _histBuf, H_SETSIZE, 1000 );
	history( _hist, _histBuf, H_SETUNIQUE, 1 );
#else
	_repl_ = this;
	rl_readline_name = PACKAGE_NAME;
	if ( ! setup._noColor ) {
		rl_redisplay_function = redisplay;
	}
	rl_basic_word_break_characters = BREAK_CHARS_RAW;
#endif
}

HRepl::~HRepl( void ) {
	if ( ! _historyPath.is_empty() ) {
		REPL_save_history( HUTF8String( _historyPath ).c_str() );
	}
#ifdef USE_REPLXX
	replxx_history_free();
#elif defined( USE_EDITLINE )
	history_end( _hist );
	el_end( _el );
#endif
}

void HRepl::set_shell( HShell* shell_ ) {
	_shell = shell_;
}

void HRepl::set_line_runner( HLineRunner* lineRunner_ ) {
	_lineRunner = lineRunner_;
}

void HRepl::set_completer( completion_words_t completer_ ) {
#ifdef USE_REPLXX
	replxx_set_completion_callback( replxx_completion_words, this );
	if ( ! setup._noColor ) {
		replxx_set_highlighter_callback( colorize, this );
		replxx_set_hint_callback( find_hints, this );
	}
	replxx_set_special_prefixes( SPECIAL_PREFIXES_RAW );
#elif defined( USE_EDITLINE )
	el_set( _el, EL_ADDFN, "complete", "Command completion", complete );
	el_set( _el, EL_BIND, "^I", "complete", nullptr );
#else
	rl_completion_entry_function = rl_completion_words;
	rl_special_prefixes = SPECIAL_PREFIXES_RAW;
#endif
	_completer = completer_;
}

void HRepl::set_history_path( yaal::hcore::HString const& historyPath_ ) {
	_historyPath = historyPath_;
	if ( ! _historyPath.is_empty() ) {
		REPL_load_history( HUTF8String( _historyPath ).c_str() );
	}
}

HLineRunner::words_t HRepl::completion_words( yaal::hcore::HString&& context_, yaal::hcore::HString&& prefix_, bool shell_ ) {
	HScopedValueReplacement<HShell*> svr( _shell, shell_ ? _shell : nullptr );
	return ( _completer( yaal::move( context_ ), yaal::move( prefix_ ), this ) );
}

bool HRepl::input( yaal::hcore::HString& line_, char const* prompt_ ) {
	char* rawLine( nullptr );
	_prompt = prompt_;
	bool gotLine( ( rawLine = REPL_get_input( prompt_ ) ) != nullptr );
	if ( gotLine ) {
		line_ = rawLine;
		int len( static_cast<int>( strlen( rawLine ) ) );
		if ( ( len > 0 ) && ( rawLine[len - 1] == '\n' ) ) {
			line_.pop_back();
			rawLine[len - 1] = 0;
		}
		if ( ( rawLine[0] != 0 ) && ( rawLine[0] != ' ' ) ) {
			REPL_add_history( rawLine );
		}
#if defined( USE_REPLXX ) || ! ( defined( USE_EDITLINE ) || defined( __MSVCXX__ ) )
		REPL_free( rawLine );
#endif
	}
	return ( gotLine );
}

void HRepl::print( char const* str_ ) {
	REPL_print( "%s\n", str_ );
}

}

