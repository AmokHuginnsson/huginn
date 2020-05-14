/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <yaal/hcore/hlog.hxx>
#include <yaal/hcore/hclock.hxx>
#include <yaal/tools/hhuginn.hxx>
#include <yaal/tools/stringalgo.hxx>
#include <yaal/tools/filesystem.hxx>
#include <yaal/tools/huginn/runtime.hxx>
#include <yaal/tools/huginn/thread.hxx>
#include <yaal/tools/huginn/objectfactory.hxx>
#include <yaal/tools/huginn/helper.hxx>
#include <yaal/tools/huginn/integer.hxx>

M_VCSID( "$Id: " __ID__ " $" )
#include "huginn.hxx"
#include "repl.hxx"
#include "settings.hxx"
#include "colorize.hxx"
#include "timeit.hxx"
#include "setup.hxx"

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;
using namespace yaal::tools::filesystem;
using namespace yaal::tools::huginn;

namespace huginn {

namespace {

HHuginn::value_t repl( HRepl* repl_, tools::huginn::HThread* thread_, HHuginn::value_t*, HHuginn::values_t& values_, int position_ ) {
	M_PROLOG
	char const name[] = "repl";
	verify_arg_count( name, values_, 0, 1, thread_, position_ );
	HUTF8String utf8;
	if ( values_.get_size() > 0 ) {
		verify_arg_type( name, values_, 0, HHuginn::TYPE::STRING, ARITY::UNARY, thread_, position_ );
		utf8.assign( get_string( values_[0] ) );
	}
	yaal::hcore::HString l;
	return (
		repl_->input( l, utf8.c_str() )
			? thread_->object_factory().create_string( yaal::move( l ) )
			: thread_->runtime().none_value()
	);
	M_EPILOG
}

}

int run_huginn( int argc_, char** argv_ ) {
	M_PROLOG
	if ( setup._rapidStart ) {
		HHuginn::disable_grammar_verification();
	}
	HClock c;
	HHuginn h( setup._optimize ? HHuginn::COMPILER::OPTIMIZE : HHuginn::COMPILER::DEFAULT );
	HRepl rpl;
	h.register_function( "repl", call( &repl, &rpl, _1, _2, _3, _4 ), "( [*prompt*] ) - read line of user input potentially prefixing it with *prompt*" );
	time::duration_t huginn( c.get_time_elapsed( time::UNIT::NANOSECOND ) );
	HPointer<HFile> f;
	bool readFromScript( ( argc_ > 0 ) && ( argv_[0] != "-"_ys ) );
	if ( readFromScript ) {
		hcore::HString scriptPath( argv_[0] );
		f = make_pointer<HFile>( scriptPath, HFile::OPEN::READING );
		if ( ! *f ) {
			if ( is_relative( scriptPath ) ) {
				scriptPath.append( ".hgn" );
				HHuginn::paths_t paths( HHuginn::MODULE_PATHS );
				paths.insert( paths.end(), setup._modulePath.begin(), setup._modulePath.end() );
				paths.push_back( setup._sessionDir );
				for ( hcore::HString path : paths ) {
					path.append( path::SEPARATOR ).append( scriptPath );
					f = make_pointer<HFile>( path, HFile::OPEN::READING );
					if ( !! *f ) {
						break;
					}
				}
				if ( ! *f ) {
					throw HFileException( "Huginn module : '"_ys.append( argv_[0] ).append( "' was not found." ) );
				}
			} else {
				throw HFileException( f->get_error() );
			}
		}
	}
	HStreamInterface* source( readFromScript ? static_cast<HStreamInterface*>( f.raw() ) : &cin );

	if ( ! setup._noArgv ) {
		for ( int i( 0 ); i < argc_; ++ i ) {
			h.add_argument( argv_[i] );
		}
	}
	c.reset();
	int lineSkip( 0 );
	if ( setup._embedded ) {
		hcore::HString line;
#define LANG_NAME "huginn"
		HRegex r( "^#!.*\\b" LANG_NAME "\\b.*" );
		while ( source->read_until( line ) > 0 ) {
			++ lineSkip;
			if ( r.matches( line ) ) {
				break;
			}
		}
		int long settingPos( line.find( LANG_NAME ) );
		if ( settingPos != hcore::HString::npos ) {
			settingPos += static_cast<int>( sizeof ( LANG_NAME ) );
			tools::string::tokens_t settings(
				tools::string::split<tools::string::tokens_t>(
					line.mid( settingPos ),
					character_class<CHARACTER_CLASS::WHITESPACE>().data(),
					HTokenizer::SKIP_EMPTY | HTokenizer::DELIMITED_BY_ANY_OF
				)
			);
			for ( hcore::HString const& s : settings ) {
				apply_setting( h, s );
			}
		}
#undef LANG_NAME

	}
	h.load( *source, setup._nativeLines ? 0 : lineSkip );
	time::duration_t load( c.get_time_elapsed( time::UNIT::NANOSECOND ) );
	c.reset();
	h.preprocess();
	time::duration_t preprocess( c.get_time_elapsed( time::UNIT::NANOSECOND ) );
	c.reset();
	bool ok( false );
	int retVal( 0 );
	do {
		if ( ! h.parse() ) {
			retVal = 1;
			break;
		}
		time::duration_t parse( c.get_time_elapsed( time::UNIT::NANOSECOND ) );
		c.reset();
		HHuginn::compiler_setup_t errorHandling( setup._beSloppy ? HHuginn::COMPILER::BE_SLOPPY : HHuginn::COMPILER::BE_STRICT );
		if ( ! h.compile( setup._modulePath, errorHandling ) ) {
			retVal = 2;
			break;
		}
		time::duration_t compile( c.get_time_elapsed( time::UNIT::NANOSECOND ) );
		time::duration_t execute( 0 );
		time::duration_t preciseTime( 0 );
		int runs( 0 );
		if ( ! setup._lint ) {
			c.reset();
			ok = timeit( h, preciseTime, runs );
			execute = time::duration_t( c.get_time_elapsed( time::UNIT::NANOSECOND ) );
			if ( ! ok ) {
				if ( setup._verbose ) {
					dump_call_stack( h.trace(), cerr );
				}
				retVal = 3;
				break;
			}
		}
		if ( setup._dumpState ) {
			h.dump_vm_state( hcore::log );
		}
		if ( setup._logPath ) {
			hcore::log( LOG_LEVEL::NOTICE )
				<< "Execution stats: huginn(" << lexical_cast<hcore::HString>( huginn )
				<< "), load(" << lexical_cast<hcore::HString>( load )
				<< "), preprocess(" << lexical_cast<hcore::HString>( preprocess )
				<< "), parse(" << lexical_cast<hcore::HString>( parse )
				<< "), compile(" << lexical_cast<hcore::HString>( compile )
				<< "), execute(" << lexical_cast<hcore::HString>( execute ) << ")" << endl;
		}
		report_timeit( huginn, load, preprocess, parse, compile, execute, preciseTime, runs );
		if ( ! setup._lint ) {
			HHuginn::value_t result( h.result() );
			if ( result->type_id() == HHuginn::TYPE::INTEGER ) {
				retVal = static_cast<int>( static_cast<tools::huginn::HInteger*>( result.raw() )->value() );
			}
		}
		ok = true;
	} while ( false );
	if ( ! ok ) {
		if ( ! setup._noColor ) {
			cerr << colorize_error( h.error_message() ) << endl;
		} else {
			cerr << h.error_message() << endl;
		}
	}
	return ( retVal );
	M_EPILOG
}


}

