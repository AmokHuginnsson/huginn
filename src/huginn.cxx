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

M_VCSID( "$Id: " __ID__ " $" )
#include "huginn.hxx"
#include "repl.hxx"
#include "settings.hxx"
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
	return ( repl_->input( l, utf8.c_str() ) ? thread_->object_factory().create_string( l ) : thread_->runtime().none_value() );
	M_EPILOG
}
}

int main( int argc_, char** argv_ ) {
	M_PROLOG
	if ( setup._rapidStart ) {
		HHuginn::disable_grammar_verification();
	}
	HClock c;
	HHuginn h;
	HRepl rpl;
	h.register_function( "repl", call( &repl, &rpl, _1, _2, _3, _4 ), "( [*prompt*] ) - read line of user input potentially prefixing it with *prompt*" );
	i64_t huginn( c.get_time_elapsed( time::UNIT::MILLISECOND ) );
	HPointer<HFile> f;
	bool readFromScript( ( argc_ > 0 ) && ( argv_[0] != "-"_ys ) );
	if ( readFromScript ) {
		HString scriptPath( argv_[0] );
		f = make_pointer<HFile>( scriptPath, HFile::OPEN::READING );
		if ( ! *f ) {
			if ( is_relative( scriptPath ) ) {
				scriptPath.append( ".hgn" );
				HHuginn::paths_t paths( HHuginn::MODULE_PATHS );
				paths.insert( paths.end(), setup._modulePath.begin(), setup._modulePath.end() );
				paths.push_back( setup._sessionDir );
				for ( HString path : paths ) {
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
		HString line;
#define LANG_NAME "huginn"
		HRegex r( "^#!.*\\b" LANG_NAME "\\b.*" );
		while ( source->read_until( line ) > 0 ) {
			++ lineSkip;
			if ( r.matches( line ) ) {
				break;
			}
		}
		typedef yaal::hcore::HArray<HString> tokens_t;
		int long settingPos( line.find( LANG_NAME ) );
		if ( settingPos != HString::npos ) {
			settingPos += static_cast<int>( sizeof ( LANG_NAME ) );
			tokens_t settings(
				string::split<tokens_t>(
					line.mid( settingPos ),
					character_class( CHARACTER_CLASS::WHITESPACE ).data(),
					HTokenizer::SKIP_EMPTY | HTokenizer::DELIMITED_BY_ANY_OF
				)
			);
			for ( HString const& s : settings ) {
				apply_setting( h, s );
			}
		}
#undef LANG_NAME

	}
	h.load( *source, setup._nativeLines ? 0 : lineSkip );
	i64_t load( c.get_time_elapsed( time::UNIT::MILLISECOND ) );
	c.reset();
	h.preprocess();
	i64_t preprocess( c.get_time_elapsed( time::UNIT::MILLISECOND ) );
	c.reset();
	bool ok( false );
	int retVal( 0 );
	do {
		if ( ! h.parse() ) {
			retVal = 1;
			break;
		}
		i64_t parse( c.get_time_elapsed( time::UNIT::MILLISECOND ) );
		c.reset();
		HHuginn::compiler_setup_t errorHandling( setup._beSloppy ? HHuginn::COMPILER::BE_SLOPPY : HHuginn::COMPILER::BE_STRICT );
		if ( ! h.compile( setup._modulePath, errorHandling ) ) {
			retVal = 2;
			break;
		}
		i64_t compile( c.get_time_elapsed( time::UNIT::MILLISECOND ) );
		c.reset();
		if ( ! setup._lint ) {
			if ( ! h.execute() ) {
				retVal = 3;
				break;
			}
		}
		if ( setup._dumpState ) {
			h.dump_vm_state( hcore::log );
		}
		i64_t execute( c.get_time_elapsed( time::UNIT::MILLISECOND ) );
		if ( setup._logPath ) {
			hcore::log( LOG_LEVEL::NOTICE )
				<< "Execution stats: huginn(" << huginn
				<< "), load(" << load
				<< "), preprocess(" << preprocess
				<< "), parse(" << parse
				<< "), compile(" << compile
				<< "), execute(" << execute << ")" << endl;
		}
		if ( ! setup._lint ) {
			HHuginn::value_t result( h.result() );
			if ( result->type_id() == HHuginn::TYPE::INTEGER ) {
				retVal = static_cast<int>( static_cast<HHuginn::HInteger*>( result.raw() )->value() );
			}
		}
		ok = true;
	} while ( false );
	if ( ! ok ) {
		cerr << h.error_message() << endl;
	}
	return ( retVal );
	M_EPILOG
}


}

