/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <cstring>
#include <cstdio>

#include <yaal/hcore/hcore.hxx>
#include <yaal/tools/stringalgo.hxx>
#include <yaal/tools/streamtools.hxx>

#include "repl.hxx"
M_VCSID( "$Id: " __ID__ " $" )
M_VCSID( "$Id: " __TID__ " $" )

#include "colorize.hxx"
#include "systemshell.hxx"
#include "setup.hxx"

#ifdef USE_REPLXX
#	define REPL_load_history _replxx.history_load
#	define REPL_add_history _replxx.history_add
#	define REPL_get_input _replxx.input
#	define REPL_print _replxx.print
using namespace replxx;
#elif defined( USE_EDITLINE )
#	include <yaal/tools/hterminal.hxx>
#	include <histedit.h>
#	include <signal.h>
#	define REPL_load_history( file ) ::history( _hist, &_histEvent, H_LOAD, file )
#	define REPL_add_history( line ) ::history( _hist, &_histEvent, H_ENTER, line )
#	define REPL_get_input( ... ) el_gets( _el, &_count )
#	define REPL_print printf
#	define REPL_bind_key( seq, key, fun, name ) \
	do { \
		_keyTable.insert( make_pair( key, action_t() ) ); \
		_keyBindingDispatchInfo.insert( make_pair( key, OKeyBindDispatchInfo{ seq, name } ) ); \
	} while ( false )
#	define REPL_get_data( ud ) void* p( nullptr ); el_get( ( ud ), EL_CLIENTDATA, &p );
#else
#	include <readline/readline.h>
#	include <readline/history.h>
#	define REPL_load_history read_history
#	define REPL_add_history add_history
#	define REPL_get_input readline
#	define REPL_print printf
#	define REPL_bind_key( seq, key, fun, name ) \
	do { \
		_keyTable.insert( make_pair( key, action_t() ) ); \
		_keyBindingDispatchInfo.insert( make_pair( key, OKeyBindDispatchInfo{ seq, fun } ) ); \
	} while ( false )
#	define REPL_get_data( ud ) static_cast<void>( ud ); HRepl* p( _repl_ )
#endif

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;
using namespace yaal::tools::huginn;

namespace yaal { namespace hcore {
template<>
hash<yaal::tools::COLOR::color_t>::hash_value_type hash<yaal::tools::COLOR::color_t>::operator () ( yaal::tools::COLOR::color_t const& val_ ) const {
	return ( static_cast<hash_value_type>( val_ ) );
}
} }

