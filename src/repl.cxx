/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <cstring>
#include <cstdio>

#include "repl.hxx"
M_VCSID( "$Id: " __ID__ " $" )
M_VCSID( "$Id: " __TID__ " $" )

#include "colorize.hxx"
#include "shell.hxx"
#include "setup.hxx"

#ifdef USE_REPLXX
#	define REPL_load_history _replxx.history_load
#	define REPL_save_history _replxx.history_save
#	define REPL_add_history _replxx.history_add
#	define REPL_ignore_start ""
#	define REPL_ignore_end ""
#	define REPL_get_input _replxx.input
#	define REPL_print _replxx.print
using namespace replxx;
#elif defined( USE_EDITLINE )
#	include <yaal/tools/hterminal.hxx>
#	include <histedit.h>
#	include <signal.h>
#	define REPL_load_history( file ) history( _hist, &_histEvent, H_LOAD, file )
#	define REPL_save_history( file ) history( _hist, &_histEvent, H_SAVE, file )
#	define REPL_add_history( line ) history( _hist, &_histEvent, H_ENTER, line )
#	define REPL_ignore_start ""
#	define REPL_ignore_end ""
#	define REPL_get_input( ... ) el_gets( _el, &_count )
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
char const SPECIAL_PREFIXES_RAW[] = "\\^/";
HString const SPECIAL_PREFIXES( SPECIAL_PREFIXES_RAW );

#ifdef USE_REPLXX

typedef yaal::hcore::HHashMap<yaal::tools::COLOR::color_t, Replxx::Color> replxx_colors_t;
replxx_colors_t _replxxColors_ = {
	{ COLOR::FG_BLACK,         Replxx::Color::BLACK },
	{ COLOR::FG_RED,           Replxx::Color::RED },
	{ COLOR::FG_GREEN,         Replxx::Color::GREEN },
	{ COLOR::FG_BROWN,         Replxx::Color::BROWN },
	{ COLOR::FG_BLUE,          Replxx::Color::BLUE },
	{ COLOR::FG_MAGENTA,       Replxx::Color::MAGENTA },
	{ COLOR::FG_CYAN,          Replxx::Color::CYAN },
	{ COLOR::FG_LIGHTGRAY,     Replxx::Color::LIGHTGRAY },
	{ COLOR::FG_GRAY,          Replxx::Color::GRAY },
	{ COLOR::FG_BRIGHTRED,     Replxx::Color::BRIGHTRED },
	{ COLOR::FG_BRIGHTGREEN,   Replxx::Color::BRIGHTGREEN },
	{ COLOR::FG_YELLOW,        Replxx::Color::YELLOW },
	{ COLOR::FG_BRIGHTBLUE,    Replxx::Color::BRIGHTBLUE },
	{ COLOR::FG_BRIGHTMAGENTA, Replxx::Color::BRIGHTMAGENTA },
	{ COLOR::FG_BRIGHTCYAN,    Replxx::Color::BRIGHTCYAN },
	{ COLOR::FG_WHITE,         Replxx::Color::WHITE }
};

Replxx::completions_t replxx_completion_words( std::string const& prefix_, int offset_, void* data_ ) {
	HString prefix( prefix_.c_str() );
	prefix.shift_left( offset_ );
	HLineRunner::words_t completions( static_cast<HRepl*>( data_ )->completion_words( prefix_.c_str(), yaal::move( prefix ) ) );
	HUTF8String utf8;
	Replxx::completions_t replxxCompletions;
	for ( yaal::hcore::HString const& c : completions ) {
		utf8.assign( c );
		replxxCompletions.emplace_back( utf8.c_str() );
	}
	return ( replxxCompletions );
}

