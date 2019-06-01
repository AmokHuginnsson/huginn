/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <yaal/tools/hpipedchild.hxx>
#include <yaal/tools/streamtools.hxx>

#include "forwardingshell.hxx"
#include "setup.hxx"

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;

namespace huginn {

bool HForwardingShell::do_try_command( yaal::hcore::HString const& command_ ) {
	M_PROLOG
	HPipedChild pc;
	HString shell( *setup._shell );
	HPipedChild::argv_t argv;
	while ( ! shell.is_empty() && ( shell.front() == '-'_ycp ) ) {
		int long commaIdx( shell.find( ','_ycp ) );
		argv.push_back( shell.substr( 0, commaIdx ) );
		shell.shift_left( commaIdx + 1 );
	}
	if ( shell.is_empty() ) {
		return ( false );
	}
	if ( shell.front() == ','_ycp ) {
		shell.shift_left( 1 );
	} else if ( argv.is_empty() ) {
		argv.push_back( "-c" );
	}
	argv.push_back( command_ );
	pc.spawn( shell, argv, &cin, &cout, &cerr );
	HPipedChild::STATUS s( pc.finish( OSetup::CENTURY_IN_SECONDS ) );
	return ( ( s.type == HPipedChild::STATUS::TYPE::NORMAL ) && ( s.value == 0 ) );
	M_EPILOG
}

}

