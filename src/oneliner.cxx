/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <yaal/tools/hhuginn.hxx>
#include <yaal/tools/hstringstream.hxx>
#include <yaal/hcore/hfile.hxx>
#include <yaal/tools/huginn/integer.hxx>
M_VCSID( "$Id: " __ID__ " $" )
M_VCSID( "$Id: " __TID__ " $" )
#include "oneliner.hxx"
#include "setup.hxx"
#include "colorize.hxx"

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::random;
using namespace yaal::tools;
using namespace yaal::tools::huginn;

namespace huginn {

namespace {
void import( yaal::hcore::HStreamInterface& source_, yaal::hcore::HString const& package_, yaal::hcore::HString const& alias_ ) {
	if ( setup._aliasImports ) {
		source_ << "import " << package_ << " as " << alias_ << ";\n";
	} else {
		source_ << "from " << package_ << " import *;\n";
	}
}
}

int oneliner( yaal::hcore::HString const& program_, int argc_, char** argv_ ) {
	M_PROLOG
	HHuginn::disable_grammar_verification();

	static HHuginn grammarSource;
	static executing_parser::HRule grammar( grammarSource.make_engine() );
	static executing_parser::HRuleBase const* expressionRule( grammar.find( "expression" ) );
	static HExecutingParser expressionParser( *expressionRule, HExecutingParser::INIT_MODE::TRUST_GRAMMAR );

	hcore::HString program( program_ );

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
	hcore::HString code;

	if ( ! setup._noDefaultImports || ( setup._streamEditor && ( argc_ > 0 ) ) ) {
		import( ss, "FileSystem", "fs" );
	}
	if ( ! setup._noDefaultImports || setup._autoSplit ) {
		import( ss, "Text", "text" );
		util::escape( setup._fieldSeparator, executing_parser::_escapes_ );
	}
	if ( ! setup._noDefaultImports ) {
		import( ss, "Mathematics", "math" );
		import( ss, "Algorithms", "algo" );
		import( ss, "Operators", "op" );
		import( ss, "Introspection", "intro" );
		import( ss, "RegularExpressions", "re" );
		import( ss, "DateTime", "dt" );
		import( ss, "OperatingSystem", "os" );
		import( ss, "Cryptography", "crypto" );
		import( ss, "Network", "net" );
		import( ss, "Database", "db" );
		import( ss, "XML", "xml" );
		import( ss, "Terminal", "term" );
		ss << "\n";
	}
	if ( argc_ > 0 ) {
		ss << "main( argv_ ) {\n\t";
	} else {
		ss << "main() {\n\t";
	}
	char const* indent = ( setup._streamEditor && ( argc_ > 0 ) ) ? "\t\t\t" : "\t\t";
	distribution::HDiscrete rnd( rng_helper::make_random_number_generator() );
	hcore::HString tmpExt( static_cast<int long long>( rnd() ) );
	hcore::HString fs( setup._aliasImports ? "fs." : "" );
	hcore::HString text( setup._aliasImports ? "text." : "" );
	if ( setup._streamEditor ) {
		if ( argc_ > 0 ) {
			ss << "for ( __arg__ : argv_ ) {\n\t\t__in__ = " << fs << "open( __arg__, " << fs << "OPEN_MODE.READ );\n\t\t";
			if ( setup._inplace ) {
				ss
					<< "__outName__ = \"{}-{}\".format( __arg__, " << tmpExt << " );\n\t\t"
					<< "__out__ = " << fs << "open( __outName__, " << fs << "OPEN_MODE.WRITE );\n\t\t"
					<< fs << "chmod( __outName__, " << fs << "stat( __arg__ ).mode() );\n\t\t";
			}
			ss << "__ = 0;\n\t\twhile ( ( _ = __in__.read_line() ) != none ) {\n\t\t\t__ += 1;\n\t\t\t";
		} else {
			ss << "__ = 0;\n\twhile ( ( _ = input() ) != none ) {\n\t\t__ += 1;\n\t\t";
		}
		if ( setup._chomp ) {
			ss << "_ = _.strip_right( \"\\r\\n\" );\n" << indent;
		}
		if ( setup._autoSplit ) {
			ss << "F = " << text << "split( _, \"" << setup._fieldSeparator << "\" );\n" << indent;
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
		if ( ! setup._noColor ) {
			cerr << colorize( code ) << colorize_error( h.error_message() ) << endl;
		} else {
			cerr << code << h.error_message() << endl;
		}
	} else if ( ! setup._streamEditor ) {
		HHuginn::value_t result( h.result() );
		cout << to_string( result, &h ) << endl;
		if ( result->type_id() == HHuginn::TYPE::INTEGER ) {
			retVal = static_cast<int>( static_cast<HInteger*>( result.raw() )->value() );
		}
	}
	return ( retVal );
	M_EPILOG
}

}

