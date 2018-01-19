/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <cstdio>

#include <yaal/tools/util.hxx>
M_VCSID( "$Id: " __ID__ " $" )
#include <yaal/hcore/hfile.hxx>
#include "setup.hxx"

using namespace yaal::hcore;

namespace huginn {

void OSetup::test_setup( int argc_ ) {
	M_PROLOG
	int errNo( 1 );
	if ( ( argc_ == 0 ) && ! ( _jupyter || _program || _streamEditor || _streamEditorSilent ) ) {
		_interactive = true;
	}
	if ( _quiet && _verbose ) {
		yaal::tools::util::failure( errNo,
			_( "quiet and verbose options are exclusive\n" )
		);
	}
	++ errNo;
	if ( _verbose ) {
		clog.reset( make_pointer<HFile>( stdout, HFile::OWNERSHIP::EXTERNAL ) );
	}
	if ( _nativeLines && ! _embedded ) {
		yaal::tools::util::failure( errNo,
			_( "native lines makes sense only with embedded\n" )
		);
	}
	++ errNo;
	if ( _lint && _beSloppy ) {
		yaal::tools::util::failure( errNo,
			_( "lint and be-sloppy options are mutually exclusive\n" )
		);
	}
	++ errNo;
	if ( _interactive && ( _lint || _embedded || _nativeLines || _streamEditor || _streamEditorSilent ) ) {
		yaal::tools::util::failure( errNo,
			_( "interactive mode is exclusive with other modes of operation\n" )
		);
	}
	++ errNo;
	if ( _program && ( _interactive || _lint || _embedded || _nativeLines || _jupyter ) ) {
		yaal::tools::util::failure( errNo,
			_( "one-liner code mode is exclusive with other modes of operation\n" )
		);
	}
	++ errNo;
	if ( _jupyter && ( _interactive || _lint || _embedded || _nativeLines || _streamEditor || _streamEditorSilent ) ) {
		yaal::tools::util::failure( errNo,
			_( "Jupyter backend mode is exclusive with other modes of operation\n" )
		);
	}
	++ errNo;
	if ( _interactive && _beSloppy ) {
		yaal::tools::util::failure( errNo,
			_( "sloppy compiler mode is always selected for interactive mode\n" )
		);
	}
	++ errNo;
	if ( _program && _beSloppy ) {
		yaal::tools::util::failure( errNo,
			_( "sloppy compiler mode is always selected for one-liner code mode\n" )
		);
	}
	++ errNo;
	if ( _noDefaultImports && ! _program ) {
		yaal::tools::util::failure( errNo,
			_( "default imports setting can be only used in one-liner mode\n" )
		);
	}
	++ errNo;
	if ( ! _genDocs.is_empty() && ( _interactive || _program || _lint || _jupyter ) ) {
		yaal::tools::util::failure( errNo,
			_( "gens-docs is not usable with real execution mode not with lint mode\n" )
		);
	}
	++ errNo;
	if ( _noColor && ! _interactive ) {
		yaal::tools::util::failure( errNo,
			_( "color setting can be only used in interactive mode\n" )
		);
	}
	++ errNo;
	if ( _noColor && ( _background == BACKGROUND::LIGHT ) ) {
		yaal::tools::util::failure( errNo,
			_( "bright background makes no sense with disabled colors\n" )
		);
	}
	++ errNo;
	if ( _streamEditor && _streamEditorSilent ) {
		yaal::tools::util::failure( errNo,
			_( "stream editor mode (**-p**) and silent stream editor mode (**-n**) are mutually exclusive\n" )
		);
	}
	++ errNo;
	if ( ( _streamEditor || _streamEditorSilent ) && ! _program ) {
		yaal::tools::util::failure( errNo,
			_( "stream editor mode (**-p**) or silent stream editor mode (**-n**) require program code (**-e**)\n" )
		);
	}
	++ errNo;
	if ( _inplace && ! ( _streamEditor || _streamEditorSilent ) ) {
		yaal::tools::util::failure( errNo,
			_( "in-place (**-i**) switch makes sense only for stream editor mode (**-p**) or silent stream editor mode (**-n**)\n" )
		);
	}
	/* Normalize switches. */
	if ( _streamEditorSilent ) {
		_streamEditorSilent = false;
		_streamEditor = true;
		_quiet = true;
	}
	return;
	M_EPILOG
}

}

