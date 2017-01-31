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

#include <readline/readline.h>
#include <readline/history.h>

M_VCSID( "$Id: " __ID__ " $" )
#include "interactive.hxx"
#include "linerunner.hxx"
#include "meta.hxx"
#include "setup.hxx"
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
		tokens_t yaalVersion( string::split<tokens_t>( yaal_version( true ), _whiteSpace_.data(), HTokenizer::DELIMITED_BY_ANY_OF ) );
		if ( ! setup._noColor ) {
			cout << *ansi::brightblue;
		}
		cout << endl
			<<   "  _                 _              | A programming language with no quirks," << endl
			<<   " | |               (_)             | so simple every child can master it." << endl
			<<   " | |___ _   _  ____ _ _____ _____  |" << endl
			<<   " |  _  | | | |/ _  | |  _  |  _  | | Homepage: http://codestation.org/" << endl
			<<   " | | | | |_| | |_| | | | | | | | | | " << PACKAGE_STRING << endl
			<< " |_| |_|\\__'_|\\__  |_|_| |_|_| |_| | " << COMMIT_ID << endl
			<<   "               __| |               | yaal " << yaalVersion[0] << endl
			<<   "              (____/               | " << yaalVersion[1];
		if ( ! setup._noColor ) {
			cout << *ansi::reset;
		}
		cout << endl << endl;
	}
	return;
}

HLineRunner* _lineRunner_( nullptr );
char* completion_words( char const* prefix_, int state_ ) {
	static int index( 0 );
	static HString prefix;
	static HString symbol;
	static HString buf;
	rl_completion_suppress_append = 1;
	static HLineRunner::words_t const* words( nullptr );
	if ( state_ == 0 ) {
		prefix = prefix_;
		int long sepIdx( prefix.find_last( '.' ) );
		symbol.clear();
		if ( sepIdx != HString::npos ) {
			symbol.assign( prefix, 0, sepIdx );
			prefix.shift_left( sepIdx + 1 );
		}
		index = 0;
		words = ! symbol.is_empty() ? &_lineRunner_->dependent_symbols( symbol ) : &_lineRunner_->words();
	}
	int len( static_cast<int>( prefix.get_length() ) );
	char* p( nullptr );
	if ( len > 0 ) {
		for ( ; index < words->get_size(); ++ index ) {
			if ( strncmp( prefix.raw(), (*words)[index].raw(), static_cast<size_t>( len ) ) == 0 ) {
				if ( symbol.is_empty() ) {
					p = strdup( (*words)[index].raw() );
				} else {
					buf.assign( symbol ).append( "." ).append( (*words)[index] ).append( "(" );
					p = strdup( buf.raw() );
				}
				break;
			}
		}
	} else if ( index < words->get_size() ) {
		if ( symbol.is_empty() ) {
			p = strdup( (*words)[index].raw() );
		} else {
			buf.assign( symbol ).append( "." ).append( (*words)[index] ).append( "()" );
			p = strdup( buf.raw() );
		}
	}
	++ index;
	return ( p );
}

void make_prompt( HString& prompt_, int& no_ ) {
	prompt_.clear();
	if ( ! setup._noColor ) {
		prompt_.assign( RL_PROMPT_START_IGNORE );
		prompt_.append( *ansi::blue );
		prompt_.append( RL_PROMPT_END_IGNORE );
	}
	prompt_.append( "huginn[" );
	if ( ! setup._noColor ) {
		prompt_.append( RL_PROMPT_START_IGNORE );
		prompt_.append( *ansi::brightblue );
		prompt_.append( RL_PROMPT_END_IGNORE );
	}
	prompt_.append( no_ );
	if ( ! setup._noColor ) {
		prompt_.append( RL_PROMPT_START_IGNORE );
		prompt_.append( *ansi::blue );
		prompt_.append( RL_PROMPT_END_IGNORE );
	}
	prompt_.append( "]> " );
	if ( ! setup._noColor ) {
		prompt_.append( RL_PROMPT_START_IGNORE );
		prompt_.append( *ansi::reset );
		prompt_.append( RL_PROMPT_END_IGNORE );
	}
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
				res.append( *ansi::brightmagenta );
			} break;
			case ( static_cast<int>( HHuginn::TYPE::FUNCTION_REFERENCE ) ): {
				if ( is_builtin( strRes ) ) {
					res.append( *ansi::brightgreen );
				} else if ( strRes == "Exception" ) {
					res.append( *ansi::brown );
				}
			} break;
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
	banner();
	HString prompt;
	int lineNo( 0 );
	make_prompt( prompt, lineNo );
	HString line;
	HLineRunner lr( "*interactive session*" );
	_lineRunner_ = &lr;
	char* rawLine( nullptr );
	rl_readline_name = "Huginn";
	rl_completion_entry_function = completion_words;
	rl_basic_word_break_characters = " \t\n\"\\'`@$><=?:;,|&![{()}]+-*/%^~";
	if ( ! setup._historyPath.is_empty() ) {
		read_history( setup._historyPath.c_str() );
	}
	int retVal( 0 );
	while ( ( rawLine = readline( prompt.raw() ) ) ) {
		line = rawLine;
		if ( ! line.is_empty() ) {
			add_history( rawLine );
		}
#ifndef __MSVCXX__
		memory::free0( rawLine );
#endif
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
				if ( lr.use_result() ) {
					cout << colorize( res, lr.huginn() ) << endl;
				}
			} else {
				cerr << lr.err() << endl;
			}
		} else {
			cerr << lr.err() << endl;
		}
		make_prompt( prompt, lineNo );
	}
	cout << endl;
	if ( ! setup._historyPath.is_empty() ) {
		write_history( setup._historyPath.c_str() );
	}
	return ( retVal );
	M_EPILOG
}

}