namespace huginn {

namespace {

/*
 * The dot character ('.') shall be explicitly removed from set of word breaking
 * characters to support `obj.member` semantics.
 * The underscore character ('_') removed to support standard idenfifiers (`some_symbol`)
 * from programming languages.
 */
char const BREAK_CHARS_HUGINN_RAW[] = " \t\v\f\a\b\r\n`~!@#$%^&*()-=+[{]}\\|;:'\",<>/?";
character_class_t const BREAK_CHARACTERS_HUGINN_CLASS( BREAK_CHARS_HUGINN_RAW, static_cast<int>( sizeof ( BREAK_CHARS_HUGINN_RAW ) - 1 ) );
char const BREAK_CHARACTERS_SHELL_RAW[] = " \t\v\f\a\b\r\n\"\\'`@$><=;|&{(/";
character_class_t const BREAK_CHARACTERS_SHELL_CLASS( BREAK_CHARACTERS_SHELL_RAW, static_cast<int>( sizeof ( BREAK_CHARACTERS_SHELL_RAW ) - 1 ) );
char const SPECIAL_PREFIXES_RAW[] = "\\^";
HString const SPECIAL_PREFIXES( SPECIAL_PREFIXES_RAW );

#ifdef USE_REPLXX

Replxx::ACTION_RESULT do_nothing( char32_t ) {
	return ( Replxx::ACTION_RESULT::CONTINUE );
}

#endif

}

int context_length( yaal::hcore::HString const& input_, CONTEXT_TYPE contextType_ ) {
	M_PROLOG
	if ( input_.is_empty() ) {
		return ( 0 );
	}
	character_class_t const& cc( contextType_ == CONTEXT_TYPE::HUGINN ? BREAK_CHARACTERS_HUGINN_CLASS : BREAK_CHARACTERS_SHELL_CLASS );
	bool skip( false );
	int prefixStart( 0 );
	for ( int i( 0 ), len( static_cast<int>( input_.get_length() ) ); i < len; ++ i ) {
		if ( skip ) {
			skip = false;
			continue;
		}
		code_point_t c( input_[i] );
		if ( c == '\\' ) {
			skip = true;
			continue;
		}
		if ( cc.has( c ) ) {
			prefixStart = i + 1;
		}
	}
	while ( ( prefixStart > 0 ) && ( SPECIAL_PREFIXES.find( input_[prefixStart - 1] ) != HString::npos ) ) {
		-- prefixStart;
	}
	return ( static_cast<int>( input_.get_length() ) - prefixStart );
	M_EPILOG
}

namespace {

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

Replxx::completions_t replxx_completion_words( std::string const& context_, int& contextLen_, void* data_ ) {
	M_PROLOG
	HString prefix( context_.c_str() );
	contextLen_ = context_length( prefix, CONTEXT_TYPE::HUGINN );
	prefix.shift_left( prefix.get_length() - contextLen_ );
	CONTEXT_TYPE contextType( CONTEXT_TYPE::HUGINN );
	HRepl::completions_t completions( static_cast<HRepl*>( data_ )->completion_words( context_.c_str(), yaal::move( prefix ), contextLen_, contextType ) );
	HUTF8String utf8;
	Replxx::completions_t replxxCompletions;
	for ( HRepl::HCompletion const& c : completions ) {
		utf8.assign( c.text() );
		replxxCompletions.emplace_back( utf8.c_str(), static_cast<Replxx::Color>( c.color() ) );
	}
	return ( replxxCompletions );
	M_EPILOG
}

#elif defined( USE_EDITLINE )

char* el_make_prompt( EditLine* el_ ) {
	void* p( nullptr );
	el_get( el_, EL_CLIENTDATA, &p );
	return ( const_cast<char*>( static_cast<HRepl*>( p )->prompt() ) );
}

int complete( EditLine* el_, int ) {
	void* p( nullptr );
	el_get( el_, EL_CLIENTDATA, &p );
	HRepl* repl( static_cast<HRepl*>( p ) );
	LineInfo const* li( el_line( el_ ) );
	HString context( li->buffer, li->cursor - li->buffer );
	int contextLen( context_length( context, CONTEXT_TYPE::HUGINN ) );
	HString prefix( context.right( contextLen ) );
	int prefixLen( static_cast<int>( prefix.get_length() ) );
	if ( context.starts_with( "//" ) && ( ( context.get_length() - prefix.get_length() ) == 2 ) ) {
		prefixLen += 2;
	}
	CONTEXT_TYPE contextType( CONTEXT_TYPE::HUGINN );
	HRepl::completions_t completions( repl->completion_words( yaal::move( context ), yaal::move( prefix ), contextLen, contextType ) );
	HUTF8String utf8;
	HString buf( ! completions.is_empty() ? completions.front().text() : HString() );
	int maxLen( 0 );
	for ( HRepl::HCompletion const& c : completions ) {
		maxLen = max( maxLen, static_cast<int>( c.text().get_length() ) );
	}
	HString commonPrefix( string::longest_common_prefix( view( completions, []( HRepl::HCompletion const& c_ ) -> HString const& { return ( c_.text() ); } ) ) );
	HString::size_type commonPrefixLength( commonPrefix.get_length() );
	if ( ( commonPrefixLength > prefixLen ) || ( completions.get_size() == 1 ) ) {
		buf.erase( commonPrefixLength );
		if ( ! buf.is_empty() ) {
			el_deletestr( el_, prefixLen );
			el_insertstr( el_, HUTF8String( buf ).c_str() );
		}
	} else {
		REPL_print( "\n" );
		HTerminal t;
		int termWidth( t.exists() ? t.size().columns() : 0 );
		int colWidth( maxLen + 2 );
		int cols( max( termWidth / colWidth, 1 ) );
		int rows( static_cast<int>( completions.get_size() + cols - 1 ) / cols );
		sort( completions.begin(), completions.end() );
		bool needNl( false );
		for ( int i( 0 ), c( 0 ), WC( static_cast<int>( completions.get_size() ) ); i < WC; ++ c ) {
			int n( ( c % cols ) * rows + c / cols );
			if ( n < WC ) {
				if ( ! setup._noColor ) {
					buf.assign( *ansi::brightmagenta ).append( completions[n].text(), 0, commonPrefixLength ).append( *ansi::reset ).append( completions[n].text(), commonPrefixLength );
				} else {
					buf.assign( completions[n].text() );
				}
				buf.append( colWidth - completions[n].text().get_length(), ' '_ycp );
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

namespace {

HRepl* _repl_( nullptr );
HRepl::completions_t _completions_;

}

char* rl_word_break_characters( void ) {
	static HString previousLine = "\t\t\t~~~";
	HString currentLine( rl_line_buffer );
	if ( currentLine != previousLine ) {
		CONTEXT_TYPE contextType( CONTEXT_TYPE::HUGINN );
		static HString context;
		static HString prefix;
		context.assign( currentLine );
		int contextLen( context_length( context, _repl_->shell() ? CONTEXT_TYPE::SHELL : CONTEXT_TYPE::HUGINN ) );
		prefix = context.right( contextLen );
		_completions_ = _repl_->completion_words( yaal::move( context ), yaal::move( prefix ), contextLen, contextType );
		previousLine = yaal::move( currentLine );
		if ( contextType == CONTEXT_TYPE::HUGINN ) {
			rl_basic_word_break_characters = const_cast<char*>( BREAK_CHARACTERS_HUGINN_CLASS.data() );
			rl_completer_word_break_characters = const_cast<char*>( BREAK_CHARACTERS_HUGINN_CLASS.data() );
		} else if ( contextType == CONTEXT_TYPE::SHELL ) {
			rl_basic_word_break_characters = const_cast<char*>( BREAK_CHARACTERS_SHELL_CLASS.data() );
			rl_completer_word_break_characters = const_cast<char*>( BREAK_CHARACTERS_SHELL_CLASS.data() );
		}
		rl_stuff_char( 0 );
	}
	return ( const_cast<char*>( rl_basic_word_break_characters ) );
}

char* rl_completion_words( char const*, int state_ ) {
	static int index( 0 );
	rl_completion_suppress_append = 1;
	if ( state_ == 0 ) {
		rl_word_break_characters();
		index = 0;
	}
	char* p( nullptr );
	if ( index < _completions_.get_size() ) {
		HString const& word( _completions_[index].text() );
		int skip( word.starts_with( "//" ) ? 2 : 0 );
		p = strdup( HUTF8String( word ).c_str() + skip );
	}
	++ index;
	return ( p );
}

#endif

}

HRepl::HRepl( void )
	: _inputSoFar()
#ifdef USE_REPLXX
	, _replxx()
	, _keyTable({
		{ "C-a",   Replxx::KEY::control( 'A' ) },
		{ "C-b",   Replxx::KEY::control( 'B' ) },
		{ "C-c",   Replxx::KEY::control( 'C' ) },
		{ "C-d",   Replxx::KEY::control( 'D' ) },
		{ "C-e",   Replxx::KEY::control( 'E' ) },
		{ "C-f",   Replxx::KEY::control( 'F' ) },
		{ "C-g",   Replxx::KEY::control( 'G' ) },
		{ "C-h",   Replxx::KEY::control( 'H' ) },
		{ "C-i",   Replxx::KEY::control( 'I' ) },
		{ "C-j",   Replxx::KEY::control( 'J' ) },
		{ "C-k",   Replxx::KEY::control( 'K' ) },
		{ "C-l",   Replxx::KEY::control( 'L' ) },
		{ "C-m",   Replxx::KEY::control( 'M' ) },
		{ "C-n",   Replxx::KEY::control( 'N' ) },
		{ "C-o",   Replxx::KEY::control( 'O' ) },
		{ "C-p",   Replxx::KEY::control( 'P' ) },
		{ "C-q",   Replxx::KEY::control( 'Q' ) },
		{ "C-r",   Replxx::KEY::control( 'R' ) },
		{ "C-s",   Replxx::KEY::control( 'S' ) },
		{ "C-t",   Replxx::KEY::control( 'T' ) },
		{ "C-u",   Replxx::KEY::control( 'U' ) },
		{ "C-v",   Replxx::KEY::control( 'V' ) },
		{ "C-w",   Replxx::KEY::control( 'W' ) },
		{ "C-x",   Replxx::KEY::control( 'X' ) },
		{ "C-y",   Replxx::KEY::control( 'Y' ) },
		{ "C-z",   Replxx::KEY::control( 'Z' ) },
		{ "C-0",   Replxx::KEY::control( '0' ) },
		{ "C-1",   Replxx::KEY::control( '1' ) },
		{ "C-2",   Replxx::KEY::control( '2' ) },
		{ "C-3",   Replxx::KEY::control( '3' ) },
		{ "C-4",   Replxx::KEY::control( '4' ) },
		{ "C-5",   Replxx::KEY::control( '5' ) },
		{ "C-6",   Replxx::KEY::control( '6' ) },
		{ "C-7",   Replxx::KEY::control( '7' ) },
		{ "C-8",   Replxx::KEY::control( '8' ) },
		{ "C-9",   Replxx::KEY::control( '9' ) },
		{ "M-a",   Replxx::KEY::meta( 'a' ) },
		{ "M-b",   Replxx::KEY::meta( 'b' ) },
		{ "M-c",   Replxx::KEY::meta( 'm' ) },
		{ "M-d",   Replxx::KEY::meta( 'd' ) },
		{ "M-e",   Replxx::KEY::meta( 'e' ) },
		{ "M-f",   Replxx::KEY::meta( 'f' ) },
		{ "M-g",   Replxx::KEY::meta( 'g' ) },
		{ "M-h",   Replxx::KEY::meta( 'h' ) },
		{ "M-i",   Replxx::KEY::meta( 'i' ) },
		{ "M-j",   Replxx::KEY::meta( 'j' ) },
		{ "M-k",   Replxx::KEY::meta( 'k' ) },
		{ "M-l",   Replxx::KEY::meta( 'l' ) },
		{ "M-m",   Replxx::KEY::meta( 'm' ) },
		{ "M-n",   Replxx::KEY::meta( 'n' ) },
		{ "M-o",   Replxx::KEY::meta( 'o' ) },
		{ "M-p",   Replxx::KEY::meta( 'p' ) },
		{ "M-q",   Replxx::KEY::meta( 'q' ) },
		{ "M-r",   Replxx::KEY::meta( 'r' ) },
		{ "M-s",   Replxx::KEY::meta( 's' ) },
		{ "M-t",   Replxx::KEY::meta( 't' ) },
		{ "M-u",   Replxx::KEY::meta( 'u' ) },
		{ "M-v",   Replxx::KEY::meta( 'v' ) },
		{ "M-w",   Replxx::KEY::meta( 'w' ) },
		{ "M-x",   Replxx::KEY::meta( 'x' ) },
		{ "M-y",   Replxx::KEY::meta( 'y' ) },
		{ "M-z",   Replxx::KEY::meta( 'z' ) },
		{ "M-0",   Replxx::KEY::meta( '0' ) },
		{ "M-1",   Replxx::KEY::meta( '1' ) },
		{ "M-2",   Replxx::KEY::meta( '2' ) },
		{ "M-3",   Replxx::KEY::meta( '3' ) },
		{ "M-4",   Replxx::KEY::meta( '4' ) },
		{ "M-5",   Replxx::KEY::meta( '5' ) },
		{ "M-6",   Replxx::KEY::meta( '6' ) },
		{ "M-7",   Replxx::KEY::meta( '7' ) },
		{ "M-8",   Replxx::KEY::meta( '8' ) },
		{ "M-9",   Replxx::KEY::meta( '9' ) },
		{ "F1",    Replxx::KEY::F1 + 0 },
		{ "F2",    Replxx::KEY::F2 + 0 },
		{ "F3",    Replxx::KEY::F3 + 0 },
		{ "F4",    Replxx::KEY::F4 + 0 },
		{ "F5",    Replxx::KEY::F5 + 0 },
		{ "F6",    Replxx::KEY::F6 + 0 },
		{ "F7",    Replxx::KEY::F7 + 0 },
		{ "F8",    Replxx::KEY::F8 + 0 },
		{ "F9",    Replxx::KEY::F9 + 0 },
		{ "F10",   Replxx::KEY::F10 + 0 },
		{ "F11",   Replxx::KEY::F11 + 0 },
		{ "F12",   Replxx::KEY::F12 + 0 },
		{ "S-F1",  Replxx::KEY::shift( Replxx::KEY::F1 ) },
		{ "S-F2",  Replxx::KEY::shift( Replxx::KEY::F2 ) },
		{ "S-F3",  Replxx::KEY::shift( Replxx::KEY::F3 ) },
		{ "S-F4",  Replxx::KEY::shift( Replxx::KEY::F4 ) },
		{ "S-F5",  Replxx::KEY::shift( Replxx::KEY::F5 ) },
		{ "S-F6",  Replxx::KEY::shift( Replxx::KEY::F6 ) },
		{ "S-F7",  Replxx::KEY::shift( Replxx::KEY::F7 ) },
		{ "S-F8",  Replxx::KEY::shift( Replxx::KEY::F8 ) },
		{ "S-F9",  Replxx::KEY::shift( Replxx::KEY::F9 ) },
		{ "S-F10", Replxx::KEY::shift( Replxx::KEY::F10 ) },
		{ "S-F11", Replxx::KEY::shift( Replxx::KEY::F11 ) },
		{ "S-F12", Replxx::KEY::shift( Replxx::KEY::F12 ) },
		{ "C-F1",  Replxx::KEY::control( Replxx::KEY::F1 ) },
		{ "C-F2",  Replxx::KEY::control( Replxx::KEY::F2 ) },
		{ "C-F3",  Replxx::KEY::control( Replxx::KEY::F3 ) },
		{ "C-F4",  Replxx::KEY::control( Replxx::KEY::F4 ) },
		{ "C-F5",  Replxx::KEY::control( Replxx::KEY::F5 ) },
		{ "C-F6",  Replxx::KEY::control( Replxx::KEY::F6 ) },
		{ "C-F7",  Replxx::KEY::control( Replxx::KEY::F7 ) },
		{ "C-F8",  Replxx::KEY::control( Replxx::KEY::F8 ) },
		{ "C-F9",  Replxx::KEY::control( Replxx::KEY::F9 ) },
		{ "C-F10", Replxx::KEY::control( Replxx::KEY::F10 ) },
		{ "C-F11", Replxx::KEY::control( Replxx::KEY::F11 ) },
		{ "C-F12", Replxx::KEY::control( Replxx::KEY::F12 ) }
	})
#elif defined( USE_EDITLINE )
	, _el( el_init( PACKAGE_NAME, stdin, stdout, stderr ) )
	, _hist( history_init() )
	, _histEvent()
	, _count( 0 )
	, _keyTable()
	, _keyBindingDispatchInfo()
#else
	, _keyTable()
	, _keyBindingDispatchInfo()
#endif
	, _lineRunner( nullptr )
	, _shell( nullptr )
	, _prompt( nullptr )
	, _completer( nullptr )
	, _historyPath() {
#ifdef USE_REPLXX
	_replxx.install_window_change_handler();
	_replxx.set_no_color( setup._noColor ? 1 : 0 );
#elif defined( USE_EDITLINE )
	el_set( _el, EL_EDITOR, "emacs" );
	el_set( _el, EL_SIGNAL, SIGWINCH );
	el_set( _el, EL_CLIENTDATA, this );
	el_set( _el, EL_HIST, &::history, _hist );
	el_set( _el, EL_PROMPT_ESC, el_make_prompt, 1 );
	el_set( _el, EL_BIND, "\\e[1;5D", "ed-prev-word", nullptr );
	el_set( _el, EL_BIND, "\\e[1;5C", "em-next-word", nullptr );
	el_set( _el, EL_BIND, "\\ep", "ed-search-prev-history", nullptr );
	el_set( _el, EL_BIND, "\\en", "ed-search-next-history", nullptr );
	el_set( _el, EL_ADDFN, "repl_key_C_a",  "", HRepl::handle_key_C_a );
	el_set( _el, EL_ADDFN, "repl_key_C_b",  "", HRepl::handle_key_C_b );
	el_set( _el, EL_ADDFN, "repl_key_C_c",  "", HRepl::handle_key_C_c );
	el_set( _el, EL_ADDFN, "repl_key_C_d",  "", HRepl::handle_key_C_d );
	el_set( _el, EL_ADDFN, "repl_key_C_e",  "", HRepl::handle_key_C_e );
	el_set( _el, EL_ADDFN, "repl_key_C_f",  "", HRepl::handle_key_C_f );
	el_set( _el, EL_ADDFN, "repl_key_C_g",  "", HRepl::handle_key_C_g );
	el_set( _el, EL_ADDFN, "repl_key_C_h",  "", HRepl::handle_key_C_h );
	el_set( _el, EL_ADDFN, "repl_key_C_i",  "", HRepl::handle_key_C_i );
	el_set( _el, EL_ADDFN, "repl_key_C_j",  "", HRepl::handle_key_C_j );
	el_set( _el, EL_ADDFN, "repl_key_C_k",  "", HRepl::handle_key_C_k );
	el_set( _el, EL_ADDFN, "repl_key_C_l",  "", HRepl::handle_key_C_l );
	el_set( _el, EL_ADDFN, "repl_key_C_m",  "", HRepl::handle_key_C_m );
	el_set( _el, EL_ADDFN, "repl_key_C_n",  "", HRepl::handle_key_C_n );
	el_set( _el, EL_ADDFN, "repl_key_C_o",  "", HRepl::handle_key_C_o );
	el_set( _el, EL_ADDFN, "repl_key_C_p",  "", HRepl::handle_key_C_p );
	el_set( _el, EL_ADDFN, "repl_key_C_q",  "", HRepl::handle_key_C_q );
	el_set( _el, EL_ADDFN, "repl_key_C_r",  "", HRepl::handle_key_C_r );
	el_set( _el, EL_ADDFN, "repl_key_C_s",  "", HRepl::handle_key_C_s );
	el_set( _el, EL_ADDFN, "repl_key_C_t",  "", HRepl::handle_key_C_t );
	el_set( _el, EL_ADDFN, "repl_key_C_u",  "", HRepl::handle_key_C_u );
	el_set( _el, EL_ADDFN, "repl_key_C_v",  "", HRepl::handle_key_C_v );
	el_set( _el, EL_ADDFN, "repl_key_C_w",  "", HRepl::handle_key_C_w );
	el_set( _el, EL_ADDFN, "repl_key_C_x",  "", HRepl::handle_key_C_x );
	el_set( _el, EL_ADDFN, "repl_key_C_y",  "", HRepl::handle_key_C_y );
	el_set( _el, EL_ADDFN, "repl_key_C_z",  "", HRepl::handle_key_C_z );
	el_set( _el, EL_ADDFN, "repl_key_C_0",  "", HRepl::handle_key_C_0 );
	el_set( _el, EL_ADDFN, "repl_key_C_1",  "", HRepl::handle_key_C_1 );
	el_set( _el, EL_ADDFN, "repl_key_C_2",  "", HRepl::handle_key_C_2 );
	el_set( _el, EL_ADDFN, "repl_key_C_3",  "", HRepl::handle_key_C_3 );
	el_set( _el, EL_ADDFN, "repl_key_C_4",  "", HRepl::handle_key_C_4 );
	el_set( _el, EL_ADDFN, "repl_key_C_5",  "", HRepl::handle_key_C_5 );
	el_set( _el, EL_ADDFN, "repl_key_C_6",  "", HRepl::handle_key_C_6 );
	el_set( _el, EL_ADDFN, "repl_key_C_7",  "", HRepl::handle_key_C_7 );
	el_set( _el, EL_ADDFN, "repl_key_C_8",  "", HRepl::handle_key_C_8 );
	el_set( _el, EL_ADDFN, "repl_key_C_9",  "", HRepl::handle_key_C_9 );
	el_set( _el, EL_ADDFN, "repl_key_M_a",  "", HRepl::handle_key_M_a );
	el_set( _el, EL_ADDFN, "repl_key_M_b",  "", HRepl::handle_key_M_b );
	el_set( _el, EL_ADDFN, "repl_key_M_c",  "", HRepl::handle_key_M_c );
	el_set( _el, EL_ADDFN, "repl_key_M_d",  "", HRepl::handle_key_M_d );
	el_set( _el, EL_ADDFN, "repl_key_M_e",  "", HRepl::handle_key_M_e );
	el_set( _el, EL_ADDFN, "repl_key_M_f",  "", HRepl::handle_key_M_f );
	el_set( _el, EL_ADDFN, "repl_key_M_g",  "", HRepl::handle_key_M_g );
	el_set( _el, EL_ADDFN, "repl_key_M_h",  "", HRepl::handle_key_M_h );
	el_set( _el, EL_ADDFN, "repl_key_M_i",  "", HRepl::handle_key_M_i );
	el_set( _el, EL_ADDFN, "repl_key_M_j",  "", HRepl::handle_key_M_j );
	el_set( _el, EL_ADDFN, "repl_key_M_k",  "", HRepl::handle_key_M_k );
	el_set( _el, EL_ADDFN, "repl_key_M_l",  "", HRepl::handle_key_M_l );
	el_set( _el, EL_ADDFN, "repl_key_M_m",  "", HRepl::handle_key_M_m );
	el_set( _el, EL_ADDFN, "repl_key_M_n",  "", HRepl::handle_key_M_n );
	el_set( _el, EL_ADDFN, "repl_key_M_o",  "", HRepl::handle_key_M_o );
	el_set( _el, EL_ADDFN, "repl_key_M_p",  "", HRepl::handle_key_M_p );
	el_set( _el, EL_ADDFN, "repl_key_M_q",  "", HRepl::handle_key_M_q );
	el_set( _el, EL_ADDFN, "repl_key_M_r",  "", HRepl::handle_key_M_r );
	el_set( _el, EL_ADDFN, "repl_key_M_s",  "", HRepl::handle_key_M_s );
	el_set( _el, EL_ADDFN, "repl_key_M_t",  "", HRepl::handle_key_M_t );
	el_set( _el, EL_ADDFN, "repl_key_M_u",  "", HRepl::handle_key_M_u );
	el_set( _el, EL_ADDFN, "repl_key_M_v",  "", HRepl::handle_key_M_v );
	el_set( _el, EL_ADDFN, "repl_key_M_w",  "", HRepl::handle_key_M_w );
	el_set( _el, EL_ADDFN, "repl_key_M_x",  "", HRepl::handle_key_M_x );
	el_set( _el, EL_ADDFN, "repl_key_M_y",  "", HRepl::handle_key_M_y );
	el_set( _el, EL_ADDFN, "repl_key_M_z",  "", HRepl::handle_key_M_z );
	el_set( _el, EL_ADDFN, "repl_key_M_0",  "", HRepl::handle_key_M_0 );
	el_set( _el, EL_ADDFN, "repl_key_M_1",  "", HRepl::handle_key_M_1 );
	el_set( _el, EL_ADDFN, "repl_key_M_2",  "", HRepl::handle_key_M_2 );
	el_set( _el, EL_ADDFN, "repl_key_M_3",  "", HRepl::handle_key_M_3 );
	el_set( _el, EL_ADDFN, "repl_key_M_4",  "", HRepl::handle_key_M_4 );
	el_set( _el, EL_ADDFN, "repl_key_M_5",  "", HRepl::handle_key_M_5 );
	el_set( _el, EL_ADDFN, "repl_key_M_6",  "", HRepl::handle_key_M_6 );
	el_set( _el, EL_ADDFN, "repl_key_M_7",  "", HRepl::handle_key_M_7 );
	el_set( _el, EL_ADDFN, "repl_key_M_8",  "", HRepl::handle_key_M_8 );
	el_set( _el, EL_ADDFN, "repl_key_M_9",  "", HRepl::handle_key_M_9 );
	el_set( _el, EL_ADDFN, "repl_key_F1",   "", HRepl::handle_key_F1 );
	el_set( _el, EL_ADDFN, "repl_key_F2",   "", HRepl::handle_key_F2 );
	el_set( _el, EL_ADDFN, "repl_key_F3",   "", HRepl::handle_key_F3 );
	el_set( _el, EL_ADDFN, "repl_key_F4",   "", HRepl::handle_key_F4 );
	el_set( _el, EL_ADDFN, "repl_key_F5",   "", HRepl::handle_key_F5 );
	el_set( _el, EL_ADDFN, "repl_key_F6",   "", HRepl::handle_key_F6 );
	el_set( _el, EL_ADDFN, "repl_key_F7",   "", HRepl::handle_key_F7 );
	el_set( _el, EL_ADDFN, "repl_key_F8",   "", HRepl::handle_key_F8 );
	el_set( _el, EL_ADDFN, "repl_key_F9",   "", HRepl::handle_key_F9 );
	el_set( _el, EL_ADDFN, "repl_key_F10",  "", HRepl::handle_key_F10 );
	el_set( _el, EL_ADDFN, "repl_key_F11",  "", HRepl::handle_key_F11 );
	el_set( _el, EL_ADDFN, "repl_key_F12",  "", HRepl::handle_key_F12 );
	el_set( _el, EL_ADDFN, "repl_key_SF1",  "", HRepl::handle_key_SF1 );
	el_set( _el, EL_ADDFN, "repl_key_SF2",  "", HRepl::handle_key_SF2 );
	el_set( _el, EL_ADDFN, "repl_key_SF3",  "", HRepl::handle_key_SF3 );
	el_set( _el, EL_ADDFN, "repl_key_SF4",  "", HRepl::handle_key_SF4 );
	el_set( _el, EL_ADDFN, "repl_key_SF5",  "", HRepl::handle_key_SF5 );
	el_set( _el, EL_ADDFN, "repl_key_SF6",  "", HRepl::handle_key_SF6 );
	el_set( _el, EL_ADDFN, "repl_key_SF7",  "", HRepl::handle_key_SF7 );
	el_set( _el, EL_ADDFN, "repl_key_SF8",  "", HRepl::handle_key_SF8 );
	el_set( _el, EL_ADDFN, "repl_key_SF9",  "", HRepl::handle_key_SF9 );
	el_set( _el, EL_ADDFN, "repl_key_SF10", "", HRepl::handle_key_SF10 );
	el_set( _el, EL_ADDFN, "repl_key_SF11", "", HRepl::handle_key_SF11 );
	el_set( _el, EL_ADDFN, "repl_key_SF12", "", HRepl::handle_key_SF12 );
	el_set( _el, EL_ADDFN, "repl_key_CF1",  "", HRepl::handle_key_CF1 );
	el_set( _el, EL_ADDFN, "repl_key_CF2",  "", HRepl::handle_key_CF2 );
	el_set( _el, EL_ADDFN, "repl_key_CF3",  "", HRepl::handle_key_CF3 );
	el_set( _el, EL_ADDFN, "repl_key_CF4",  "", HRepl::handle_key_CF4 );
	el_set( _el, EL_ADDFN, "repl_key_CF5",  "", HRepl::handle_key_CF5 );
	el_set( _el, EL_ADDFN, "repl_key_CF6",  "", HRepl::handle_key_CF6 );
	el_set( _el, EL_ADDFN, "repl_key_CF7",  "", HRepl::handle_key_CF7 );
	el_set( _el, EL_ADDFN, "repl_key_CF8",  "", HRepl::handle_key_CF8 );
	el_set( _el, EL_ADDFN, "repl_key_CF9",  "", HRepl::handle_key_CF9 );
	el_set( _el, EL_ADDFN, "repl_key_CF10", "", HRepl::handle_key_CF10 );
	el_set( _el, EL_ADDFN, "repl_key_CF11", "", HRepl::handle_key_CF11 );
	el_set( _el, EL_ADDFN, "repl_key_CF12", "", HRepl::handle_key_CF12 );
	::history( _hist, &_histEvent, H_SETSIZE, 1000 );
	::history( _hist, &_histEvent, H_SETUNIQUE, 1 );
#else
	_repl_ = this;
	rl_readline_name = PACKAGE_NAME;
	history_write_timestamps = 1;
#endif
#ifndef USE_REPLXX
	REPL_bind_key( "\001",       "C-a",   HRepl::handle_key_C_a,  "repl_key_C_a" );
	REPL_bind_key( "\002",       "C-b",   HRepl::handle_key_C_b,  "repl_key_C_b" );
	REPL_bind_key( "\003",       "C-c",   HRepl::handle_key_C_c,  "repl_key_C_c" );
	REPL_bind_key( "\004",       "C-d",   HRepl::handle_key_C_d,  "repl_key_C_d" );
	REPL_bind_key( "\005",       "C-e",   HRepl::handle_key_C_e,  "repl_key_C_e" );
	REPL_bind_key( "\006",       "C-f",   HRepl::handle_key_C_f,  "repl_key_C_f" );
	REPL_bind_key( "\007",       "C-g",   HRepl::handle_key_C_g,  "repl_key_C_g" );
	REPL_bind_key( "\010",       "C-h",   HRepl::handle_key_C_h,  "repl_key_C_h" );
	REPL_bind_key( "\011",       "C-i",   HRepl::handle_key_C_i,  "repl_key_C_i" );
	REPL_bind_key( "\012",       "C-j",   HRepl::handle_key_C_j,  "repl_key_C_j" );
	REPL_bind_key( "\013",       "C-k",   HRepl::handle_key_C_k,  "repl_key_C_k" );
	REPL_bind_key( "\014",       "C-l",   HRepl::handle_key_C_l,  "repl_key_C_l" );
	REPL_bind_key( "\015",       "C-m",   HRepl::handle_key_C_m,  "repl_key_C_m" );
	REPL_bind_key( "\016",       "C-n",   HRepl::handle_key_C_n,  "repl_key_C_n" );
	REPL_bind_key( "\017",       "C-o",   HRepl::handle_key_C_o,  "repl_key_C_o" );
	REPL_bind_key( "\020",       "C-p",   HRepl::handle_key_C_p,  "repl_key_C_p" );
	REPL_bind_key( "\021",       "C-q",   HRepl::handle_key_C_q,  "repl_key_C_q" );
	REPL_bind_key( "\022",       "C-r",   HRepl::handle_key_C_r,  "repl_key_C_r" );
	REPL_bind_key( "\023",       "C-s",   HRepl::handle_key_C_s,  "repl_key_C_s" );
	REPL_bind_key( "\024",       "C-t",   HRepl::handle_key_C_t,  "repl_key_C_t" );
	REPL_bind_key( "\025",       "C-u",   HRepl::handle_key_C_u,  "repl_key_C_u" );
	REPL_bind_key( "\026",       "C-v",   HRepl::handle_key_C_v,  "repl_key_C_v" );
	REPL_bind_key( "\027",       "C-w",   HRepl::handle_key_C_w,  "repl_key_C_w" );
	REPL_bind_key( "\030",       "C-x",   HRepl::handle_key_C_x,  "repl_key_C_x" );
	REPL_bind_key( "\031",       "C-y",   HRepl::handle_key_C_y,  "repl_key_C_y" );
	REPL_bind_key( "\032",       "C-z",   HRepl::handle_key_C_z,  "repl_key_C_z" );
/*	REPL_bind_key( "\033",       "C-0",   HRepl::handle_key_C_0,  "repl_key_C_0" ); */
	REPL_bind_key( "\034",       "C-1",   HRepl::handle_key_C_1,  "repl_key_C_1" );
	REPL_bind_key( "\035",       "C-2",   HRepl::handle_key_C_2,  "repl_key_C_2" );
	REPL_bind_key( "\036",       "C-3",   HRepl::handle_key_C_3,  "repl_key_C_3" );
	REPL_bind_key( "\037",       "C-4",   HRepl::handle_key_C_4,  "repl_key_C_4" );
	REPL_bind_key( "\040",       "C-5",   HRepl::handle_key_C_5,  "repl_key_C_5" );
	REPL_bind_key( "\041",       "C-6",   HRepl::handle_key_C_6,  "repl_key_C_6" );
	REPL_bind_key( "\042",       "C-7",   HRepl::handle_key_C_7,  "repl_key_C_7" );
	REPL_bind_key( "\043",       "C-8",   HRepl::handle_key_C_8,  "repl_key_C_8" );
	REPL_bind_key( "\044",       "C-9",   HRepl::handle_key_C_9,  "repl_key_C_9" );
	REPL_bind_key( "\033a",      "M-a",   HRepl::handle_key_M_a,  "repl_key_M_a" );
	REPL_bind_key( "\033b",      "M-b",   HRepl::handle_key_M_b,  "repl_key_M_b" );
	REPL_bind_key( "\033c",      "M-c",   HRepl::handle_key_M_c,  "repl_key_M_c" );
	REPL_bind_key( "\033d",      "M-d",   HRepl::handle_key_M_d,  "repl_key_M_d" );
	REPL_bind_key( "\033e",      "M-e",   HRepl::handle_key_M_e,  "repl_key_M_e" );
	REPL_bind_key( "\033f",      "M-f",   HRepl::handle_key_M_f,  "repl_key_M_f" );
	REPL_bind_key( "\033g",      "M-g",   HRepl::handle_key_M_g,  "repl_key_M_g" );
	REPL_bind_key( "\033h",      "M-h",   HRepl::handle_key_M_h,  "repl_key_M_h" );
	REPL_bind_key( "\033i",      "M-i",   HRepl::handle_key_M_i,  "repl_key_M_i" );
	REPL_bind_key( "\033j",      "M-j",   HRepl::handle_key_M_j,  "repl_key_M_j" );
	REPL_bind_key( "\033k",      "M-k",   HRepl::handle_key_M_k,  "repl_key_M_k" );
	REPL_bind_key( "\033l",      "M-l",   HRepl::handle_key_M_l,  "repl_key_M_l" );
	REPL_bind_key( "\033m",      "M-m",   HRepl::handle_key_M_m,  "repl_key_M_m" );
	REPL_bind_key( "\033n",      "M-n",   HRepl::handle_key_M_n,  "repl_key_M_n" );
	REPL_bind_key( "\033o",      "M-o",   HRepl::handle_key_M_o,  "repl_key_M_o" );
	REPL_bind_key( "\033p",      "M-p",   HRepl::handle_key_M_p,  "repl_key_M_p" );
	REPL_bind_key( "\033q",      "M-q",   HRepl::handle_key_M_q,  "repl_key_M_q" );
	REPL_bind_key( "\033r",      "M-r",   HRepl::handle_key_M_r,  "repl_key_M_r" );
	REPL_bind_key( "\033s",      "M-s",   HRepl::handle_key_M_s,  "repl_key_M_s" );
	REPL_bind_key( "\033t",      "M-t",   HRepl::handle_key_M_t,  "repl_key_M_t" );
	REPL_bind_key( "\033u",      "M-u",   HRepl::handle_key_M_u,  "repl_key_M_u" );
	REPL_bind_key( "\033v",      "M-v",   HRepl::handle_key_M_v,  "repl_key_M_v" );
	REPL_bind_key( "\033w",      "M-w",   HRepl::handle_key_M_w,  "repl_key_M_w" );
	REPL_bind_key( "\033x",      "M-x",   HRepl::handle_key_M_x,  "repl_key_M_x" );
	REPL_bind_key( "\033y",      "M-y",   HRepl::handle_key_M_y,  "repl_key_M_y" );
	REPL_bind_key( "\033z",      "M-z",   HRepl::handle_key_M_z,  "repl_key_M_z" );
	REPL_bind_key( "\0330",      "M-0",   HRepl::handle_key_M_0,  "repl_key_M_0" );
	REPL_bind_key( "\0331",      "M-1",   HRepl::handle_key_M_1,  "repl_key_M_1" );
	REPL_bind_key( "\0332",      "M-2",   HRepl::handle_key_M_2,  "repl_key_M_2" );
	REPL_bind_key( "\0333",      "M-3",   HRepl::handle_key_M_3,  "repl_key_M_3" );
	REPL_bind_key( "\0334",      "M-4",   HRepl::handle_key_M_4,  "repl_key_M_4" );
	REPL_bind_key( "\0335",      "M-5",   HRepl::handle_key_M_5,  "repl_key_M_5" );
	REPL_bind_key( "\0336",      "M-6",   HRepl::handle_key_M_6,  "repl_key_M_6" );
	REPL_bind_key( "\0337",      "M-7",   HRepl::handle_key_M_7,  "repl_key_M_7" );
	REPL_bind_key( "\0338",      "M-8",   HRepl::handle_key_M_8,  "repl_key_M_8" );
	REPL_bind_key( "\0339",      "M-9",   HRepl::handle_key_M_9,  "repl_key_M_9" );
	REPL_bind_key( "\033OP",     "F1",    HRepl::handle_key_F1,   "repl_key_F1" );
	REPL_bind_key( "\033OQ",     "F2",    HRepl::handle_key_F2,   "repl_key_F2" );
	REPL_bind_key( "\033OR",     "F3",    HRepl::handle_key_F3,   "repl_key_F3" );
	REPL_bind_key( "\033OS",     "F4",    HRepl::handle_key_F4,   "repl_key_F4" );
	REPL_bind_key( "\033[15~",   "F5",    HRepl::handle_key_F5,   "repl_key_F5" );
	REPL_bind_key( "\033[17~",   "F6",    HRepl::handle_key_F6,   "repl_key_F6" );
	REPL_bind_key( "\033[18~",   "F7",    HRepl::handle_key_F7,   "repl_key_F7" );
	REPL_bind_key( "\033[19~",   "F8",    HRepl::handle_key_F8,   "repl_key_F8" );
	REPL_bind_key( "\033[20~",   "F9",    HRepl::handle_key_F9,   "repl_key_F9" );
	REPL_bind_key( "\033[21~",   "Fi0",   HRepl::handle_key_F10,  "repl_key_F10" );
	REPL_bind_key( "\033[23~",   "F11",   HRepl::handle_key_F11,  "repl_key_F11" );
	REPL_bind_key( "\033[24~",   "F12",   HRepl::handle_key_F12,  "repl_key_F12" );
	REPL_bind_key( "\033[1;2P",  "S-F1",  HRepl::handle_key_SF1,  "repl_key_SF1" );
	REPL_bind_key( "\033[1;2Q",  "S-F2",  HRepl::handle_key_SF2,  "repl_key_SF2" );
	REPL_bind_key( "\033[1;2R",  "S-F3",  HRepl::handle_key_SF3,  "repl_key_SF3" );
	REPL_bind_key( "\033[1;2S",  "S-F4",  HRepl::handle_key_SF4,  "repl_key_SF4" );
	REPL_bind_key( "\033[15;2~", "S-F5",  HRepl::handle_key_SF5,  "repl_key_SF5" );
	REPL_bind_key( "\033[17;2~", "S-F6",  HRepl::handle_key_SF6,  "repl_key_SF6" );
	REPL_bind_key( "\033[18;2~", "S-F7",  HRepl::handle_key_SF7,  "repl_key_SF7" );
	REPL_bind_key( "\033[19;2~", "S-F8",  HRepl::handle_key_SF8,  "repl_key_SF8" );
	REPL_bind_key( "\033[20;2~", "S-F9",  HRepl::handle_key_SF9,  "repl_key_SF9" );
	REPL_bind_key( "\033[21;2~", "S-F10", HRepl::handle_key_SF10, "repl_key_SF10" );
	REPL_bind_key( "\033[23;2~", "S-F11", HRepl::handle_key_SF11, "repl_key_SF11" );
	REPL_bind_key( "\033[24;2~", "S-F12", HRepl::handle_key_SF12, "repl_key_SF12" );
	REPL_bind_key( "\033[1;5P",  "C-F1",  HRepl::handle_key_CF1,  "repl_key_CF1" );
	REPL_bind_key( "\033[1;5Q",  "C-F2",  HRepl::handle_key_CF2,  "repl_key_CF2" );
	REPL_bind_key( "\033[1;5R",  "C-F3",  HRepl::handle_key_CF3,  "repl_key_CF3" );
	REPL_bind_key( "\033[1;5S",  "C-F4",  HRepl::handle_key_CF4,  "repl_key_CF4" );
	REPL_bind_key( "\033[15;5~", "C-F5",  HRepl::handle_key_CF5,  "repl_key_CF5" );
	REPL_bind_key( "\033[17;5~", "C-F6",  HRepl::handle_key_CF6,  "repl_key_CF6" );
	REPL_bind_key( "\033[18;5~", "C-F7",  HRepl::handle_key_CF7,  "repl_key_CF7" );
	REPL_bind_key( "\033[19;5~", "C-F8",  HRepl::handle_key_CF8,  "repl_key_CF8" );
	REPL_bind_key( "\033[20;5~", "C-F9",  HRepl::handle_key_CF9,  "repl_key_CF9" );
	REPL_bind_key( "\033[21;5~", "C-F10", HRepl::handle_key_CF10, "repl_key_CF10" );
	REPL_bind_key( "\033[23;5~", "C-F11", HRepl::handle_key_CF11, "repl_key_CF11" );
	REPL_bind_key( "\033[24;5~", "C-F12", HRepl::handle_key_CF12, "repl_key_CF12" );
#endif
}


HRepl::~HRepl( void ) {
	save_history();
#ifdef USE_EDITLINE
	history_end( _hist );
	el_end( _el );
#endif
}

void HRepl::save_history( void ) {
	if ( _historyPath.is_empty() ) {
		return;
	}
	HUTF8String utf8( _historyPath );
#ifdef USE_REPLXX
	_replxx.history_save( utf8.c_str() );
#else
	lines_t linesCur;
	lines_t linesOrig;
	lines_t lines;
#ifdef USE_EDITLINE
	::history( _hist, &_histEvent, H_GETSIZE );
	int size( _histEvent.num );
	for ( int i( 0 ); i <= size; ++ i ) {
		::history( _hist, &_histEvent, H_SET, i );
		if ( ::history( _hist, &_histEvent, H_CURR ) != 0 ) {
			continue;
		}
		linesCur.emplace_back( _histEvent.str );
	}
	::history( _hist, &_histEvent, H_CLEAR );
	::history( _hist, &_histEvent, H_LOAD, utf8.c_str() );
	::history( _hist, &_histEvent, H_GETSIZE );
	size = _histEvent.num;
	for ( int i( 0 ); i <= size; ++ i ) {
		::history( _hist, &_histEvent, H_SET, i );
		if ( ::history( _hist, &_histEvent, H_CURR ) != 0 ) {
			continue;
		}
		if ( ! ( _histEvent.str && _histEvent.str[0] ) ) {
			continue;
		}
		linesOrig.emplace_back( _histEvent.str );
	}
	::history( _hist, &_histEvent, H_CLEAR );
#else
	for ( int i( 0 ); i < history_length; ++ i ) {
		HIST_ENTRY* he( history_get( history_base + i ) );
		if ( ! he ) {
			continue;
		}
		linesCur.emplace_back( he->line );
	}
	::clear_history();
	::read_history( utf8.c_str() );
	for ( int i( 0 ); i < history_length; ++ i ) {
		HIST_ENTRY* he( history_get( history_base + i ) );
		if ( ! he ) {
			continue;
		}
		linesOrig.emplace_back( he->line );
	}
	::clear_history();
#endif
	for ( HString const& l : linesOrig ) {
		lines_t::iterator it( find( lines.begin(), lines.end(), l ) );
		if ( it != lines.end() ) {
			lines.erase( it );
		}
		lines.push_back( l );
	}
	for ( HString const& l : linesCur ) {
		lines_t::iterator it( find( lines.begin(), lines.end(), l ) );
		if ( it != lines.end() ) {
			lines.erase( it );
		}
		lines.push_back( l );
	}
	HUTF8String utf8line;
	for ( HString const& l : lines ) {
		utf8line.assign( l );
		REPL_add_history( utf8line.c_str() );
	}
#ifdef USE_EDITLINE
	::history( _hist, &_histEvent, H_SAVE, utf8.c_str() );
#else
	write_history( utf8.c_str() );
#endif
#endif
}

void HRepl::clear_history( void ) {
#ifdef USE_REPLXX
	_replxx.history_clear();
#elif defined( USE_EDITLINE )
	::history( _hist, &_histEvent, H_CLEAR );
#else
	::clear_history();
#endif
}

void HRepl::set_max_history_size( int maxHistorySize_ ) {
#ifdef USE_REPLXX
	_replxx.set_max_history_size( maxHistorySize_ );
#elif defined( USE_EDITLINE )
	::history( _hist, &_histEvent, H_SETSIZE, maxHistorySize_ );
#else
	::stifle_history( maxHistorySize_ );
#endif
}

int HRepl::history_size( void ) const {
	int historySize( 0 );
#ifdef USE_REPLXX
	historySize = _replxx.history_size();
#elif defined( USE_EDITLINE )
	HistEvent histEvent;
	::history( _hist, &histEvent, H_GETSIZE );
	historySize = histEvent.num;
#else
	historySize = history_length;
#endif
	return ( historySize );
}

HRepl::lines_t HRepl::history( void ) const {
	lines_t lines;
#ifdef USE_REPLXX
	replxx::Replxx::HistoryScan hs( _replxx.history_scan() );
	while ( hs.next() ) {
		lines.emplace_back( hs.get().text().c_str() );
	}
#elif defined( USE_EDITLINE )
	HistEvent histEvent;
	for ( int i( 0 ), size( history_size() ); i < size; ++ i ) {
		::history( _hist, &histEvent, H_SET, i );
		if ( ::history( _hist, &histEvent, H_CURR ) != 0 ) {
			continue;
		}
		if ( ! ( histEvent.str && histEvent.str[0] ) ) {
			continue;
		}
		lines.emplace_back( histEvent.str );
	}
	::history( _hist, &histEvent, H_SET, 0 );
#else
	for ( int i( 0 ), size( history_size() ); i < size; ++ i ) {
		HIST_ENTRY* he( history_get( history_base + i ) );
		if ( ! he ) {
			continue;
		}
		lines.emplace_back( he->line );
	}
#endif
	return ( lines );
}

void HRepl::set_shell( HShell* shell_ ) {
	_shell = shell_;
#ifdef USE_REPLXX
	if ( dynamic_cast<HSystemShell*>( shell_ ) ) {
		_replxx.bind_key( Replxx::KEY::control( 'Z' ), std::bind( &do_nothing, std::placeholders::_1 ) );
	}
#endif
}

void HRepl::set_line_runner( HLineRunner* lineRunner_ ) {
	_lineRunner = lineRunner_;
}

void HRepl::enable_bracketed_paste( void ) {
#ifdef USE_REPLXX
	_replxx.enable_bracketed_paste();
#endif
}

void HRepl::disable_bracketed_paste( void ) {
#ifdef USE_REPLXX
	_replxx.disable_bracketed_paste();
#endif
}

void HRepl::set_completer( completion_words_t completer_ ) {
#ifdef USE_REPLXX
	_replxx.set_completion_callback( std::bind( &replxx_completion_words, std::placeholders::_1, std::placeholders::_2, this ) );
	if ( ! setup._noColor ) {
		_replxx.set_highlighter_callback( std::bind( &HRepl::colorize, this, std::placeholders::_1, std::placeholders::_2 ) );
		_replxx.set_hint_callback( std::bind( &HRepl::find_hints, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3 ) );
	}
#elif defined( USE_EDITLINE )
	el_set( _el, EL_ADDFN, "complete", "Command completion", complete );
	el_set( _el, EL_BIND, "^I", "complete", nullptr );
#else
	rl_completion_entry_function = rl_completion_words;
	rl_completion_word_break_hook = rl_word_break_characters;
	rl_special_prefixes = SPECIAL_PREFIXES_RAW;
	rl_basic_word_break_characters = const_cast<char*>( BREAK_CHARACTERS_HUGINN_CLASS.data() );
	rl_completer_word_break_characters = const_cast<char*>( BREAK_CHARACTERS_HUGINN_CLASS.data() );
#endif
	_completer = completer_;
}

void HRepl::set_history_path( yaal::hcore::HString const& historyPath_ ) {
	_historyPath = historyPath_;
}

void HRepl::load_history( void ) {
	if ( ! _historyPath.is_empty() ) {
		REPL_load_history( HUTF8String( _historyPath ).c_str() );
	}
}

HRepl::completions_t HRepl::completion_words( yaal::hcore::HString&& context_, yaal::hcore::HString&& prefix_, int& contextLen_, CONTEXT_TYPE& contextType_, bool shell_ ) {
	HScopedValueReplacement<HShell*> svr( _shell, shell_ ? _shell : nullptr );
	return ( _completer( _inputSoFar + context_, yaal::move( prefix_ ), contextLen_, contextType_, this ) );
}

bool HRepl::input_impl( yaal::hcore::HString& line_, char const* prompt_ ) {
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

bool HRepl::input( yaal::hcore::HString& line_, char const* prompt_ ) {
	M_PROLOG
	bool gotLine( false );
	HString line;
	_inputSoFar.clear();
	while ( true ) {
		line.clear();
		gotLine = input_impl( line, _inputSoFar.is_empty() ? prompt_ : "> " );
		if ( ! gotLine ) {
			break;
		}
		if ( line.is_empty() ) {
			break;
		}
		_inputSoFar.append( line );
		if ( line.back() != '\\'_ycp ) {
			break;
		}
		_inputSoFar.pop_back();
		_inputSoFar.push_back( ' '_ycp );
	}
	line_.assign( _inputSoFar );
	return ( gotLine );
	M_EPILOG
}

void HRepl::print( char const* str_ ) {
	REPL_print( "%s", str_ );
}

#ifdef USE_REPLXX
void HRepl::colorize( std::string const& line_, Replxx::colors_t& colors_ ) const {
	M_PROLOG
	HString line( line_.c_str() );
	colors_t colors;
	::huginn::colorize( line, colors, shell() && shell()->is_valid_command( line ) ? shell() : nullptr );
	int size( static_cast<int>( colors_.size() ) );
	for ( int i( 0 ); i < size; ++ i ) {
		colors_[static_cast<size_t>( i )] = static_cast<Replxx::Color>( colors[i] );
	}
	return;
	M_EPILOG
}

Replxx::hints_t HRepl::find_hints( std::string const& prefix_, int& contextLen_, Replxx::Color& color_ ) {
	M_PROLOG
	HSystemShell* systemShell( dynamic_cast<HSystemShell*>( _shell ) );
	if ( systemShell && systemShell->has_huginn_jobs() ) {
		return ( Replxx::hints_t() );
	}
	HString context( _inputSoFar );
	context.append( prefix_.c_str() );
	contextLen_ = context_length( context, CONTEXT_TYPE::HUGINN );
	HString prefix( prefix_.c_str() );
	prefix.shift_left( context.get_length() - contextLen_ );
	if ( ( prefix.is_empty() && ( prefix_ != "//" ) ) || ( prefix == "." ) ) {
		return ( Replxx::hints_t() );
	}
	bool inDocContext( context.find( "//doc " ) == 0 );
	CONTEXT_TYPE contextType( CONTEXT_TYPE::HUGINN );
	HRepl::completions_t hints( completion_words( prefix_.c_str(), HString( prefix ), contextLen_, contextType, false ) );
	HUTF8String utf8;
	HString doc;
	Replxx::hints_t replxxHints;
	HString h;
	for ( HRepl::HCompletion const& c : hints ) {
		h.assign( c.text() );
		doc.clear();
		h.trim_right( "()" );
		HString ask( h );
		int long dotIdx( ask.find_last( '.'_ycp ) );
		int long toStrip( 0 );
		if ( dotIdx != HString::npos ) {
			HString obj( line_runner()->symbol_type_name( ask.left( dotIdx ) ) );
			HString member( ask.mid( dotIdx + 1 ) );
			ask.assign( obj ).append( '.' ).append( member );
			if ( line_runner()->symbol_kind( ask ) != HDescription::SYMBOL_KIND::CLASS ) {
				toStrip = member.get_length();
			} else {
				doc.assign( " - " );
			}
		} else if ( line_runner()->symbol_kind( ask ) != HDescription::SYMBOL_KIND::CLASS ) {
			toStrip = h.get_length();
		} else {
			doc.assign( " - " );
		}
		doc.append( line_runner()->doc( ask, inDocContext ) );
		doc.replace( "*", "" );
		doc.shift_left( toStrip );
		utf8.assign( h.append( doc ) );
		replxxHints.emplace_back( utf8.c_str() );
	}
	color_ = _replxxColors_.at( color( GROUP::HINT ) );
	return ( replxxHints );
	M_EPILOG
}

void HRepl::set_hint_delay( int ms ) {
	_replxx.set_hint_delay( ms );
}
#else
void HRepl::set_hint_delay( int ) {
}
#endif

bool HRepl::bind_key( yaal::hcore::HString const& key_, action_t const& action_ ) {
	key_table_t::iterator it( _keyTable.find( key_ ) );
	bool validKey( it != _keyTable.end() );
	if ( validKey ) {
#ifdef USE_REPLXX
		_replxx.bind_key( it->second, call( &HRepl::run_action, this, action_, _1 ) );
#else
		it->second = action_;
		OKeyBindDispatchInfo const& keyBindDispatchInfo( _keyBindingDispatchInfo.at( key_ ) );
#	ifdef USE_EDITLINE
		el_set( _el, EL_BIND, keyBindDispatchInfo._seq, keyBindDispatchInfo._name, nullptr );
#	else
		rl_bind_keyseq( keyBindDispatchInfo._seq, keyBindDispatchInfo._func );
#	endif
#endif
	}
	return ( validKey );
}

HRepl::HModel HRepl::get_model( void ) const {
	HString line;
	int position( 0 );
#ifdef USE_REPLXX
	replxx::Replxx::State state( _replxx.get_state() );
	line.assign( state.text() );
	position = state.cursor_position();
#elif defined ( USE_EDITLINE )
	LineInfo const* li( el_line( _el ) );
	line.assign( li->buffer, li->lastchar - li->buffer );
	position = static_cast<int>( HString( li->buffer, li->cursor - li->buffer ).get_length() );
#else
	line.assign( rl_line_buffer );
	position = rl_point;
#endif
	return ( HModel( line, position ) );
}

void HRepl::set_model( HModel const& model_ ) {
	HUTF8String utf8( model_.line() );
#ifdef USE_REPLXX
	_replxx.set_state( replxx::Replxx::State( utf8.c_str(), model_.position() ) );
#elif defined ( USE_EDITLINE )
	LineInfo const* li( el_line( _el ) );
	el_cursor( _el, static_cast<int>( li->lastchar - li->cursor ) );
	el_deletestr( _el, static_cast<int>( li->lastchar - li->buffer ) );
	el_insertstr( _el, utf8.c_str() );
	el_cursor( _el, model_.position() );
#else
	rl_point = 0;
	rl_delete_text( 0, rl_end );
	rl_insert_text( utf8.c_str() );
	rl_point = model_.position();
	rl_redisplay();
#endif
	return;
}

namespace {
char const HUGINN_REPL_LINE_ENV_NAME[] = "HUGINN_REPL_LINE";
char const HUGINN_REPL_POSITION_ENV_NAME[] = "HUGINN_REPL_POSITION";
}

void HRepl::model_to_env( void ) {
	HModel model( get_model() );
	set_env( HUGINN_REPL_LINE_ENV_NAME, model.line() );
	set_env( HUGINN_REPL_POSITION_ENV_NAME, model.position() );
}

void HRepl::env_to_model( void ) {
	char const* HUGINN_REPL_LINE( ::getenv( HUGINN_REPL_LINE_ENV_NAME ) );
	char const* HUGINN_REPL_POSITION( ::getenv( HUGINN_REPL_POSITION_ENV_NAME ) );
	if ( HUGINN_REPL_LINE && HUGINN_REPL_POSITION ) {
		set_model( HModel( HUGINN_REPL_LINE, lexical_cast<int>( HUGINN_REPL_POSITION ) ) );
	}
	unset_env( HUGINN_REPL_LINE_ENV_NAME );
	unset_env( HUGINN_REPL_POSITION_ENV_NAME );
}

#ifdef USE_REPLXX
replxx::Replxx::ACTION_RESULT HRepl::run_action( action_t action_, char32_t ) {
	_replxx.invoke( Replxx::ACTION::CLEAR_SELF, 0 );
	model_to_env();
	action_();
	env_to_model();
	_replxx.invoke( Replxx::ACTION::REPAINT, 0 );
	return ( replxx::Replxx::ACTION_RESULT::CONTINUE );
}
#else
HRepl::ret_t HRepl::handle_key( const char* key_ ) {
#ifndef USE_EDITLINE
	static int const CC_REDISPLAY = 0;
	static int const CC_ERROR = 0;
#endif
	key_table_t::const_iterator it( _keyTable.find( key_ ) );
	if ( ( it != _keyTable.end() ) && !! it->second ) {
		model_to_env();
		it->second();
		env_to_model();
		return ( CC_REDISPLAY );
	}
	return ( CC_ERROR );
}
HRepl::ret_t HRepl::handle_key_C_a( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "C-a" ) );
}
HRepl::ret_t HRepl::handle_key_C_b( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "C-b" ) );
}
HRepl::ret_t HRepl::handle_key_C_c( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "C-c" ) );
}
HRepl::ret_t HRepl::handle_key_C_d( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "C-d" ) );
}
HRepl::ret_t HRepl::handle_key_C_e( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "C-e" ) );
}
HRepl::ret_t HRepl::handle_key_C_f( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "C-f" ) );
}
HRepl::ret_t HRepl::handle_key_C_g( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "C-g" ) );
}
HRepl::ret_t HRepl::handle_key_C_h( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "C-h" ) );
}
HRepl::ret_t HRepl::handle_key_C_i( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "C-i" ) );
}
HRepl::ret_t HRepl::handle_key_C_j( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "C-j" ) );
}
HRepl::ret_t HRepl::handle_key_C_k( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "C-k" ) );
}
HRepl::ret_t HRepl::handle_key_C_l( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "C-l" ) );
}
HRepl::ret_t HRepl::handle_key_C_m( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "C-m" ) );
}
HRepl::ret_t HRepl::handle_key_C_n( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "C-n" ) );
}
HRepl::ret_t HRepl::handle_key_C_o( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "C-o" ) );
}
HRepl::ret_t HRepl::handle_key_C_p( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "C-p" ) );
}
HRepl::ret_t HRepl::handle_key_C_q( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "C-q" ) );
}
HRepl::ret_t HRepl::handle_key_C_r( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "C-r" ) );
}
HRepl::ret_t HRepl::handle_key_C_s( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "C-s" ) );
}
HRepl::ret_t HRepl::handle_key_C_t( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "C-t" ) );
}
HRepl::ret_t HRepl::handle_key_C_u( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "C-u" ) );
}
HRepl::ret_t HRepl::handle_key_C_v( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "C-v" ) );
}
HRepl::ret_t HRepl::handle_key_C_w( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "C-w" ) );
}
HRepl::ret_t HRepl::handle_key_C_x( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "C-x" ) );
}
HRepl::ret_t HRepl::handle_key_C_y( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "C-y" ) );
}
HRepl::ret_t HRepl::handle_key_C_z( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "C-z" ) );
}
HRepl::ret_t HRepl::handle_key_C_0( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "C-0" ) );
}
HRepl::ret_t HRepl::handle_key_C_1( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "C-1" ) );
}
HRepl::ret_t HRepl::handle_key_C_2( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "C-2" ) );
}
HRepl::ret_t HRepl::handle_key_C_3( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "C-3" ) );
}
HRepl::ret_t HRepl::handle_key_C_4( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "C-4" ) );
}
HRepl::ret_t HRepl::handle_key_C_5( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "C-5" ) );
}
HRepl::ret_t HRepl::handle_key_C_6( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "C-6" ) );
}
HRepl::ret_t HRepl::handle_key_C_7( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "C-7" ) );
}
HRepl::ret_t HRepl::handle_key_C_8( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "C-8" ) );
}
HRepl::ret_t HRepl::handle_key_C_9( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "C-9" ) );
}
HRepl::ret_t HRepl::handle_key_M_a( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "M-a" ) );
}
HRepl::ret_t HRepl::handle_key_M_b( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "M-b" ) );
}
HRepl::ret_t HRepl::handle_key_M_c( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "M-c" ) );
}
HRepl::ret_t HRepl::handle_key_M_d( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "M-d" ) );
}
HRepl::ret_t HRepl::handle_key_M_e( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "M-e" ) );
}
HRepl::ret_t HRepl::handle_key_M_f( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "M-f" ) );
}
HRepl::ret_t HRepl::handle_key_M_g( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "M-g" ) );
}
HRepl::ret_t HRepl::handle_key_M_h( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "M-h" ) );
}
HRepl::ret_t HRepl::handle_key_M_i( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "M-i" ) );
}
HRepl::ret_t HRepl::handle_key_M_j( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "M-j" ) );
}
HRepl::ret_t HRepl::handle_key_M_k( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "M-k" ) );
}
HRepl::ret_t HRepl::handle_key_M_l( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "M-l" ) );
}
HRepl::ret_t HRepl::handle_key_M_m( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "M-m" ) );
}
HRepl::ret_t HRepl::handle_key_M_n( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "M-n" ) );
}
HRepl::ret_t HRepl::handle_key_M_o( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "M-o" ) );
}
HRepl::ret_t HRepl::handle_key_M_p( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "M-p" ) );
}
HRepl::ret_t HRepl::handle_key_M_q( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "M-q" ) );
}
HRepl::ret_t HRepl::handle_key_M_r( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "M-r" ) );
}
HRepl::ret_t HRepl::handle_key_M_s( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "M-s" ) );
}
HRepl::ret_t HRepl::handle_key_M_t( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "M-t" ) );
}
HRepl::ret_t HRepl::handle_key_M_u( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "M-u" ) );
}
HRepl::ret_t HRepl::handle_key_M_v( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "M-v" ) );
}
HRepl::ret_t HRepl::handle_key_M_w( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "M-w" ) );
}
HRepl::ret_t HRepl::handle_key_M_x( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "M-x" ) );
}
HRepl::ret_t HRepl::handle_key_M_y( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "M-y" ) );
}
HRepl::ret_t HRepl::handle_key_M_z( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "M-z" ) );
}
HRepl::ret_t HRepl::handle_key_M_0( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "M-0" ) );
}
HRepl::ret_t HRepl::handle_key_M_1( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "M-1" ) );
}
HRepl::ret_t HRepl::handle_key_M_2( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "M-2" ) );
}
HRepl::ret_t HRepl::handle_key_M_3( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "M-3" ) );
}
HRepl::ret_t HRepl::handle_key_M_4( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "M-4" ) );
}
HRepl::ret_t HRepl::handle_key_M_5( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "M-5" ) );
}
HRepl::ret_t HRepl::handle_key_M_6( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "M-6" ) );
}
HRepl::ret_t HRepl::handle_key_M_7( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "M-7" ) );
}
HRepl::ret_t HRepl::handle_key_M_8( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "M-8" ) );
}
HRepl::ret_t HRepl::handle_key_M_9( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "M-9" ) );
}
HRepl::ret_t HRepl::handle_key_F1( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "F1" ) );
}
HRepl::ret_t HRepl::handle_key_F2( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "F2" ) );
}
HRepl::ret_t HRepl::handle_key_F3( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "F3" ) );
}
HRepl::ret_t HRepl::handle_key_F4( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "F4" ) );
}
HRepl::ret_t HRepl::handle_key_F5( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "F5" ) );
}
HRepl::ret_t HRepl::handle_key_F6( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "F6" ) );
}
HRepl::ret_t HRepl::handle_key_F7( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "F7" ) );
}
HRepl::ret_t HRepl::handle_key_F8( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "F8" ) );
}
HRepl::ret_t HRepl::handle_key_F9( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "F9" ) );
}
HRepl::ret_t HRepl::handle_key_F10( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "F10" ) );
}
HRepl::ret_t HRepl::handle_key_F11( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "F11" ) );
}
HRepl::ret_t HRepl::handle_key_F12( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "F12" ) );
}
HRepl::ret_t HRepl::handle_key_SF1( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "S-F1" ) );
}
HRepl::ret_t HRepl::handle_key_SF2( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "S-F2" ) );
}
HRepl::ret_t HRepl::handle_key_SF3( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "S-F3" ) );
}
HRepl::ret_t HRepl::handle_key_SF4( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "S-F4" ) );
}
HRepl::ret_t HRepl::handle_key_SF5( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "S-F5" ) );
}
HRepl::ret_t HRepl::handle_key_SF6( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "S-F6" ) );
}
HRepl::ret_t HRepl::handle_key_SF7( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "S-F7" ) );
}
HRepl::ret_t HRepl::handle_key_SF8( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "S-F8" ) );
}
HRepl::ret_t HRepl::handle_key_SF9( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "S-F9" ) );
}
HRepl::ret_t HRepl::handle_key_SF10( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "S-F10" ) );
}
HRepl::ret_t HRepl::handle_key_SF11( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "S-F11" ) );
}
HRepl::ret_t HRepl::handle_key_SF12( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "S-F12" ) );
}
HRepl::ret_t HRepl::handle_key_CF1( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "C-F1" ) );
}
HRepl::ret_t HRepl::handle_key_CF2( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "C-F2" ) );
}
HRepl::ret_t HRepl::handle_key_CF3( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "C-F3" ) );
}
HRepl::ret_t HRepl::handle_key_CF4( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "C-F4" ) );
}
HRepl::ret_t HRepl::handle_key_CF5( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "C-F5" ) );
}
HRepl::ret_t HRepl::handle_key_CF6( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "C-F6" ) );
}
HRepl::ret_t HRepl::handle_key_CF7( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "C-F7" ) );
}
HRepl::ret_t HRepl::handle_key_CF8( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "C-F8" ) );
}
HRepl::ret_t HRepl::handle_key_CF9( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "C-F9" ) );
}
HRepl::ret_t HRepl::handle_key_CF10( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "C-F10" ) );
}
HRepl::ret_t HRepl::handle_key_CF11( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "C-F11" ) );
}
HRepl::ret_t HRepl::handle_key_CF12( arg_t ud_, int ) {
	REPL_get_data( ud_ );
	return ( static_cast<HRepl*>( p )->handle_key( "C-F12" ) );
}
#endif

int long HRepl::do_write( void const* data_, int long size_ ) {
	print( static_cast<char const*>( data_ ) );
	return ( size_ );
}

int long HRepl::do_read( void*, int long ) {
	return ( 0 );
}

void HRepl::do_flush( void ) {
}

bool HRepl::do_is_valid( void ) const {
	return ( true );
}

HStreamInterface::POLL_TYPE HRepl::do_poll_type( void ) const {
	return ( POLL_TYPE::NATIVE );
}

void const* HRepl::do_data( void ) const {
	static int_native_t const STDOUT_FILENO( 1 );
	return ( reinterpret_cast<void const*>( STDOUT_FILENO ) );
}

}

