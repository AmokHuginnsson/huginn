/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <cstring>
#include <cstdio>

#include <yaal/tools/stringalgo.hxx>

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
#	define REPL_get_input _replxx.input
#	define REPL_print _replxx.print
#	define REPL_bind_key( seq, fun, name ) /**/
#	define REPL_get_data( ud ) /**/
using namespace replxx;
#elif defined( USE_EDITLINE )
#	include <yaal/tools/hterminal.hxx>
#	include <histedit.h>
#	include <signal.h>
#	define REPL_load_history( file ) history( _hist, &_histEvent, H_LOAD, file )
#	define REPL_save_history( file ) history( _hist, &_histEvent, H_SAVE, file )
#	define REPL_add_history( line ) history( _hist, &_histEvent, H_ENTER, line )
#	define REPL_get_input( ... ) el_gets( _el, &_count )
#	define REPL_print printf
#	define REPL_bind_key( seq, fun, name ) el_set( _el, EL_BIND, ( seq ), ( name ), nullptr )
#	define REPL_get_data( ud ) void* p( nullptr ); el_get( ( ud ), EL_CLIENTDATA, &p );
#else
#	include <readline/readline.h>
#	include <readline/history.h>
#	define REPL_load_history read_history
#	define REPL_save_history write_history
#	define REPL_add_history add_history
#	define REPL_get_input readline
#	define REPL_print printf
#	define REPL_bind_key( seq, fun, name ) rl_bind_keyseq( ( seq ), ( fun ) )
#	define REPL_get_data( ud ) static_cast<void>( ud ); HRepl* p( _repl_ )
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

/*
 * The dot character ('.') shall be explicitly removed from set of word breaking
 * characters to support `obj.member` semantics.
 * The underscore character ('_') removed to support standard idenfifiers (`some_symbol`)
 * from programming languages.
 */
char const BREAK_CHARS_HUGINN_RAW[] = " \t\v\f\a\b\r\n`~!@#$%^&*()-=+[{]}\\|;:'\",<>/?";
character_class_t const BREAK_CHARACTERS_HUGINN_CLASS( BREAK_CHARS_HUGINN_RAW, static_cast<int>( sizeof ( BREAK_CHARS_HUGINN_RAW ) - 1 ) );
char const BREAK_CHARACTERS_SHELL_RAW[] = " \t\n\"\\'`@$><=;|&{(";
character_class_t const BREAK_CHARACTERS_SHELL_CLASS( BREAK_CHARACTERS_SHELL_RAW, static_cast<int>( sizeof ( BREAK_CHARACTERS_SHELL_RAW ) - 1 ) );
char const SPECIAL_PREFIXES_RAW[] = "\\^";
HString const SPECIAL_PREFIXES( SPECIAL_PREFIXES_RAW );

}

int context_length( yaal::hcore::HString const& input_, CONTEXT_TYPE ) {
	M_PROLOG
	int contextLength( 0 );
	int i( static_cast<int>( input_.get_length() - 1 ) );
	while ( ( i >= 0 )  && ! BREAK_CHARACTERS_HUGINN_CLASS.has( input_[i] ) ) {
		++ contextLength;
		-- i;
	}
	while ( ( i >= 0 )  && ( SPECIAL_PREFIXES.find( input_[i] ) != HString::npos ) ) {
		++ contextLength;
		-- i;
	}
	return ( contextLength );
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
	HRepl::completions_t completions( static_cast<HRepl*>( data_ )->completion_words( context_.c_str(), yaal::move( prefix ), contextLen_ ) );
	HUTF8String utf8;
	Replxx::completions_t replxxCompletions;
	for ( HRepl::HCompletion const& c : completions ) {
		utf8.assign( c.text() );
		replxxCompletions.emplace_back( utf8.c_str(), static_cast<Replxx::Color>( c.color() ) );
	}
	return ( replxxCompletions );
	M_EPILOG
}

Replxx::hints_t find_hints( std::string const& prefix_, int& contextLen_, Replxx::Color& color_, void* data_ ) {
	M_PROLOG
	HRepl* repl( static_cast<HRepl*>( data_ ) );
	HString context( prefix_.c_str() );
	contextLen_ = context_length( context, CONTEXT_TYPE::HUGINN );
	HString prefix( prefix_.c_str() );
	prefix.shift_left( context.get_length() - contextLen_ );
	if ( ( prefix.is_empty() && ( prefix_ != "//" ) ) || ( prefix == "." ) ) {
		return ( Replxx::hints_t() );
	}
	bool inDocContext( context.find( "//doc " ) == 0 );
	HRepl::completions_t hints( repl->completion_words( prefix_.c_str(), HString( prefix ), contextLen_, false ) );
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
			HString obj( repl->line_runner()->symbol_type_name( ask.left( dotIdx ) ) );
			HString member( ask.mid( dotIdx + 1 ) );
			ask.assign( obj ).append( '.' ).append( member );
			if ( repl->line_runner()->symbol_kind( ask ) != HDescription::SYMBOL_KIND::CLASS ) {
				toStrip = member.get_length();
			} else {
				doc.assign( " - " );
			}
		} else if ( repl->line_runner()->symbol_kind( ask ) != HDescription::SYMBOL_KIND::CLASS ) {
			toStrip = h.get_length();
		} else {
			doc.assign( " - " );
		}
		doc.append( repl->line_runner()->doc( ask, inDocContext ) );
		doc.replace( "*", "" );
		doc.shift_left( toStrip );
		utf8.assign( h.append( doc ) );
		replxxHints.emplace_back( utf8.c_str() );
	}
	color_ = _replxxColors_.at( color( GROUP::HINT ) );
	return ( replxxHints );
	M_EPILOG
}

