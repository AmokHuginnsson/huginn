/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <cstdlib>
#include <yaal/hcore/hcore.hxx>
#include <yaal/hcore/hfile.hxx>
#include <yaal/hcore/hrawfile.hxx>
#include <yaal/hcore/duration.hxx>
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

#ifndef __MSVCXX__
code_point_t PATH_SEP = filesystem::path::SEPARATOR;
char const PATH_ENV_SEP[] = ":";
#else
code_point_t PATH_SEP = '\\'_ycp;
char const PATH_ENV_SEP[] = ";";
#endif

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
		tokens_t paths( split<>( PATH_ENV, PATH_ENV_SEP ) );
		reverse( paths.begin(), paths.end() );
		for ( yaal::hcore::HString const& p : paths ) {
			HFSItem dir( p );
			if ( ! dir ) {
				continue;
			}
			for ( HFSItem const& file : dir ) {
				HString name( file.get_name() );
#ifndef __MSVCXX__
				if ( ! file.is_executable() ) {
					continue;
				}
#else
				name.lower();
				HString ext( name.right( 4 ) );
				if ( ( ext != ".exe" ) && ( ext != ".com" ) && ( ext != ".cmd" ) && ( ext != ".bat" ) ) {
					continue;
				}
				name.erase( name.get_size() - 4 );
#endif
				sc[name] = p;
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
	HStreamInterface* in( &cin );
	HStreamInterface* out( &cout );
	HFile fileIn;
	HFile fileOut;
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
			bool redirOut( raw == ">" );
			bool redirIn( raw == "<" );
			if ( redirIn || redirOut || append ) {
				tokens.erase( it );
				if ( it != tokens.end() ) {
					if ( redirOut || append ) {
						if ( fileOut.is_opened() ) {
							fileOut.close();
						}
						fileOut.open( *it, append ? HFile::OPEN::WRITING | HFile::OPEN::APPEND : HFile::OPEN::WRITING );
						out = &fileOut;
					} else {
						if ( fileIn.is_opened() ) {
							fileIn.close();
						}
						fileIn.open( *it, HFile::OPEN::READING );
						in = &fileIn;
					}
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
#ifdef __MSVCXX__
			image.lower();
#endif
			system_commands_t::const_iterator it( sc_.find( image ) );
			if ( it != sc_.end() ) {
#ifndef __MSVCXX__
				image = it->second + PATH_SEP + image;
#else
				char const exts[][8] = { ".cmd", ".com", ".exe" };
				for ( char const* e : exts ) {
					image = it->second + PATH_SEP + tokens.front() + e;
					if ( filesystem::exists( image ) ) {
						break;
					}
				}
#endif
			}
			tokens.erase( tokens.begin() );
			pc.spawn( image, tokens, in, out, &cerr );
		} else {
			pc.spawn( *setup._shell, { "-c", join( tokens, " " ) }, in, out, &cerr );
		}
		ok = true;
		static time::duration_t const CENTURY( time::duration( 520, time::UNIT::WEEK ) );
		static int const CENTURY_IN_SECONDS( static_cast<int>( time::in_units<time::UNIT::SECOND>( CENTURY ) ) );
		HPipedChild::STATUS s( pc.finish( CENTURY_IN_SECONDS ) );
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

