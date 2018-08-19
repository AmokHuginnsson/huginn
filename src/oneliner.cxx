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

int oneliner( yaal::hcore::HString const& program_, int argc_, char** argv_ ) {
	M_PROLOG
	HHuginn::disable_grammar_verification();

	static HHuginn grammarSource;
	static executing_parser::HRule grammar( grammarSource.make_engine() );
	static executing_parser::HRuleBase const* expressionRule( grammar.find( "expression" ) );
	static HExecutingParser expressionParser( *expressionRule, HExecutingParser::INIT_MODE::TRUST_GRAMMAR );

	HString program( program_ );

	HHuginn preprocessor;
	HStringStream src( program );
	preprocessor.load( src );
	preprocessor.preprocess();
	preprocessor.dump_preprocessed_source( src );
	program.assign( src.string() );

	program.trim_right( character_class<CHARACTER_CLASS::WHITESPACE>().data() );
	while ( ! program.is_empty() && ( program.back() == ';' ) ) {
		program.pop_back();
		program.trim_right( character_class<CHARACTER_CLASS::WHITESPACE>().data() );
	}

	bool isExpression( expressionParser( program ) );

	HStringStream ss;
	HString code;

	if ( ! setup._noDefaultImports || ( setup._streamEditor && ( argc_ > 0 ) ) ) {
		ss << "import FileSystem as fs;\n";
	}
	if ( ! setup._noDefaultImports || setup._autoSplit ) {
		ss << "import Text as text;\n";
		util::escape( setup._fieldSeparator, executing_parser::_escapes_ );
	}
	if ( ! setup._noDefaultImports ) {
		ss <<
			"import Mathematics as math;\n"
			"import Algorithms as algo;\n"
			"import Introspection as intro;\n"
			"import RegularExpressions as re;\n"
			"import DateTime as dt;\n"
			"import OperatingSystem as os;\n"
			"import Cryptography as crypto;\n"
			"import Network as net;\n"
			"import Database as db;\n"
			"import XML as xml;\n"
			"\n";
	}
	if ( argc_ > 0 ) {
		ss << "main( argv_ ) {\n\t";
	} else {
		ss << "main() {\n\t";
	}
	char const* indent = ( setup._streamEditor && ( argc_ > 0 ) ) ? "\t\t\t" : "\t\t";
	HRandomizer rnd( randomizer_helper::make_randomizer() );
	HString tmpExt( static_cast<int long long>( rnd() ) );
	if ( setup._streamEditor ) {
		if ( argc_ > 0 ) {
			ss << "for ( __arg__ : argv_ ) {\n\t\t__in__ = fs.open( __arg__, fs.OPEN_MODE.READ );\n\t\t";
			if ( setup._inplace ) {
				ss
					<< "__outName__ = \"{}-{}\".format( __arg__, " << tmpExt << " );\n\t\t"
					<< "__out__ = fs.open( __outName__, fs.OPEN_MODE.WRITE );\n\t\t"
					<< "fs.chmod( __outName__, fs.stat( __arg__ ).mode() );\n\t\t";
			}
			ss << "__ = 0;\n\t\twhile ( ( _ = __in__.read_line() ) != none ) {\n\t\t\t__ += 1;\n\t\t\t";
		} else {
			ss << "__ = 0;\n\twhile ( ( _ = input() ) != none ) {\n\t\t__ += 1;\n\t\t";
		}
		if ( setup._chomp ) {
			ss << "_ = _.strip_right( \"\\r\\n\" );\n" << indent;
		}
		if ( setup._autoSplit ) {
			ss << "F = text.split( _, \"" << setup._fieldSeparator << "\" );\n" << indent;
		}
		if ( isExpression ) {
			ss << "_ = ";
		}
	}
	ss << program;
	if ( ! program.is_empty() && ( program.back() != '}' ) ) {
		ss << ";";
	}
	if ( setup._streamEditor ) {
		if ( ! setup._quiet ) {
			if ( setup._inplace ) {
				ss << "\n" << indent << "__out__.write( \"{}\\n\".format( _ ) );";
			} else {
				ss << "\n" << indent << "print( \"{}\\n\".format( _ ) );";
			}
		}
		if ( argc_ > 0 ) {
			ss << "\n\t\t}";
			if ( setup._inplace ) {
				if ( ! setup._inplace->is_empty() ) {
					ss << "\n\t\tfs.rename( __arg__, __arg__ + \"" << *setup._inplace << "\" );";
				}
				ss << "\n\t\tfs.rename( __outName__, __arg__ );";
			}
		}
		ss << "\n\t}";
	}
	ss << "\n}\n";
	code = ss.string();
	HHuginn h;

	for ( int i( 0 ); i < argc_; ++ i ) {
		h.add_argument( argv_[i] );
	}
	h.load( ss );
	h.preprocess();
	int retVal( 0 );
	bool ok( h.parse() && h.compile( HHuginn::COMPILER::BE_SLOPPY ) && h.execute() );
	if ( ! ok) {
		cerr << code << h.error_message() << endl;
	} else if ( ! setup._streamEditor ) {
		HHuginn::value_t result( h.result() );
		cout << to_string( result, &h ) << endl;
		if ( result->type_id() == HHuginn::TYPE::INTEGER ) {
			retVal = static_cast<int>( static_cast<HHuginn::HInteger*>( result.raw() )->value() );
		}
	}
	return ( retVal );
	M_EPILOG
}

}

