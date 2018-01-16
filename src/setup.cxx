/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <cstdio>

#include <yaal/tools/util.hxx>
M_VCSID( "$Id: " __ID__ " $" )
#include <yaal/hcore/hfile.hxx>
#include "setup.hxx"

using namespace yaal::hcore;

namespace huginn {

void OSetup::test_setup( void ) {
	M_PROLOG
	int errNo( 1 );
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
	if ( _interactive && ( _lint || _embedded || _nativeLines ) ) {
		yaal::tools::util::failure( errNo,
			_( "interactive is mutually axclusive with other switches\n" )
		);
	}
	++ errNo;
	if ( _hasProgram && ( _interactive || _lint || _embedded || _nativeLines || _jupyter ) ) {
		yaal::tools::util::failure( errNo,
			_( "one-liner code mode is exclusive with other modes of operation\n" )
		);
	}
	++ errNo;
	if ( _jupyter && ( _interactive || _lint || _embedded || _nativeLines ) ) {
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
	if ( _hasProgram && _beSloppy ) {
		yaal::tools::util::failure( errNo,
			_( "sloppy compiler mode is always selected for one-liner code mode\n" )
		);
	}
	++ errNo;
	if ( _noDefaultImports && ! _hasProgram ) {
		yaal::tools::util::failure( errNo,
			_( "default imports setting can be only used in one-liner mode\n" )
		);
	}
	++ errNo;
	if ( ! _genDocs.is_empty() && ( _interactive || _hasProgram || _lint || _jupyter ) ) {
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
	return;
	M_EPILOG
}

}

