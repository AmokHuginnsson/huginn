/*
---           `huginn' 0.0.0 (c) 1978 by Marcin 'Amok' Konarski            ---

  interactive.cxx - this file is integral part of `huginn' project.

  i.  You may not make any changes in Copyright information.
  ii. You must attach Copyright information to any part of every copy
      of this software.

Copyright:

 You can use this software free of charge and you can redistribute its binary
 package freely but:
  1. You are not allowed to use any part of sources of this software.
  2. You are not allowed to redistribute any part of sources of this software.
  3. You are not allowed to reverse engineer this software.
  4. If you want to distribute a binary package of this software you cannot
     demand any fees for it. You cannot even demand
     a return of cost of the media or distribution (CD for example).
  5. You cannot involve this software in any commercial activity (for example
     as a free add-on to paid software or newspaper).
 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. Use it at your own risk.
*/

#include <yaal/hcore/hcore.hxx>
#include <yaal/hcore/hfile.hxx>
#include <yaal/tools/ansi.hxx>
#include <yaal/tools/stringalgo.hxx>

#include <cstring>
#include <cstdio>

#include "config.hxx"

#ifdef USE_REPLXX
#	include <replxx.h>
#	define REPL_load_history replxx_history_load
#	define REPL_save_history replxx_history_save
#	define REPL_add_history replxx_history_add
#	define REPL_ignore_start ""
#	define REPL_ignore_end ""
#	define REPL_get_input replxx_input
#	define REPL_print replxx_print
#	define REPL_const
#elif defined( USE_EDITLINE )
#	include <yaal/tools/hterminal.hxx>
# include <histedit.h>
# include <signal.h>
#	define REPL_load_history( file ) history( hist, &histEvent, H_LOAD, file )
#	define REPL_save_history( file ) history( hist, &histEvent, H_SAVE, file )
#	define REPL_add_history( line ) history( hist, &histEvent, H_ENTER, line )
#	define REPL_ignore_start ""
#	define REPL_ignore_end ""
#	define REPL_get_input( ... ) el_gets( el, &elCount )
#	define REPL_print printf
#	define REPL_const const
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
#	define REPL_const
#endif

M_VCSID( "$Id: " __ID__ " $" )
#include "interactive.hxx"
#include "linerunner.hxx"
#include "meta.hxx"
#include "setup.hxx"
#include "colorize.hxx"
#include "symbolicnames.hxx"
#include "commit_id.hxx"

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;
using namespace yaal::tools::huginn;

namespace yaal { namespace tools { namespace huginn {
bool is_builtin( yaal::hcore::HString const& );
}}}

