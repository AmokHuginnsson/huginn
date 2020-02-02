/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <yaal/hcore/system.hxx>
#include <yaal/hcore/hrawfile.hxx>
#include <yaal/tools/huginn/helper.hxx>

M_VCSID( "$Id: " __ID__ " $" )
M_VCSID( "$Id: " __TID__ " $" )

#include "src/systemshell.hxx"
#include "command.hxx"
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

void spell_out_status( yaal::hcore::HString& msg_, char const* reason_, int code_ ) {
	msg_.append( msg_.is_empty() ? "" : "\n" ).append( reason_ ).append( code_ );
}

}

bool HSystemShell::OCommand::compile( EVALUATION_MODE evaluationMode_ ) {
	M_PROLOG
	_systemShell.resolve_aliases( _tokens );
	tokens_t tokens( _systemShell.denormalize( _tokens, evaluationMode_ ) );
	_isShellCommand = _systemShell.is_command( tokens.front() );
	if ( _isShellCommand && setup._shell->is_empty() && ( _systemShell.builtins().count( tokens.front() ) == 0 ) ) {
		_tokens = tokens;
	}
	if ( _isShellCommand ) {
		return ( true );
	}
	unescape_huginn_command( *this );
	HString line( string::join( _tokens, " " ) );
	if ( _systemShell.line_runner().add_line( line, _systemShell.loaded() ) ) {
		return ( true );
	}
	cerr << _systemShell.line_runner().err() << endl;
	return ( false );
	M_EPILOG
}

bool HSystemShell::OCommand::spawn( int pgid_, bool foreground_, bool overwriteImage_ ) {
	M_PROLOG
	if ( ! _isShellCommand ) {
		return ( spawn_huginn( foreground_ ) );
	}
	builtins_t::const_iterator builtin( _systemShell.builtins().find( _tokens.front() ) );
	if ( builtin != _systemShell.builtins().end() ) {
		if ( foreground_ ) {
			run_builtin( builtin->second );
		} else {
			_promise = make_resource<future_t>( call( &OCommand::run_builtin, this, builtin->second ), HWorkFlow::SCHEDULE_POLICY::EAGER );
		}
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
			if ( overwriteImage_ ) {
				_systemShell.session_stop();
				try {
					system::exec( image, _tokens );
				} catch ( ... ) {
					_systemShell.session_start();
				}
			} else {
				_tokens.erase( _tokens.begin() );
			}
		} else {
			image.assign( *setup._shell );
			HString command( join( _tokens, " " ) );
			_tokens.clear();
			_tokens.push_back( "-c" );
			_tokens.push_back( command );
		}
		_child = make_resource<HPipedChild>( _in, _out, _err );
		_child->spawn(
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

bool HSystemShell::OCommand::spawn_huginn( bool foreground_ ) {
	M_PROLOG
	HLineRunner& lr( _systemShell.line_runner() );
	if ( !! _in ) {
		lr.huginn()->set_input_stream( _in );
	}
	if ( !! _out ) {
		lr.huginn()->set_output_stream( _out );
	}
	if ( !! _err ) {
		lr.huginn()->set_error_stream( _err );
	}
	if ( foreground_ ) {
		run_huginn( lr );
	} else {
		_promise = make_resource<future_t>( call( &OCommand::run_huginn, this, ref( lr ) ), HWorkFlow::SCHEDULE_POLICY::EAGER );
	}
	return ( true );
	M_EPILOG
}

yaal::tools::HPipedChild::STATUS HSystemShell::OCommand::run_builtin( builtin_t const& builtin_ ) {
	M_PROLOG
	try {
		_status.type = HPipedChild::STATUS::TYPE::RUNNING;
		builtin_( *this );
		_status.type = HPipedChild::STATUS::TYPE::FINISHED;
	} catch ( HException const& e ) {
		_failureMessage.assign( e.what() );
		_status.type = HPipedChild::STATUS::TYPE::FINISHED;
		_status.value = 1;
	}
	return ( _status );
	M_EPILOG
}

yaal::tools::HPipedChild::STATUS HSystemShell::OCommand::run_huginn( HLineRunner& lineRunner_ ) {
	M_PROLOG
	HHuginn& huginn( *lineRunner_.huginn() );
	try {
		_status.type = HPipedChild::STATUS::TYPE::RUNNING;
		lineRunner_.execute();
		if ( !! huginn.result() ) {
			_huginnExecuted = true;
			_status.type = HPipedChild::STATUS::TYPE::FINISHED;
			if ( huginn.result()->type_id() == HHuginn::TYPE::INTEGER ) {
				_status.value = static_cast<int>( tools::huginn::get_integer( huginn.result() ) );
			} else if ( huginn.result()->type_id() == HHuginn::TYPE::BOOLEAN ) {
				_status.value = tools::huginn::get_boolean( huginn.result() ) ? 0 : 1;
			} else {
				_status.value = 0;
			}
		} else {
			_failureMessage.assign( huginn.error_message() );
			_status.type = HPipedChild::STATUS::TYPE::ABORTED;
			_status.value = 1;
		}
	} catch ( HException const& e ) {
		_failureMessage.assign( e.what() );
		_status.type = HPipedChild::STATUS::TYPE::ABORTED;
		_status.value = 1;
	}
	huginn.set_input_stream( cin );
	huginn.set_output_stream( cout );
	huginn.set_error_stream( cerr );
	return ( _status );
	M_EPILOG
}

yaal::tools::HPipedChild::STATUS HSystemShell::OCommand::do_finish( void ) {
	M_PROLOG
	_in.reset();
	if ( !! _child ) {
		_status = _child->get_status();
		if ( _status.type == HPipedChild::STATUS::TYPE::PAUSED ) {
			_child->restore_parent_term();
			return ( _status );
		}
		_status = _child->wait();
		_child.reset();
	} else if ( !! _promise ) {
		_status = _promise->get();
		_promise.reset();
	}
	HRawFile* fd( dynamic_cast<HRawFile*>( _out.raw() ) );
	if ( fd && fd->is_valid() ) {
		fd->close();
	}
	_out.reset();
	_pipe.reset();
	return ( _status );
	M_EPILOG
}

yaal::tools::HPipedChild::STATUS HSystemShell::OCommand::finish( bool predecessor_ ) {
	M_PROLOG
	yaal::tools::HPipedChild::STATUS exitStatus( do_finish() );
	if ( predecessor_ && !! _err ) {
	} else if ( exitStatus.type == HPipedChild::STATUS::TYPE::PAUSED ) {
		spell_out_status( _failureMessage, "Suspended ", exitStatus.value );
	} else if ( exitStatus.type != HPipedChild::STATUS::TYPE::FINISHED ) {
		spell_out_status( _failureMessage, "Abort ", exitStatus.value );
	} else if ( exitStatus.value != 0 ) {
		spell_out_status( _failureMessage, "Exit ", exitStatus.value );
	}
	return ( exitStatus );
	M_EPILOG
}

yaal::tools::HPipedChild::STATUS const& HSystemShell::OCommand::get_status( void ) {
	M_PROLOG
	if ( !! _child ) {
		_status = _child->get_status();
	}
	return ( _status );
	M_EPILOG
}

bool HSystemShell::OCommand::is_shell_command( void ) const {
	return ( _isShellCommand );
}

yaal::hcore::HString const& HSystemShell::OCommand::failure_message( void ) const {
	return ( _failureMessage );
}

}

