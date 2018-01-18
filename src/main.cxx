/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <cstdlib>

#include <yaal/hcore/hlog.hxx>
#include <yaal/tools/signals.hxx>
#include <yaal/tools/util.hxx>
#include <yaal/tools/hthreadpool.hxx>
#if defined( __MSVCXX__ ) || defined( __HOST_OS_TYPE_CYGWIN__ )
#include <yaal/dbwrapper/dbwrapper.hxx>
#endif /* #if defined( __MSVCXX__ ) || defined( __HOST_OS_TYPE_CYGWIN__ ) */
M_VCSID( "$Id: " __ID__ " $" )
#include "huginn.hxx"
#include "interactive.hxx"
#include "jupyter.hxx"
#include "oneliner.hxx"
#include "gendocs.hxx"

#include "setup.hxx"
#include "options.hxx"

using namespace std;
using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;
using namespace yaal::tools::util;
using namespace ::huginn;

namespace huginn {

OSetup setup;

}

int main( int argc_, char* argv_[] ) {
	HScopeExitCall secTP( call( &HThreadPool::stop, &HThreadPool::get_instance() ) );
	HScopeExitCall secSS( call( &HSignalService::stop, &HSignalService::get_instance() ) );
	M_PROLOG
	int err( 0 );
	try {
		HSignalService::get_instance();
		setup._programName = argv_[ 0 ];
		int argc( handle_program_options( argc_, argv_ ) );
		if ( setup._generateLogs ) {
			hcore::log.rehash( setup._logPath, setup._programName );
		} else {
			HLog::disable_auto_rehash();
		}
		setup.test_setup();
		argc_ -= argc;
		argv_ += argc;
		if ( setup._interactive ) {
			err = ::huginn::interactive_session();
		} else if ( setup._jupyter ) {
			err = ::huginn::jupyter_session();
		} else if ( setup._hasProgram ) {
			err = ::huginn::oneliner( setup._program, argc_, argv_ );
		} else if ( ! setup._genDocs.is_empty() ) {
			err = ::huginn::gen_docs( argc_, argv_ );
		} else {
			err = ::huginn::main( argc_, argv_ );
		}
	} catch ( int e ) {
		err = e;
	}
#if defined( __MSVCXX__ ) || defined( __HOST_OS_TYPE_CYGWIN__ )
	typedef void ( * banner_t )( void );
	static __attribute__(( used )) banner_t const banner = &yaal::dbwrapper::banner;
#endif /* #if defined( __MSVCXX__ ) || defined( __HOST_OS_TYPE_CYGWIN__ ) */
#ifdef __MSVCXX__
	return ( banner ? err : 0 );
#else /* #ifdef __MSVCXX__ */
	return ( err );
#endif /* #else #ifdef __MSVCXX__ */
	M_FINAL
}

