/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <yaal/hcore/hcore.hxx>
#include <yaal/hcore/hfile.hxx>
#include <yaal/hcore/system.hxx>
#include <yaal/tools/ansi.hxx>
#include <yaal/tools/stringalgo.hxx>
#include <yaal/tools/filesystem.hxx>
#include <yaal/tools/huginn/integer.hxx>
#include <yaal/tools/huginn/packagefactory.hxx>
M_VCSID( "$Id: " __ID__ " $" )
M_VCSID( "$Id: " __TID__ " $" )
#include "shellscript.hxx"
#include "linerunner.hxx"
#include "meta.hxx"
#include "setup.hxx"
#include "colorize.hxx"
#include "symbolicnames.hxx"
#include "systemshell.hxx"
#include "forwardingshell.hxx"
#include "quotes.hxx"
#include "repl.hxx"
#include "settings.hxx"

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;
using namespace yaal::tools::huginn;

namespace huginn {

int run_huginn_shell_script( int argc_, char** argv_ ) {
	M_PROLOG
	HLineRunner lr( "*shell-script session*" );
	HRepl repl;
	shell_t shell(
		setup._shell->is_empty() ? shell_t( make_resource<HSystemShell>( lr, repl, argc_, argv_ ) ) : shell_t( make_resource<HForwardingShell>() )
	);
	HPointer<HFile> f;
	bool readFromScript( ( argc_ > 0 ) && ( argv_[0] != "-"_ys ) );
	hcore::HString scriptPath( "*standard input*" );
	if ( readFromScript ) {
		scriptPath.assign( argv_[0] );
		f = make_pointer<HFile>( scriptPath, HFile::OPEN::READING );
		if ( ! *f ) {
			throw HFileException( f->get_error() );
		}
	}
	HStreamInterface* source( readFromScript ? static_cast<HStreamInterface*>( f.raw() ) : &cin );
	int exitStatus( 0 );
	try {
		exitStatus = shell->run_script( *source, scriptPath );
	} catch ( ... ) {
		exitStatus = -1;
	}
	return exitStatus;
	M_EPILOG
}

}

