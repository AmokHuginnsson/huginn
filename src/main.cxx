/*
---            `huginn' 0.0.0 (c) 1978 by Marcin 'Amok' Konarski             ---

  main.cxx - this file is integral part of `huginn' project.

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
		if ( setup._interactive ) {
			err = ::huginn::interactive_session();
		} else if ( setup._jupyter ) {
			err = ::huginn::jupyter_session();
		} else if ( ! setup._program.is_empty() ) {
			err = ::huginn::oneliner( setup._program );
		} else if ( ! setup._genDocs.is_empty() ) {
			err = ::huginn::gen_docs( argc_ - argc, argv_ + argc );
		} else {
			err = ::huginn::main( argc_ - argc, argv_ + argc );
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

