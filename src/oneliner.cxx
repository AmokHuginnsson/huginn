/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <yaal/tools/hhuginn.hxx>
#include <yaal/tools/hstringstream.hxx>
#include <yaal/hcore/hfile.hxx>
M_VCSID( "$Id: " __ID__ " $" )
M_VCSID( "$Id: " __TID__ " $" )
#include "oneliner.hxx"
#include "setup.hxx"

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;
using namespace yaal::tools::huginn;

namespace huginn {

int oneliner( yaal::hcore::HString const& program_ ) {
	M_PROLOG
	HHuginn::disable_grammar_verification();
	HStringStream ss;
	HString code;
	HString program( program_ );
	program.trim_right( character_class( CHARACTER_CLASS::WHITESPACE ).data() );
	while ( ! program.is_empty() && ( program.back() == ';' ) ) {
		program.pop_back();
		program.trim_right( character_class( CHARACTER_CLASS::WHITESPACE ).data() );
	}
	if ( ! setup._noDefaultImports ) {
		ss <<
			"import Mathematics as math;\n"
			"import Algorithms as algo;\n"
			"import Text as text;\n"
			"import RegularExpressions as re;\n"
			"import DateTime as dt;\n"
			"import OperatingSystem as os;\n"
			"import FileSystem as fs;\n"
			"import Cryptography as crypto;\n"
			"import Network as net;\n"
			"import Database as db;\n"
			"\n";
	}
	ss << "main() {\n\t" << program << ( ! program.is_empty() && ( program.back() != '}' ) ? ";" : "" ) << "\n}\n";
	code = ss.string();
	HHuginn h;
	h.load( ss );
	h.preprocess();
	int retVal( 0 );
	bool ok( h.parse() && h.compile( HHuginn::COMPILER::BE_SLOPPY ) && h.execute() );
	if ( ok ) {
		HHuginn::value_t result( h.result() );
		cout << to_string( result, &h ) << endl;
		if ( result->type_id() == HHuginn::TYPE::INTEGER ) {
			retVal = static_cast<int>( static_cast<HHuginn::HInteger*>( result.raw() )->value() );
		}
	} else {
		cerr << code << h.error_message() << endl;
	}
	return ( retVal );
	M_EPILOG
}

}

