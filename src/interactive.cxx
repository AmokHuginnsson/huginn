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

#include <yaal/hcore/hfile.hxx>
#include <yaal/tools/ansi.hxx>

#include <readline/readline.h>
#include <readline/history.h>

M_VCSID( "$Id: " __ID__ " $" )
#include "interactive.hxx"
#include "linerunner.hxx"
#include "setup.hxx"

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;
using namespace yaal::tools::huginn;

namespace huginn {


namespace {

HLineRunner* _lineRunner_( nullptr );
char* completion_words( char const* prefix_, int state_ ) {
	static int index( 0 );
	rl_completion_suppress_append = 1;
	if ( state_ == 0 ) {
		index = 0;
	}
	HLineRunner::words_t const& words( _lineRunner_->words() );
	int len( static_cast<int>( ::strlen( prefix_ ) ) );
	char* p( nullptr );
	if ( len > 0 ) {
		for ( ; index < words.get_size(); ++ index ) {
			if ( strncmp( prefix_, words[index].raw(), static_cast<size_t>( len ) ) == 0 ) {
				p = strdup( words[index].raw() );
				break;
			}
		}
	} else if ( index < words.get_size() ) {
		p = strdup( words[index].c_str() );
	}
	++ index;
	return ( p );
}

void make_prompt( HString& prompt_, int& no_ ) {
	prompt_.clear();
	if ( ! setup._noColor ) {
		prompt_.assign( *ansi::blue );
	}
	prompt_.append( "huginn[" );
	if ( ! setup._noColor ) {
		prompt_.append( *ansi::brightblue );
	}
	prompt_.append( no_ );
	if ( ! setup._noColor ) {
		prompt_.append( *ansi::blue );
	}
	prompt_.append( "]> " );
	if ( ! setup._noColor ) {
		prompt_.append( *ansi::reset );
	}
	++ no_;
}

}

int interactive_session( void ) {
	M_PROLOG
	HString prompt;
	int lineNo( 0 );
	make_prompt( prompt, lineNo );
	HString line;
	HLineRunner lr( "*interactive session*" );
	_lineRunner_ = &lr;
	char* rawLine( nullptr );
	rl_completion_entry_function = completion_words;
	rl_basic_word_break_characters = " \t\n\"\\'`@$><=;|&{(.";
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
		if ( lr.add_line( line ) ) {
			HHuginn::value_t res( lr.execute() );
			if ( !! res && lr.use_result() && ( res->type_id() == HHuginn::TYPE::INTEGER ) ) {
				retVal = static_cast<int>( static_cast<HHuginn::HInteger*>( res.raw() )->value() );
			} else {
				retVal = 0;
			}
			if ( !! res ) {
				if ( lr.use_result() ) {
					cout << to_string( res, lr.huginn() ) << endl;
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

