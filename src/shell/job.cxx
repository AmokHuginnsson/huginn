/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <csignal>

#include <yaal/hcore/hrawfile.hxx>
#include <yaal/hcore/system.hxx>

M_VCSID( "$Id: " __ID__ " $" )
M_VCSID( "$Id: " __TID__ " $" )

#include "src/systemshell.hxx"
#include "src/setup.hxx"
#include "command.hxx"
#include "job.hxx"
#include "util.hxx"

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;
using namespace yaal::tools::util;
using namespace yaal::tools::string;

#ifndef SIGCONT
static int const SIGCONT = 19;
#endif

namespace {

bool is_finished( HPipedChild::STATUS status_ ) {
	return ( ( status_.type != HPipedChild::STATUS::TYPE::RUNNING ) && ( status_.type != HPipedChild::STATUS::TYPE::PAUSED ) );
}

}

namespace huginn {

HSystemShell::HJob::HJob( HSystemShell& systemShell_, commands_t&& commands_, HCapture* capture_, EVALUATION_MODE evaluationMode_, bool predecessor_, bool lastChain_ )
	: _systemShell( systemShell_ )
	, _description( make_desc( commands_ ) )
	, _commands( yaal::move( commands_ ) )
	, _leader( HPipedChild::PROCESS_GROUP_LEADER )
	, _background( false )
	, _evaluationMode( evaluationMode_ )
	, _predecessor( predecessor_ )
	, _lastChain( lastChain_ )
	, _failureMessages()
	, _capture( capture_ )
	, _sec( call( &HJob::stop_capture, this ) ) {
	if ( _evaluationMode == EVALUATION_MODE::COMMAND_SUBSTITUTION ) {
		if ( capture_->quotes() == QUOTES::EXEC_SINK ) {
			_commands.front()->_in = _capture->pipe_out();
		} else {
			_commands.back()->_out = _capture->pipe_in();
		}
	}
	return;
}

yaal::hcore::HString HSystemShell::HJob::make_desc( commands_t const& commands_ ) const {
	M_PROLOG
	HString desc;
	for ( command_t const& c : commands_ ) {
		if ( ! desc.is_empty() ) {
			desc.append( " | " );
		}
		desc.append( stringify_command( c->_tokens ) );
	}
	return ( desc );
	M_EPILOG
}

bool HSystemShell::HJob::start( bool background_ ) {
	M_PROLOG
	bool validShell( false );
	bool hasHuginnExpression( false );
	for ( command_t& c : _commands ) {
		if ( ! c->compile( _evaluationMode ) ) {
			return ( false );
		}
		if ( hasHuginnExpression && ! c->is_shell_command() ) {
			cerr << "Only one Huginn expression per pipe is allowed." << endl;
			return ( false );
		}
		hasHuginnExpression = hasHuginnExpression || ! c->is_shell_command();
	}
	for ( command_t& c : _commands ) {
		OCommand& cmd( *c );
		bool lastCommand( c == _commands.back() );
		bool isShellCommand( c->is_shell_command() );
		bool singleCommand( _commands.get_size() == 1 );
		bool foreground(
			! background_
			&& lastCommand
			&& ( isShellCommand || singleCommand )
			&& ( ! isShellCommand || ( _evaluationMode != EVALUATION_MODE::COMMAND_SUBSTITUTION ) )
		);
		bool closeOut( ! isShellCommand || ! lastCommand || ( _lastChain && ! _predecessor ) );
		bool overwriteImage(
			!! setup._program
			&& isShellCommand
			&& ! background_
			&& _lastChain
			&& ! _predecessor
			&& singleCommand
			&& ! c->_in && ! c->_out && ! c->_err
		);
		validShell = c->spawn( _leader, foreground, overwriteImage, closeOut, lastCommand ) || validShell;
		if ( ( _leader == HPipedChild::PROCESS_GROUP_LEADER ) && !! cmd._child ) {
			_leader = cmd._child->get_pid();
		}
	}
	_background = background_;
	return ( validShell );
	M_EPILOG
}

yaal::tools::HPipedChild::process_group_t HSystemShell::HJob::process_group( void ) {
	M_PROLOG
	yaal::tools::HPipedChild::process_group_t processGroup;
	for ( command_t& c : _commands ) {
		if ( ! c->_child ) {
			continue;
		}
		processGroup.push_back( c->_child.raw() );
	}
	return ( processGroup );
	M_EPILOG
}

HSystemShell::commands_t::iterator HSystemShell::HJob::process_to_command( HPipedChild const* process_ ) {
	M_PROLOG
	return (
		find_if(
			_commands.begin(),
			_commands.end(),
			[process_]( command_t const& cmd_ ) {
				return ( cmd_->_child.raw() == process_ );
			}
		)
	);
	M_EPILOG
}

yaal::tools::HPipedChild::STATUS HSystemShell::HJob::gather_results( command_t& cmd_ ) {
	yaal::tools::HPipedChild::STATUS exitStatus( cmd_->finish( _predecessor || ( cmd_ != _commands.back() ) ) );
	if ( is_finished( exitStatus ) && ! cmd_->failure_message().is_empty() ) {
		_failureMessages.push_back( cmd_->failure_message() );
	}
	return ( exitStatus );
}

yaal::tools::HPipedChild::STATUS HSystemShell::HJob::finish_non_process(
	commands_t::iterator start_,
	yaal::tools::HPipedChild::STATUS exitStatus_
) {
	M_PROLOG
	for ( commands_t::iterator cmd( start_ ); cmd != _commands.end(); ) {
		piped_child_t& child( (*cmd)->_child );
		if ( !! child ) {
			HPipedChild::STATUS s( child->get_status() );
			if ( ( s.type == HPipedChild::STATUS::TYPE::RUNNING ) || ( s.type == HPipedChild::STATUS::TYPE::PAUSED ) ) {
				break;
			}
		}
		exitStatus_ = gather_results( *cmd );
		cmd = _commands.erase( cmd );
	}
	return ( exitStatus_ );
	M_EPILOG
}

HPipedChild::STATUS HSystemShell::HJob::wait_for_finish( void ) {
	M_PROLOG
	HHuginn::value_t huginnResult( yaal::move( _commands.back()->_huginnResult ) );
	HPipedChild::STATUS exitStatus( finish_non_process( _commands.begin() ) );
	while ( ! _commands.is_empty() && ( exitStatus.type != HPipedChild::STATUS::TYPE::PAUSED ) ) {
		HPipedChild::process_group_t processGroup( process_group() );
		HPipedChild::process_group_t::iterator finishedProcess( HPipedChild::wait_for_process_group( processGroup ) );
		if ( finishedProcess == processGroup.end() ) {
			break;
		}
		commands_t::iterator finishedCommand( process_to_command( *finishedProcess ) );
		M_ASSERT( finishedCommand != _commands.end() );
		exitStatus = gather_results( *finishedCommand );
		++ finishedCommand;
		if ( !! _commands.back()->_huginnResult ) {
			huginnResult = yaal::move( _commands.back()->_huginnResult );
		}
		exitStatus = finish_non_process( finishedCommand, exitStatus );
	}
	if ( _evaluationMode == EVALUATION_MODE::COMMAND_SUBSTITUTION ) {
		if ( _lastChain && ! _predecessor && ( _capture->quotes() == QUOTES::EXEC ) ) {
			_capture->finish();
		}
		if ( !! huginnResult ) {
			_capture->append( to_string( huginnResult, _systemShell._lineRunner.huginn() ) );
		}
	}
	return ( exitStatus );
	M_EPILOG
}

yaal::tools::HPipedChild::STATUS const& HSystemShell::HJob::status( void ) {
	M_PROLOG
	return ( _commands.back()->get_status() );
	M_EPILOG
}

bool HSystemShell::HJob::has_huginn_jobs( void ) const {
	M_PROLOG
	for ( command_t const& c : _commands ) {
		if ( ! c->is_shell_command() ) {
			return ( true );
		}
	}
	return ( false );
	M_EPILOG
}

void HSystemShell::HJob::do_continue( bool background_ ) {
	if ( _leader != HPipedChild::PROCESS_GROUP_LEADER ) {
		system::kill( -_leader, SIGCONT );
	}
	_background = background_;
	for ( command_t& c : _commands ) {
		piped_child_t& child( c->_child );
		if ( ! child ) {
			continue;
		}
		HPipedChild::STATUS s( child->get_status() );
		if ( s.type == HPipedChild::STATUS::TYPE::PAUSED ) {
			child->do_continue();
		}
	}
	return;
}

void HSystemShell::HJob::bring_to_foreground( void ) {
	M_PROLOG
	piped_child_t& child( _commands.back()->_child );
	if ( ! child ) {
		return;
	}
	child->bring_to_foreground();
	_background = false;
	return;
	M_EPILOG
}

void HSystemShell::HJob::stop_capture( void ) {
	M_PROLOG
	if ( _lastChain && ! _predecessor && !! _capture ) {
		_capture->stop();
	}
	return;
	M_EPILOG
}

bool HSystemShell::HJob::can_orphan( void ) {
	M_PROLOG
	for ( command_t& c : _commands ) {
		if ( ! c->_child || ( c->get_status().type == HPipedChild::STATUS::TYPE::PAUSED ) ) {
			return ( false );
		}
	}
	return ( true );
	M_EPILOG
}

void HSystemShell::HJob::orphan( void ) {
	M_PROLOG
	for ( command_t& c : _commands ) {
		c->_child.release();
	}
	return;
	M_EPILOG
}

}