void replxx_colorize( std::string const& line_, Replxx::colors_t& colors_, void* data_ ) {
	M_PROLOG
	HRepl* repl( static_cast<HRepl*>( data_ ) );
	HString line( line_.c_str() );
	colors_t colors;
	if ( ! ( repl->shell() && repl->shell()->is_valid_command( line ) ) ) {
		::huginn::colorize( line, colors );
	} else {
		shell_colorize( line, colors, repl->shell() );
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
	HRepl::completions_t completions( repl->completion_words( yaal::move( context ), yaal::move( prefix ), contextLen ) );
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

HRepl* _repl_( nullptr );

char* rl_word_break_characters( void ) {
	if ( _repl_->shell() ) {
		rl_basic_word_break_characters = const_cast<char*>( BREAK_CHARACTERS_SHELL_CLASS.data() );
		rl_completer_word_break_characters = const_cast<char*>( BREAK_CHARACTERS_SHELL_CLASS.data() );
	} else {
		rl_basic_word_break_characters = const_cast<char*>( BREAK_CHARACTERS_HUGINN_CLASS.data() );
		rl_completer_word_break_characters = const_cast<char*>( BREAK_CHARACTERS_HUGINN_CLASS.data() );
	}
	rl_stuff_char( 0 );
	return ( const_cast<char*>( rl_basic_word_break_characters ) );
}

char* rl_completion_words( char const*, int state_ ) {
	static int index( 0 );
	static HString context;
	static HString prefix;
	rl_completion_suppress_append = 1;
	static HRepl::completions_t completions;
	if ( state_ == 0 ) {
		context.assign( rl_line_buffer );
		int contextLen( context_length( context, CONTEXT_TYPE::HUGINN ) );
		prefix = context.right( contextLen );
		completions = _repl_->completion_words( yaal::move( context ), yaal::move( prefix ), contextLen );
		index = 0;
	}
	char* p( nullptr );
	if ( index < completions.get_size() ) {
		HString const& word( completions[index].text() );
		int skip( word.starts_with( "//" ) ? 2 : 0 );
		p = strdup( HUTF8String( word ).c_str() + skip );
	}
	++ index;
	return ( p );
}

#endif

}

HRepl::HRepl( void )
#ifdef USE_REPLXX
	: _replxx()
	, _keyTable({
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
	: _el( el_init( PACKAGE_NAME, stdin, stdout, stderr ) )
	, _hist( history_init() )
	, _histEvent()
	, _count( 0 )
	, _keyTable()
#else
	: _keyTable()
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
	el_set( _el, EL_HIST, &history, _hist );
	el_set( _el, EL_PROMPT_ESC, el_make_prompt, 1 );
	el_set( _el, EL_BIND, "\\e[1;5D", "ed-prev-word", nullptr );
	el_set( _el, EL_BIND, "\\e[1;5C", "em-next-word", nullptr );
	el_set( _el, EL_BIND, "\\ep", "ed-search-prev-history", nullptr );
	el_set( _el, EL_BIND, "\\en", "ed-search-next-history", nullptr );
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
	history( _hist, &_histEvent, H_SETSIZE, 1000 );
	history( _hist, &_histEvent, H_SETUNIQUE, 1 );
#else
	_repl_ = this;
	rl_readline_name = PACKAGE_NAME;
#endif
	REPL_bind_key( "\033OP",     HRepl::handle_key_F1,   "repl_key_F1" );
	REPL_bind_key( "\033OQ",     HRepl::handle_key_F2,   "repl_key_F2" );
	REPL_bind_key( "\033OR",     HRepl::handle_key_F3,   "repl_key_F3" );
	REPL_bind_key( "\033OS",     HRepl::handle_key_F4,   "repl_key_F4" );
	REPL_bind_key( "\033[15~",   HRepl::handle_key_F5,   "repl_key_F5" );
	REPL_bind_key( "\033[17~",   HRepl::handle_key_F6,   "repl_key_F6" );
	REPL_bind_key( "\033[18~",   HRepl::handle_key_F7,   "repl_key_F7" );
	REPL_bind_key( "\033[19~",   HRepl::handle_key_F8,   "repl_key_F8" );
	REPL_bind_key( "\033[20~",   HRepl::handle_key_F9,   "repl_key_F9" );
	REPL_bind_key( "\033[21~",   HRepl::handle_key_F10,  "repl_key_F10" );
	REPL_bind_key( "\033[23~",   HRepl::handle_key_F11,  "repl_key_F11" );
	REPL_bind_key( "\033[24~",   HRepl::handle_key_F12,  "repl_key_F12" );
	REPL_bind_key( "\033[1;2P",  HRepl::handle_key_SF1,  "repl_key_SF1" );
	REPL_bind_key( "\033[1;2Q",  HRepl::handle_key_SF2,  "repl_key_SF2" );
	REPL_bind_key( "\033[1;2R",  HRepl::handle_key_SF3,  "repl_key_SF3" );
	REPL_bind_key( "\033[1;2S",  HRepl::handle_key_SF4,  "repl_key_SF4" );
	REPL_bind_key( "\033[15;2~", HRepl::handle_key_SF5,  "repl_key_SF5" );
	REPL_bind_key( "\033[17;2~", HRepl::handle_key_SF6,  "repl_key_SF6" );
	REPL_bind_key( "\033[18;2~", HRepl::handle_key_SF7,  "repl_key_SF7" );
	REPL_bind_key( "\033[19;2~", HRepl::handle_key_SF8,  "repl_key_SF8" );
	REPL_bind_key( "\033[20;2~", HRepl::handle_key_SF9,  "repl_key_SF9" );
	REPL_bind_key( "\033[21;2~", HRepl::handle_key_SF10, "repl_key_SF10" );
	REPL_bind_key( "\033[23;2~", HRepl::handle_key_SF11, "repl_key_SF11" );
	REPL_bind_key( "\033[24;2~", HRepl::handle_key_SF12, "repl_key_SF12" );
	REPL_bind_key( "\033[1;5P",  HRepl::handle_key_CF1,  "repl_key_CF1" );
	REPL_bind_key( "\033[1;5Q",  HRepl::handle_key_CF2,  "repl_key_CF2" );
	REPL_bind_key( "\033[1;5R",  HRepl::handle_key_CF3,  "repl_key_CF3" );
	REPL_bind_key( "\033[1;5S",  HRepl::handle_key_CF4,  "repl_key_CF4" );
	REPL_bind_key( "\033[15;5~", HRepl::handle_key_CF5,  "repl_key_CF5" );
	REPL_bind_key( "\033[17;5~", HRepl::handle_key_CF6,  "repl_key_CF6" );
	REPL_bind_key( "\033[18;5~", HRepl::handle_key_CF7,  "repl_key_CF7" );
	REPL_bind_key( "\033[19;5~", HRepl::handle_key_CF8,  "repl_key_CF8" );
	REPL_bind_key( "\033[20;5~", HRepl::handle_key_CF9,  "repl_key_CF9" );
	REPL_bind_key( "\033[21;5~", HRepl::handle_key_CF10, "repl_key_CF10" );
	REPL_bind_key( "\033[23;5~", HRepl::handle_key_CF11, "repl_key_CF11" );
	REPL_bind_key( "\033[24;5~", HRepl::handle_key_CF12, "repl_key_CF12" );
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
	_replxx.set_completion_callback( std::bind( &replxx_completion_words, std::placeholders::_1, std::placeholders::_2, this ) );
	if ( ! setup._noColor ) {
		_replxx.set_highlighter_callback( std::bind( replxx_colorize, std::placeholders::_1, std::placeholders::_2, this ) );
		_replxx.set_hint_callback( std::bind( find_hints, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, this ) );
	}
#elif defined( USE_EDITLINE )
	el_set( _el, EL_ADDFN, "complete", "Command completion", complete );
	el_set( _el, EL_BIND, "^I", "complete", nullptr );
#else
	rl_completion_entry_function = rl_completion_words;
	rl_completion_word_break_hook = rl_word_break_characters;
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

HRepl::completions_t HRepl::completion_words( yaal::hcore::HString&& context_, yaal::hcore::HString&& prefix_, int& contextLen_, bool shell_ ) {
	HScopedValueReplacement<HShell*> svr( _shell, shell_ ? _shell : nullptr );
	return ( _completer( yaal::move( context_ ), yaal::move( prefix_ ), contextLen_, this ) );
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

#ifdef USE_REPLXX
void HRepl::set_hint_delay( int ms ) {
	_replxx.set_hint_delay( ms );
}
#else
void HRepl::set_hint_delay( int ) {
}
#endif

#ifdef USE_REPLXX
void HRepl::bind_key( yaal::hcore::HString const& key_, action_t const& action_ ) {
	_replxx.bind_key( _keyTable.at( key_ ), call( &HRepl::run_action, this, action_, _1 ) );
}
#else
void HRepl::bind_key( yaal::hcore::HString const& key_, action_t const& action_ ) {
	_keyTable[key_] = action_;
}
#endif

#ifdef USE_REPLXX
replxx::Replxx::ACTION_RESULT HRepl::run_action( action_t action_, char32_t ) {
	_replxx.invoke( Replxx::ACTION::CLEAR_SELF, 0 );
	action_();
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
	if ( it != _keyTable.end() ) {
		cout << endl;
		it->second();
		return ( CC_REDISPLAY );
	}
	return ( CC_ERROR );
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

}