Replxx::hints_t find_hints( std::string const& prefix_, int offset_, Replxx::Color& color_, void* data_ ) {
	HRepl* repl( static_cast<HRepl*>( data_ ) );
	HString context( prefix_.c_str() );
	HString prefix( prefix_.c_str() );
	prefix.shift_left( offset_ );
	if ( prefix.is_empty() || ( prefix == "." ) ) {
		return ( Replxx::hints_t() );
	}
	bool inDocContext( context.find( "//doc " ) == 0 );
	HLineRunner::words_t hints( repl->completion_words( prefix_.c_str(), HString( prefix ), false ) );
	HUTF8String utf8;
	HString doc;
	Replxx::hints_t replxxHints;
	for ( yaal::hcore::HString h : hints ) {
		doc.clear();
		h.trim_right( "()" );
		HString ask( h );
		int long dotIdx( ask.find_last( '.'_ycp ) );
		int long toStrip( 0 );
		if ( dotIdx != HString::npos ) {
			HString obj( repl->line_runner()->symbol_type_name( ask.left( dotIdx ) ) );
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
		replxxHints.emplace_back( utf8.c_str() );
	}
	color_ = _replxxColors_.at( color( GROUP::HINT ) );
	return ( replxxHints );
}

void replxx_colorize( std::string const& line_, Replxx::colors_t& colors_, void* data_ ) {
	M_PROLOG
	HRepl* repl( static_cast<HRepl*>( data_ ) );
	HString line( line_.c_str() );
	colors_t colors;
	if ( ! ( repl->shell() && repl->shell()->is_command( line ) ) ) {
		::huginn::colorize( line, colors );
	} else {
		shell_colorize( line, colors );
	}
	int size( static_cast<int>( colors_.size() ) );
	for ( int i( 0 ); i < size; ++ i ) {
		colors_[static_cast<size_t>( i )] = static_cast<Replxx::Color>( colors[i] );
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
#ifdef USE_REPLXX
	: _replxx()
	, _lineRunner( nullptr )
#elif defined( USE_EDITLINE )
	: _el( el_init( PACKAGE_NAME, stdin, stdout, stderr ) )
	, _hist( history_init() )
	, _histEvent()
	, _count( 0 )
	, _lineRunner( nullptr )
#else
	: _lineRunner( nullptr )
#endif
	, _shell( nullptr )
	, _prompt( nullptr )
	, _completer( nullptr )
	, _historyPath() {
#ifdef USE_REPLXX
	_replxx.install_window_change_handler();
	_replxx.set_word_break_characters( BREAK_CHARS_RAW );
	_replxx.set_no_color( setup._noColor ? 1 : 0 );
#elif defined( USE_EDITLINE )
	el_set( _el, EL_EDITOR, "emacs" );
	el_set( _el, EL_SIGNAL, SIGWINCH );
	el_set( _el, EL_CLIENTDATA, this );
	el_set( _el, EL_HIST, &history, _hist );
	el_set( _el, EL_PROMPT_ESC, el_make_prompt, 1 );
	el_set( _el, EL_BIND, "\\e[1;5D", "ed-prev-word", nullptr );
	el_set( _el, EL_BIND, "\\e[1;5C", "em-next-word", nullptr );
	el_set( _el, EL_BIND, "\\ep", "ed-search-prev-history", nullptr );
	el_set( _el, EL_BIND, "\\en", "ed-search-next-history", nullptr );
	history( _hist, &_histEvent, H_SETSIZE, 1000 );
	history( _hist, &_histEvent, H_SETUNIQUE, 1 );
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
#ifdef USE_EDITLINE
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
	_replxx.set_completion_callback( replxx_completion_words, this );
	if ( ! setup._noColor ) {
		_replxx.set_highlighter_callback( replxx_colorize, this );
		_replxx.set_hint_callback( find_hints, this );
	}
	_replxx.set_special_prefixes( SPECIAL_PREFIXES_RAW );
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
	_prompt = prompt_;
	char const* rawLine( nullptr );
	bool gotLine( false );
	do {
		rawLine = REPL_get_input( prompt_ );
	} while ( ! ( gotLine = ( rawLine != nullptr ) ) && ( errno == EAGAIN ) );
	if ( gotLine ) {
		line_ = rawLine;
		int len( static_cast<int>( strlen( rawLine ) ) );
		if ( ( len > 0 ) && ( rawLine[len - 1] == '\n' ) ) {
			line_.pop_back();
			const_cast<char*>( rawLine )[len] = 0;
		}
		if ( ! line_.is_empty() && ( line_.front() != ' ' ) ) {
			REPL_add_history( rawLine );
		}
#if defined( USE_READLINE )
		memory::free0( rawLine );
#endif
	}
	return ( gotLine );
}

void HRepl::print( char const* str_ ) {
	REPL_print( "%s\n", str_ );
}

}

