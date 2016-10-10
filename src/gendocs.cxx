/*
---           `huginn' 0.0.0 (c) 1978 by Marcin 'Amok' Konarski            ---

  gendocs.cxx - this file is integral part of `huginn' project.

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

#include <yaal/tools/hhuginn.hxx>
#include <yaal/hcore/hfile.hxx>
#include <yaal/tools/hstringstream.hxx>
M_VCSID( "$Id: " __ID__ " $" )
M_VCSID( "$Id: " __TID__ " $" )
#include "gendocs.hxx"
#include "description.hxx"
#include "setup.hxx"

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;

namespace huginn {

namespace {

HString escape( HString&& str_ ) {
	return ( str_.replace( "_", "\\_" ) );
}

}

int gen_docs( int argc_, char** argv_ ) {
	HHuginn::disable_grammar_verification();
	HHuginn h;
	HPointer<HFile> f;
	bool readFromScript( ( argc_ > 0 ) && ( argv_[0] != "-"_ys ) );
	if ( readFromScript ) {
		f = make_pointer<HFile>( argv_[0], HFile::OPEN::READING );
		if ( ! *f ) {
			throw HFileException( f->get_error() );
		}
	}
	HStringStream empty( "main(){}" );
	HStreamInterface* source( readFromScript ? static_cast<HStreamInterface*>( f.raw() ) : ( argc_ > 0 ? static_cast<HStreamInterface*>( &cin ) : &empty ) );
	int lineSkip( 0 );
	if ( setup._embedded ) {
		HString s;
		HRegex r( "^#!.*" YAAL_REGEX_WORD_START "huginn" YAAL_REGEX_WORD_END ".*" );
		while ( source->read_until( s ) > 0 ) {
			++ lineSkip;
			if ( r.matches( s ) ) {
				break;
			}
		}
	}
	h.load( *source, setup._nativeLines ? 0 : lineSkip );
	h.preprocess();
	int err( 0 );
	if ( h.parse() && h.compile( HHuginn::COMPILER::BE_SLOPPY ) ) {
		HDescription d;
		d.prepare( h );
		HString doc;
		HString cn;
		for ( yaal::hcore::HString const& c : d.classes() ) {
			doc = escape( d.doc( c ) );
			if ( ! doc.is_empty() ) {
				cn.assign( "`" ).append( c ).append( "`" );
				if ( doc.find( cn ) == HString::npos ) {
					cout << cn << " - ";
				}
				cout << doc << "  " << endl;
			} else if ( setup._verbose ) {
				cout << "`" << c << "` - *undocumented class*" << endl;
			}
			HDescription::words_t const& methods( d.methods( c ) );
			bool hasDoc( false );
			for ( yaal::hcore::HString const& m : methods ) {
				if ( ! d.doc( c, m ).is_empty() || setup._verbose ) {
					hasDoc = true;
					break;
				}
			}
			if ( hasDoc ) {
				cout << endl;
			}
			for ( yaal::hcore::HString const& m : methods ) {
				doc = escape( d.doc( c, m ) );
				if ( ! doc.is_empty() ) {
					cout << "+ " << doc << endl;
				} else if ( setup._verbose ) {
					cout << "+ **" << m << "()** - *undocumented method*" << endl;
				}
			}
			if ( hasDoc ) {
				cout << endl;
			}
		}
		for ( yaal::hcore::HString const& n : d.functions() ) {
			doc = escape( d.doc( n ) );
			if ( ! doc.is_empty() ) {
				cout << doc << "  " << endl;
			} else if ( setup._verbose ) {
				cout << "**" << n << "()** - *undocumented function*  " << endl;
			}
		}
	} else {
		err = 1;
		cerr << h.error_message() << endl;
	}
	return ( err );
}

}

