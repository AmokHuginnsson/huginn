/*
---           `huginn' 0.0.0 (c) 1978 by Marcin 'Amok' Konarski            ---

  meta.cxx - this file is integral part of `huginn' project.

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
M_VCSID( "$Id: " __ID__ " $" )
M_VCSID( "$Id: " __TID__ " $" )
#include "meta.hxx"

#include "setup.hxx"

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;
using namespace yaal::ansi;

namespace huginn {

namespace {
yaal::hcore::HString start( yaal::hcore::HString const& str_ ) {
	HString s( str_ );
	if ( ! ( setup._jupyter || setup._noColor ) ) {
		if ( str_ == "**" ) {
			s = *brightcyan;
		} else if ( str_ == "`" ) {
			s = *brightgreen;
		}
	}
	return ( s );
}
yaal::hcore::HString end( yaal::hcore::HString const& str_ ) {
	return ( ( setup._jupyter || setup._noColor ) ? str_ : *reset );
}
yaal::hcore::HString highlight( yaal::hcore::HString const& str_ ) {
	HString s( str_ );
	bool strong( false );
	bool emphasis( false );
	bool code( false );
	typedef HStack<HString> colors_t;
	colors_t colors;
	colors.push( *reset );
	if ( ! ( setup._jupyter || setup._noColor ) ) {
		s.clear();
		HString c;
		for ( HString::const_iterator it( str_.begin() ), end( str_.end() ); it != end; ++ it ) {
			c = *it;
			if ( *it == '`' ) {
				if ( code ) {
					colors.pop();
				} else {
					colors.push( *brightgreen );
				}
				c = colors.top();
				code = !code;
			} else if ( *it == '*' ) {
				++ it;
				if ( ( it != end ) && ( *it == '*' ) ) {
					if ( strong ) {
						colors.pop();
					} else {
						colors.push( *brightcyan );
					}
					c = colors.top();
					strong = !strong;
				} else {
					-- it;
					if ( emphasis ) {
						colors.pop();
					} else {
						colors.push( *cyan );
					}
					c = colors.top();
					emphasis = !emphasis;
				}
			}
			s.append( c );
		}
	}
	return ( s );
}
}

bool meta( HLineRunner& lr_, yaal::hcore::HString const& line_ ) {
	M_PROLOG
	bool isMeta( true );
	if (  line_ == "// source" ) {
		cout << lr_.source();
	} else if (  line_ == "// imports" ) {
		for ( HLineRunner::lines_t::value_type const& l : lr_.imports() ) {
			cout << l << endl;
		}
	} else if (  line_.find( "// doc" ) == 0 ) {
		HString symbol( line_.substr( 7 ) );
		HString doc( lr_.doc( symbol ) );
		HDescription::words_t const& methods( lr_.methods( symbol ) );
		if ( ! doc.is_empty() ) {
			if ( ! methods.is_empty() && ( doc.find( "`"_ys.append( symbol ).append( "`" ) ) == HString::npos ) ) {
				cout << start( "`" ) << symbol << end( "`" ) << " - ";
			}
			cout << highlight( doc ) << endl;
			if ( ! methods.is_empty() ) {
				cout << "Class " << start( "`" ) << symbol << end( "`" ) << " has following members:" << endl;
			}
		} else if ( ! methods.is_empty() ) {
			cout << "Class " << start( "`" ) << symbol << end( "`" ) << " is not documented but has following members:" << endl;
		} else {
			cout << "symbol " << start( "`" ) << symbol << end( "`" ) << " is unknown or undocumented" << endl;
		}
		if ( ! methods.is_empty() ) {
			for ( yaal::hcore::HString const& m : methods ) {
				cout << "+ " << m << endl;
			}
		}
	} else if ( line_ == "// reset" ) {
		lr_.reset();
	} else {
		isMeta = false;
	}
	if ( isMeta ) {
		cout << "// ok" << endl;
	}
	return ( isMeta );
	M_EPILOG
}

}

