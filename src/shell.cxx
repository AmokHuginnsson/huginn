/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <cstdlib>
#include <yaal/hcore/hcore.hxx>
#include <yaal/hcore/hfile.hxx>
#include <yaal/hcore/hrawfile.hxx>
#include <yaal/tools/hpipedchild.hxx>
#include <yaal/tools/hfsitem.hxx>
#include <yaal/tools/hhuginn.hxx>
#include <yaal/tools/filesystem.hxx>

M_VCSID( "$Id: " __ID__ " $" )
M_VCSID( "$Id: " __TID__ " $" )

#include "shell.hxx"
#include "quotes.hxx"
#include "setup.hxx"

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;
using namespace yaal::tools::string;

namespace huginn {

namespace {
void unescape( HString& str_ ) {
	util::unescape( str_, executing_parser::_escapes_ );
	str_.pop_back();
	str_.shift_left( 1 );
	return;
}
}

system_commands_t get_system_commands( void ) {
	M_PROLOG
	system_commands_t sc;
	char const* PATH_ENV( ::getenv( "PATH" ) );
	do {
		if ( ! PATH_ENV ) {
			break;
		}
		tokens_t paths( split<>( PATH_ENV, ":" ) );
		reverse( paths.begin(), paths.end() );
		for ( yaal::hcore::HString const& p : paths ) {
			HFSItem dir( p );
			if ( ! dir ) {
				continue;
			}
			for ( HFSItem const& file : dir ) {
				sc[file.get_name()] = p;
			}
		}
	} while ( false );
	return ( sc );
	M_EPILOG
}

bool shell( yaal::hcore::HString const& line_, HLineRunner& lr_, system_commands_t const& sc_ ) {
	M_PROLOG
	HUTF8String utf8( line_ );
	HPipedChild pc;
	tokens_t tokens( split_quotes( line_ ) );
	HString line;
	HStreamInterface* out( &cout );
	HFile redirOut;
	bool ok( false );
	try {
		for ( tokens_t::iterator it( tokens.begin() ); it != tokens.end(); ++ it ) {
			HString raw( *it );
			for ( HIntrospecteeInterface::HVariableView const& vv : lr_.locals() ) {
				if ( vv.name() == *it ) {
					if ( setup._shell->is_empty() ) {
						it->assign( to_string( vv.value(), lr_.huginn() ) );
					} else {
						it->assign( "'" ).append( to_string( vv.value(), lr_.huginn() ) ).append( "'" );
					}
					break;
				}
			}
			bool inDoubleQuotes( ( it->front() == '"' ) && ( it->back() == '"' ) );
			bool inSingleQuotes( ( it->front() == '\'' ) && ( it->back() == '\'' ) );
			if ( ( it->get_size() >= 2 ) && ( inDoubleQuotes || inSingleQuotes ) ) {
				unescape( *it );
			}
			bool append( raw == ">>" );
			if ( append || ( raw == ">" ) ) {
				tokens.erase( it );
				if ( it != tokens.end() ) {
					if ( redirOut.is_opened() ) {
						redirOut.close();
					}
					redirOut.open( *it, append ? HFile::OPEN::WRITING | HFile::OPEN::APPEND : HFile::OPEN::WRITING );
					out = &redirOut;
					tokens.erase( it );
				} else {
					ok = ( sc_.count( tokens.front() ) > 0 );
					throw HRuntimeException( "Missing name or redirect." );
				}
				-- it;
				continue;
			}
			if ( ! inSingleQuotes ) {
				substitute_environment( *it, ENV_SUBST_MODE::RECURSIVE );
			}
		}
		if ( setup._shell->is_empty() ) {
			HString image( tokens.front() );
			system_commands_t::const_iterator it( sc_.find( image ) );
			if ( it != sc_.end() ) {
				image = it->second + filesystem::path::SEPARATOR + image;
			}
			tokens.erase( tokens.begin() );
			pc.spawn( image, tokens );
		} else {
			pc.spawn( *setup._shell, { "-c", join( tokens, " " ) } );
		}
		static int const BUF_SIZE( 4096 );
		char buf[BUF_SIZE];
		int long nRead( 0 );
		pc.close_in();
		ok = true;
		while ( ( nRead = pc.out().read( buf, BUF_SIZE ) ) > 0 ) {
			out->write( buf, nRead );
		}
		while ( ( nRead = pc.err().read( buf, BUF_SIZE ) ) > 0 ) {
			cerr.write( buf, nRead );
		}
		HPipedChild::STATUS s( pc.finish() );
		if ( s.type != HPipedChild::STATUS::TYPE::NORMAL ) {
			cerr << "Abort " << s.value << endl;
		} else if ( s.value != 0 ) {
			cout << "Exit " << s.value << endl;
		}
	} catch ( HException const& e ) {
		cerr << e.what() << endl;
	}
	return ( ok );
	M_EPILOG
}

}

