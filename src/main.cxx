/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <cstdlib>

#include <yaal/hcore/hlog.hxx>
#include <yaal/tools/signals.hxx>
#include <yaal/tools/util.hxx>
#include <yaal/tools/hthreadpool.hxx>
#include <yaal/tools/hterminal.hxx>
#if defined( __MSVCXX__ ) || defined( __HOST_OS_TYPE_CYGWIN__ )
#include <yaal/dbwrapper/dbwrapper.hxx>
#endif /* #if defined( __MSVCXX__ ) || defined( __HOST_OS_TYPE_CYGWIN__ ) */
M_VCSID( "$Id: " __ID__ " $" )
#include "huginn.hxx"
#include "interactive.hxx"
#include "jupyter.hxx"
#include "oneliner.hxx"
#include "gendocs.hxx"
#include "tags.hxx"
#include "shellscript.hxx"

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
		if ( setup._logPath ) {
			hcore::log.rehash( *setup._logPath, setup._programName );
		} else {
			HLog::disable_auto_rehash();
		}
		argc_ -= argc;
		argv_ += argc;
		setup.test_setup( argc_ );
		if ( setup._jupyter ) {
			err = ::huginn::jupyter_session();
		} else if ( setup._program ) {
			err = ( !! setup._shell ? ::huginn::oneliner_shell : ::huginn::oneliner )( *setup._program, argc_, argv_ );
		} else if ( ! setup._genDocs.is_empty() ) {
			err = ::huginn::gen_docs( argc_, argv_ );
		} else if ( ( argc_ == 0 ) && is_a_tty( cin ) && is_a_tty( cout ) ) {
			err = ::huginn::interactive_session();
		} else if ( setup._tags ) {
			err = ::huginn::tags( argv_[0] );
		} else if ( !! setup._shell ) {
			err = ::huginn::run_huginn_shell_script( argc_, argv_ );
		} else {
			err = ::huginn::run_huginn( argc_, argv_ );
		}
	} catch ( int e ) {
		err = e;
	}
#if defined( __MSVCXX__ ) || defined( __HOST_OS_TYPE_CYGWIN__ )
	typedef void ( * banner_t )( void );
	static __attribute__(( used )) banner_t const volatile banner = &yaal::dbwrapper::banner;
#endif /* #if defined( __MSVCXX__ ) || defined( __HOST_OS_TYPE_CYGWIN__ ) */
#ifdef __MSVCXX__
	return ( banner ? err : 0 );
#else /* #ifdef __MSVCXX__ */
	return ( err );
#endif /* #else #ifdef __MSVCXX__ */
	M_FINAL
}

