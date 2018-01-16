/* Read huginn/LICENSE.md file for copyright and licensing information. */

#ifndef HUGINN_SETUP_HXX_INCLUDED
#define HUGINN_SETUP_HXX_INCLUDED 1

#include <libintl.h>
#include <yaal/hcore/hstring.hxx>
#include <yaal/tools/hhuginn.hxx>

#include "config.hxx"

namespace huginn {

enum class ERROR_CONTEXT {
	VISIBLE,
	HIDDEN,
	SHORT
};

enum class BACKGROUND {
	DARK,
	LIGHT
};

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
	bool _hasProgram;
	BACKGROUND _background;
	ERROR_CONTEXT _errorContext;
	yaal::hcore::HString _session;
	yaal::hcore::HString _sessionDir;
	yaal::hcore::HString _program;
	yaal::tools::HHuginn::paths_t _modulePath;
	yaal::hcore::HString _historyPath;
	yaal::hcore::HString _genDocs;
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
		, _hasProgram( false )
		, _background( BACKGROUND::DARK )
		, _errorContext( ERROR_CONTEXT::SHORT )
		, _session()
		, _sessionDir()
		, _program()
		, _modulePath()
		, _historyPath()
		, _genDocs()
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

