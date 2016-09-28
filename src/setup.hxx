/*
---            `huginn' 0.0.0 (c) 1978 by Marcin 'Amok' Konarski             ---

	setup.hxx - this file is integral part of `huginn' project.

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

#ifndef HUGINN_SETUP_HXX_INCLUDED
#define HUGINN_SETUP_HXX_INCLUDED 1

#include <libintl.h>
#include <yaal/hcore/hstring.hxx>
#include <yaal/tools/hhuginn.hxx>

#include "config.hxx"

namespace huginn {

struct OSetup {
	bool _quiet;
	bool _verbose;
	bool _generateLogs;
	bool _dumpState;
	bool _embedded;
	bool _lint;
	bool _nativeLines;
	bool _rapidStart;
	bool _interactive;
	bool _jupyter;
	bool _noDefaultImports;
	bool _noArgv;
	bool _beSloppy;
	bool _noColor;
	yaal::hcore::HString _program;
	yaal::tools::HHuginn::paths_t _modulePath;
	yaal::hcore::HString _historyPath;
	char* _programName;
	yaal::hcore::HString _logPath;
	/* self-sufficient */
	OSetup( void )
		: _quiet( false )
		, _verbose( false )
		, _generateLogs( false )
		, _dumpState( false )
		, _embedded( false )
		, _lint( false )
		, _nativeLines( false )
		, _rapidStart( false )
		, _interactive( false )
		, _jupyter( false )
		, _noDefaultImports( false )
		, _noArgv( false )
		, _beSloppy( false )
		, _noColor( false )
		, _program()
		, _modulePath()
		, _historyPath()
		, _programName( NULL )
		, _logPath() {
		return;
	}
	void test_setup( void );
private:
	OSetup( OSetup const& );
	OSetup& operator = ( OSetup const& );
};

extern OSetup setup;

}

#endif /* #ifndef HUGINN_SETUP_HXX_INCLUDED */