namespace huginn {

namespace {

void banner( void ) {
	if ( ! setup._quiet ) {
		typedef yaal::hcore::HArray<yaal::hcore::HString> tokens_t;\
		tokens_t yaalVersion( string::split<tokens_t>( yaal_version( true ), character_class( CHARACTER_CLASS::WHITESPACE ).data(), HTokenizer::DELIMITED_BY_ANY_OF ) );
		if ( ! setup._noColor ) {
			REPL_print( "%s", setup._brightBackground ? *ansi::blue : *ansi::brightblue );
		}
		cout << endl
			<<    "  _                 _              | A programming language with no quirks," << endl
			<<    " | |               (_)             | so simple every child can master it." << endl
			<<    " | |__  _   _  __ _ _ _ __  _ __   |" << endl
			<< " | '_ \\| | | |/ _` | | '_ \\| '_ \\  | Homepage: http://huginn.org/" << endl
			<<    " | | | | |_| | (_| | | | | | | | | | " << PACKAGE_STRING << endl
			<<  " |_| |_|\\__,_|\\__, |_|_| |_|_| |_| | " << COMMIT_ID << endl
			<<    "               __/ |               | yaal " << yaalVersion[0] << endl
			<<    "              (___/                | " << yaalVersion[1] << endl;
		if ( ! setup._noColor ) {
			REPL_print( "%s", *ansi::reset );
		}
		cout << endl;
	}
	return;
}

HLineRunner* _lineRunner_( nullptr );
static int const PROMPT_SIZE( 128 );
char const BREAK_CHARS_RAW[] = " \t\n\"\\'`@$><=?:;,|&![{()}]+-*/%^~";
HString const BREAK_CHARS( BREAK_CHARS_RAW );
char const SPECIAL_PREFIXES_RAW[] = "\\/";
HString const SPECIAL_PREFIXES( SPECIAL_PREFIXES_RAW );

HLineRunner::words_t completion_words( yaal::hcore::HString context_, yaal::hcore::HString prefix_ ) {
	M_PROLOG
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
		if ( ( context_.find( "//" ) == 0 ) && ( context_.find( "//doc " ) == HString::npos ) ) {
			HString symbolPrefix( context_.substr( 2 ) );
			for ( yaal::hcore::HString const& n : magic_names() ) {
				if ( symbolPrefix.is_empty() || ( n.find( symbolPrefix ) == 0 ) ) {
					completions.push_back( "//"_ys.append( n ).append( ' ' ) );
				}
			}
			break;
		}
		HString symbol;
		if ( dotIdx != HString::npos ) {
			symbol.assign( prefix_, 0, dotIdx );
			prefix_.shift_left( dotIdx + 1 );
		}
		HLineRunner::words_t const& words( ! symbol.is_empty() ? _lineRunner_->dependent_symbols( symbol ) : _lineRunner_->words() );
		int len( static_cast<int>( prefix_.get_length() ) );
		HString buf;
		for ( HString const& w : words ) {
			if ( ! prefix_.is_empty() && ( prefix_ != w.left( len ) ) ) {
				continue;
			}
			if ( symbol.is_empty() ) {
				completions.push_back( w );
			} else {
				buf.assign( symbol ).append( "." ).append( w ).append( "(" );
				completions.push_back( buf );
			}
		}
	} while ( false );
	return ( completions );
	M_EPILOG
}

#ifdef USE_REPLXX

void completion_words( char const* prefix_, int offset_, replxx_completions* completions_ ) {
	HString prefix( prefix_ );
	prefix.shift_left( offset_ );
	HLineRunner::words_t completions( completion_words( prefix_, prefix ) );
	HUTF8String utf8;
	for ( yaal::hcore::HString const& c : completions ) {
		utf8.assign( c );
		replxx_add_completion( completions_, utf8.c_str() );
	}
	return;
}

void colorize( char const* line_, replxx_color::color* colors_, int size_ ) {
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
	return ( static_cast<char*>( p ) );
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
	LineInfo const* li( el_line( el_ ) );
	HString context( li->buffer, li->cursor - li->buffer );
	int long stemStart( context.find_last_one_of( BREAK_CHARS ) );
	while ( ( stemStart >= 0 ) && ( SPECIAL_PREFIXES.find( context[stemStart] ) != HString::npos ) ) {
		-- stemStart;
	}
	HString prefix( stemStart != HString::npos ? context.substr( stemStart + 1, li->cursor - li->buffer - stemStart ) : context );
	int prefixLen( static_cast<int>( prefix.get_length() ) );
	HLineRunner::words_t completions( completion_words( context, prefix ) );
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

char* completion_words( char const* prefix_, int state_ ) {
	static int index( 0 );
	static HString prefix;
	static HString symbol;
	static HString buf;
	rl_completion_suppress_append = 1;
	static HLineRunner::words_t const* words( nullptr );
	static char const* symbolicName( nullptr );
	static symbolic_names_t symbolicNames;
	if ( state_ == 0 ) {
		prefix = prefix_;
		if ( prefix_[0] == '\\' ) {
			symbolicName = symbol_from_name( prefix );
			if ( symbolicName ) {
				return ( strdup( symbolicName ) );
			} else {
				symbolicNames = symbol_name_completions( prefix );
			}
		} else {
			int long sepIdx( prefix.find_last( '.'_ycp ) );
			symbol.clear();
			if ( sepIdx != HString::npos ) {
				symbol.assign( prefix, 0, sepIdx );
				prefix.shift_left( sepIdx + 1 );
			}
		}
		words = prefix_[0] == '\\' ? &symbolicNames : ( ! symbol.is_empty() ? &_lineRunner_->dependent_symbols( symbol ) : &_lineRunner_->words() );
		index = 0;
	}
	if ( symbolicName ) {
		symbolicName = nullptr;
		return ( nullptr );
	}
	int len( static_cast<int>( prefix.get_length() ) );
	char* p( nullptr );
	for ( ; index < words->get_size(); ++ index ) {
		if ( ! prefix.is_empty() && ( prefix != (*words)[index].left( len ) ) ) {
			continue;
		}
		if ( symbol.is_empty() ) {
			p = strdup( HUTF8String( (*words)[index] ).c_str() );
		} else {
			buf.assign( symbol ).append( "." ).append( (*words)[index] ).append( "(" );
			p = strdup( HUTF8String( buf ).c_str() );
		}
		break;
	}
	++ index;
	return ( p );
}

#endif

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
		condColor( setup._brightBackground ? *ansi::brightblue : *ansi::blue ),
		condColor( REPL_ignore_end ),
		condColor( REPL_ignore_start ),
		condColor( setup._brightBackground ? *ansi::blue : *ansi::brightblue ),
		condColor( REPL_ignore_end ),
		no_,
		condColor( REPL_ignore_start ),
		condColor( setup._brightBackground ? *ansi::brightblue : *ansi::blue ),
		condColor( REPL_ignore_end ),
		condColor( REPL_ignore_start ),
		condColor( *ansi::reset ),
		condColor( REPL_ignore_end )
	);
	++ no_;
}

