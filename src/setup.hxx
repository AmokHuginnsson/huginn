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
	static int const CENTURY_IN_SECONDS;
	typedef yaal::tools::HOptional<yaal::hcore::HString> string_opt_t;
	bool _quiet;
	bool _verbose;
	bool _dumpState;
	bool _embedded;
	bool _lint;
	bool _tags;
	bool _nativeLines;
	bool _rapidStart;
	bool _interactive;
	bool _jupyter;
	bool _noDefaultImports;
	bool _noArgv;
	bool _beSloppy;
	bool _optimize;
	bool _noColor;
	bool _streamEditor;
	bool _streamEditorSilent;
	bool _chomp;
	bool _autoSplit;
	bool _aliasImports;
	ERROR_CONTEXT _errorContext;
	string_opt_t _inplace;
	string_opt_t _program;
	string_opt_t _shell;
	yaal::hcore::HString _prompt;
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
	OSetup( void );
	void test_setup( int );
	yaal::hcore::HString default_prompt( void ) const;
private:
	OSetup( OSetup const& );
	OSetup& operator = ( OSetup const& );
};

extern OSetup setup;

}

#endif /* #ifndef HUGINN_SETUP_HXX_INCLUDED */

