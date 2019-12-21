/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <yaal/hcore/hrawfile.hxx>
#include <yaal/tools/huginn/helper.hxx>

M_VCSID( "$Id: " __ID__ " $" )
M_VCSID( "$Id: " __TID__ " $" )

#include "src/systemshell.hxx"
#include "src/quotes.hxx"
#include "src/setup.hxx"

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;
using namespace yaal::tools::util;
using namespace yaal::tools::string;

namespace huginn {

namespace {

void unescape_huginn_command( HSystemShell::OCommand& command_ ) {
	M_PROLOG
	for ( yaal::hcore::HString& s : command_._tokens ) {
		s = unescape_huginn_code( s );
	}
	return;
	M_EPILOG
}

}

bool HSystemShell::OCommand::compile( EVALUATION_MODE evaluationMode_ ) {
	M_PROLOG
	_systemShell.resolve_aliases( _tokens );
	tokens_t tokens( _systemShell.denormalize( _tokens, evaluationMode_ ) );
	_isSystemCommand = _systemShell.is_command( tokens.front() );
	if ( _isSystemCommand && setup._shell->is_empty() && ( _systemShell.builtins().count( tokens.front() ) == 0 ) ) {
		_tokens = tokens;
	}
	if ( ! _isSystemCommand ) {
		unescape_huginn_command( *this );
		HString line( string::join( _tokens, " " ) );
		if ( ! _systemShell.line_runner().add_line( line, _systemShell.loaded() ) ) {
			cerr << _systemShell.line_runner().err() << endl;
			return ( false );
		}
	}
	return ( true );
	M_EPILOG
}

bool HSystemShell::OCommand::spawn( int pgid_, bool foreground_ ) {
	M_PROLOG
	if ( ! _isSystemCommand ) {
		return ( spawn_huginn() );
	}
	builtins_t::const_iterator builtin( _systemShell.builtins().find( _tokens.front() ) );
	if ( builtin != _systemShell.builtins().end() ) {
		_thread = make_pointer<HThread>();
		_thread->spawn( call( &OCommand::run_builtin, this, builtin->second ) );
		return ( true );
	}
	bool ok( true );
	try {
		HString image;
		if ( _tokens.is_empty() ) {
			return ( false );
		}
		if ( setup._shell->is_empty() ) {
			image.assign( _tokens.front() );
#ifdef __MSVCXX__
			image.lower();
#endif
			system_commands_t::const_iterator it( _systemShell.system_commands().find( image ) );
			if ( it != _systemShell.system_commands().end() ) {
#ifndef __MSVCXX__
				image.assign( it->second ).append( PATH_SEP ).append( it->first );
#else
				char const exts[][8] = { ".cmd", ".com", ".exe" };
				for ( char const* e : exts ) {
					image.assign( it->second ).append( PATH_SEP ).append( _tokens.front() ).append( e );
					if ( filesystem::exists( image ) ) {
						break;
					}
				}
#endif
			}
			_tokens.erase( _tokens.begin() );
		} else {
			image.assign( *setup._shell );
			HString command( join( _tokens, " " ) );
			_tokens.clear();
			_tokens.push_back( "-c" );
			_tokens.push_back( command );
		}
		piped_child_t pc( make_pointer<HPipedChild>( _in, _out, _err ) );
		_child = pc;
		pc->spawn(
			image,
			_tokens,
			! _in ? &cin : nullptr,
			! _out ? &cout : nullptr,
			! _err ? &cerr : nullptr,
			pgid_,
			foreground_
		);
	} catch ( HException const& e ) {
		cerr << e.what() << endl;
		ok = false;
	}
	return ( ok );
	M_EPILOG
}

bool HSystemShell::OCommand::spawn_huginn( void ) {
	M_PROLOG
	_thread = make_pointer<HThread>();
	if ( !! _in ) {
		_systemShell.line_runner().huginn()->set_input_stream( _in );
	}
	if ( !! _out ) {
		_systemShell.line_runner().huginn()->set_output_stream( _out );
	}
	if ( !! _err ) {
		_systemShell.line_runner().huginn()->set_error_stream( _err );
	}
	_thread->spawn( call( &OCommand::run_huginn, this, ref( _systemShell.line_runner() ) ) );
	return ( true );
	M_EPILOG
}

void HSystemShell::OCommand::run_builtin( builtin_t const& builtin_ ) {
	M_PROLOG
	try {
		builtin_( *this );
		_status.type = HPipedChild::STATUS::TYPE::FINISHED;
		_status.value = 0;
	} catch ( HException const& e ) {
		cerr << e.what() << endl;
		_status.type = HPipedChild::STATUS::TYPE::FINISHED;
		_status.value = 1;
	}
	return;
	M_EPILOG
}

void HSystemShell::OCommand::run_huginn( HLineRunner& lineRunner_ ) {
	M_PROLOG
	HHuginn& huginn( *lineRunner_.huginn() );
	try {
		lineRunner_.execute();
		if ( !! huginn.result() ) {
			_status.type = HPipedChild::STATUS::TYPE::FINISHED;
			_status.value =
				( huginn.result()->type_id() == HHuginn::TYPE::INTEGER )
					? static_cast<int>( tools::huginn::get_integer( huginn.result() ) )
					: 0;
		} else {
			cerr << huginn.error_message() << endl;
			_status.type = HPipedChild::STATUS::TYPE::ABORTED;
			_status.value = 1;
		}
	} catch ( HException const& e ) {
		cerr << e.what() << endl;
		_status.type = HPipedChild::STATUS::TYPE::ABORTED;
		_status.value = 1;
	}
	huginn.set_input_stream( cin );
	huginn.set_output_stream( cout );
	huginn.set_error_stream( cerr );
	return;
	M_EPILOG
}

yaal::tools::HPipedChild::STATUS HSystemShell::OCommand::do_finish( void ) {
	M_PROLOG
	_in.reset();
	HPipedChild::STATUS s;
	if ( !! _child ) {
		s = _child->get_status();
		if ( s.type == HPipedChild::STATUS::TYPE::PAUSED ) {
			_child->restore_parent_term();
			return ( s );
		}
		s = _child->wait();
		_child.reset();
	} else if ( !! _thread ) {
		_thread->finish();
		_thread.reset();
		s = _status;
	} else {
		s.type = HPipedChild::STATUS::TYPE::FINISHED;
	}
	HRawFile* fd( dynamic_cast<HRawFile*>( _out.raw() ) );
	if ( fd && fd->is_valid() ) {
		fd->close();
	}
	_out.reset();
	_pipe.reset();
	return ( s );
	M_EPILOG
}

yaal::tools::HPipedChild::STATUS HSystemShell::OCommand::finish( bool predecessor_ ) {
	M_PROLOG
	yaal::tools::HPipedChild::STATUS exitStatus( do_finish() );
	if ( predecessor_ && !! _err ) {
	} else if ( exitStatus.type == HPipedChild::STATUS::TYPE::PAUSED ) {
		cerr << "Suspended " << exitStatus.value << endl;
	} else if ( exitStatus.type != HPipedChild::STATUS::TYPE::FINISHED ) {
		cerr << "Abort " << exitStatus.value << endl;
	} else if ( exitStatus.value != 0 ) {
		cout << "Exit " << exitStatus.value << endl;
	}
	return ( exitStatus );
	M_EPILOG
}

bool HSystemShell::OCommand::is_system_command( void ) const {
	return ( _isSystemCommand );
}

}