HString colorize( HHuginn::value_t const& value_, HHuginn const* huginn_ ) {
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
				res.append( setup._brightBackground ? *ansi::magenta : *ansi::brightmagenta );
			} break;
			case ( static_cast<int>( HHuginn::TYPE::FUNCTION_REFERENCE ) ): {
				if ( is_builtin( strRes ) ) {
					res.append( setup._brightBackground ? *ansi::green : *ansi::brightgreen );
				} else if ( strRes == "Exception" ) {
					res.append( setup._brightBackground ? *ansi::brightred : *ansi::brown );
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
	set_color_scheme( setup._brightBackground );
	banner();
	char prompt[PROMPT_SIZE];
	int lineNo( 0 );
	make_prompt( prompt, PROMPT_SIZE, lineNo );
	HLineRunner lr( "*interactive session*" );
	_lineRunner_ = &lr;
	char REPL_const* rawLine( nullptr );
#ifdef USE_REPLXX
	replxx_set_ctx_completion_callback( completion_words );
	if ( ! setup._noColor ) {
		replxx_set_highlighter_callback( colorize );
	}
	replxx_set_special_prefixes( SPECIAL_PREFIXES_RAW );
	replxx_set_no_color( setup._noColor ? 1 : 0 );
#elif defined( USE_EDITLINE )
	EditLine* el( el_init( PACKAGE_NAME, stdin, stdout, stderr ) );
	History* hist( history_init() );
	HistEvent histEvent;
	::memset( &histEvent, 0, sizeof ( histEvent ) );
	int elCount( 0 );
	el_set( el, EL_EDITOR, "emacs" );
	el_set( el, EL_SIGNAL, SIGWINCH );
	el_set( el, EL_CLIENTDATA, prompt );
	el_set( el, EL_HIST, &history, hist );
	el_set( el, EL_PROMPT_ESC, el_make_prompt, 1 );
	el_set( el, EL_ADDFN, "complete", "Command completion", complete );
	el_set( el, EL_BIND, "^I", "complete", nullptr );
	el_set( el, EL_BIND, "\\e[1;5D", "ed-prev-word", nullptr );
	el_set( el, EL_BIND, "\\e[1;5C", "em-next-word", nullptr );
	el_set( el, EL_BIND, "\\ep", "ed-search-prev-history", nullptr );
	el_set( el, EL_BIND, "\\en", "ed-search-next-history", nullptr );
	history( hist, &histEvent, H_SETSIZE, 1000 );
	history( hist, &histEvent, H_SETUNIQUE, 1 );
#else
	rl_readline_name = PACKAGE_NAME;
	rl_completion_entry_function = completion_words;
	rl_basic_word_break_characters = BREAK_CHARS_RAW;
	rl_special_prefixes = "\\";
#endif
	if ( ! setup._historyPath.is_empty() ) {
		REPL_load_history( HUTF8String( setup._historyPath ).c_str() );
	}
	int retVal( 0 );
	HString line;
	HUTF8String colorized;
	lr.load_session();
	while ( setup._interactive && ( rawLine = REPL_get_input( prompt ) ) ) {
		line = rawLine;
		if ( ( rawLine[0] != 0 ) && ( rawLine[0] != ' ' ) ) {
			REPL_add_history( rawLine );
		}
#if defined( USE_REPLXX ) || ! ( defined( USE_EDITLINE ) || defined( __MSVCXX__ ) )
		memory::free0( rawLine );
#endif
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
					REPL_print( "%s\n", colorized.c_str() );
				}
			} else {
				cerr << lr.err() << endl;
			}
		} else {
			cerr << lr.err() << endl;
		}
		make_prompt( prompt, PROMPT_SIZE, lineNo );
	}
	if ( setup._interactive ) {
		REPL_print( "\n" );
	}
	lr.save_session();
	if ( ! setup._historyPath.is_empty() ) {
		REPL_save_history( HUTF8String( setup._historyPath ).c_str() );
	}
#ifdef USE_EDITLINE
	history_end( hist );
	el_end( el );
#endif
	return ( retVal );
	M_EPILOG
}

}

