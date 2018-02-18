/* Read huginn/LICENSE.md file for copyright and licensing information. */

#ifndef HUGINN_SETUP_HXX_INCLUDED
#define HUGINN_SETUP_HXX_INCLUDED 1

#include <libintl.h>
#include <yaal/hcore/hstring.hxx>
#include <yaal/tools/util.hxx>
#include <yaal/tools/hhuginn.hxx>

#include "config.hxx"

namespace huginn {

enum class ERROR_CONTEXT {
	VISIBLE,
	HIDDEN,
	SHORT
};

struct OSetup {
	typedef yaal::tools::HOptional<yaal::hcore::HString> string_opt_t;
	bool _quiet;
	bool _verbose;
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
	bool _streamEditor;
	bool _streamEditorSilent;
	bool _chomp;
	bool _autoSplit;
	ERROR_CONTEXT _errorContext;
	string_opt_t _inplace;
	string_opt_t _program;
	string_opt_t _shell;
	yaal::hcore::HString _colorScheme;
	yaal::hcore::HString _fieldSeparator;
	yaal::hcore::HString _session;
	yaal::hcore::HString _sessionDir;
	yaal::tools::HHuginn::paths_t _modulePath;
	yaal::hcore::HString _historyPath;
	yaal::hcore::HString _genDocs;
	char* _programName;
	string_opt_t _logPath;
	/* self-sufficient */
	OSetup( void )
		: _quiet( false )
		, _verbose( false )
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
		, _streamEditor( false )
		, _streamEditorSilent( false )
		, _chomp( false )
		, _autoSplit( false )
		, _errorContext( ERROR_CONTEXT::SHORT )
		, _inplace()
		, _program()
		, _shell()
		, _colorScheme()
		, _fieldSeparator( " " )
		, _session()
		, _sessionDir()
		, _modulePath()
		, _historyPath()
		, _genDocs()
		, _programName( NULL )
		, _logPath() {
		return;
	}
	void test_setup( int );
private:
	OSetup( OSetup const& );
	OSetup& operator = ( OSetup const& );
};

extern OSetup setup;

}

#endif /* #ifndef HUGINN_SETUP_HXX_INCLUDED */

