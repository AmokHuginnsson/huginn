/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <yaal/tools/hhuginn.hxx>
#include <yaal/hcore/hfile.hxx>
#include <yaal/tools/hstringstream.hxx>
#include <yaal/tools/filesystem.hxx>
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
		HRegex r( "^#!.*\\bhuginn\\b.*" );
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
		HString decoratedClassName;
		HString decoratedUnqualifiedClassName;
		HString unqualifiedClassName;
		bool hasAnyClassDoc( false );
		bool hadClassDoc( false );
		bool toStdout( setup._genDocs == "-" );
		bool splitDoc( ! toStdout && filesystem::is_directory( setup._genDocs ) );
		HFile output;
		HStreamInterface& dest( toStdout ? static_cast<HStreamInterface&>( cout ) : output );
		if ( ! splitDoc && ! toStdout ) {
			output.open( setup._genDocs, HFile::OPEN::WRITING );
		}
		for ( yaal::hcore::HString const& className : d.classes() ) {
			if ( hadClassDoc && ! splitDoc ) {
				dest << "---" << endl << endl;
			}
			hadClassDoc = false;
			doc = escape( d.doc( className ) );
			if ( splitDoc && ( ! doc.is_empty() || setup._verbose ) ) {
				if ( output.is_opened() ) {
					output.close();
				}
				output.open( setup._genDocs + "/" + className + ".md", HFile::OPEN::WRITING );
			}
			if ( ! doc.is_empty() ) {
				dest << "##### ";
				HString::size_type dotPos( className.find_last( '.'_ycp ) );
				unqualifiedClassName.assign( className, dotPos != HString::npos ? dotPos + 1 : 0 );
				decoratedClassName.assign( "`" ).append( className ).append( "`" );
				decoratedUnqualifiedClassName.assign( "`" ).append( unqualifiedClassName ).append( "`" );
				bool haveUnqualifiedNameInDoc( doc.find( decoratedUnqualifiedClassName ) != HString::npos );
				if ( ( doc.find( decoratedClassName ) == HString::npos ) && ! haveUnqualifiedNameInDoc ) {
					dest << decoratedClassName << " - ";
				} else if ( ( decoratedUnqualifiedClassName != decoratedClassName ) && haveUnqualifiedNameInDoc ) {
					doc.replace( unqualifiedClassName, className );
				}
				dest << doc << "  " << endl << endl;
				hasAnyClassDoc = hadClassDoc = true;
			} else if ( setup._verbose ) {
				dest << "`" << className << "` - *undocumented class*  " << endl << endl;
				hasAnyClassDoc = hadClassDoc = true;
			}
			HDescription::words_t const& members( d.members( className ) );
			bool hasMethodDoc( false );
			for ( yaal::hcore::HString const& m : members ) {
				if ( ! d.doc( className, m ).is_empty() ) {
					hasAnyClassDoc = hadClassDoc = hasMethodDoc = true;
				} else if ( setup._verbose ) {
					hasAnyClassDoc = hadClassDoc = hasMethodDoc = true;
				}
			}

			if ( hasMethodDoc ) {
				dest << "###### Members" << endl << endl;
			}

			for ( yaal::hcore::HString const& m : members ) {
				doc = escape( d.doc( className, m ) );
				if ( ! doc.is_empty() ) {
					if ( doc.front() != '*' ) {
						continue;
					}
					dest << "+ " << doc << endl;
				} else if ( setup._verbose ) {
					dest << "+ **" << m << "()** - *undocumented method*" << endl;
				}
			}
			if ( hasMethodDoc ) {
				dest << endl;
			}
		}
		if ( hasAnyClassDoc && hadClassDoc ) {
			dest << "---" << endl << endl;
		}
		for ( yaal::hcore::HString const& n : d.functions() ) {
			doc = escape( d.doc( n ) );
			if ( splitDoc && ( ! doc.is_empty() || setup._verbose ) ) {
				if ( output.is_opened() ) {
					output.close();
				}
				output.open( setup._genDocs + "/" + n + ".md", HFile::OPEN::WRITING );
			}
			if ( ! doc.is_empty() ) {
				dest << doc << "  " << endl;
			} else if ( setup._verbose ) {
				dest << "**" << n << "()** - *undocumented function*  " << endl;
			}
		}
	} else {
		err = 1;
		cerr << h.error_message() << endl;
	}
	return err;
}

}

