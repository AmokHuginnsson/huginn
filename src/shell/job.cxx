/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <csignal>

#include <yaal/hcore/hrawfile.hxx>

M_VCSID( "$Id: " __ID__ " $" )
M_VCSID( "$Id: " __TID__ " $" )

#include "src/systemshell.hxx"
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

void capture_output( HStreamInterface::ptr_t stream_, HString& out_ ) {
	HChunk c;
	int long totalRead( 0 );
	try {
		int long toRead( system::get_page_size() );
		while ( true ) {
			c.realloc( totalRead + toRead, HChunk::STRATEGY::EXACT );
			int long nRead( stream_->read( c.get<char>() + totalRead, toRead ) );
			if ( nRead <= 0 ) {
				break;
			}
			totalRead += nRead;
			if ( nRead == toRead ) {
				toRead *= 2;
			}
		}
	} catch ( ... ) {
		/* Ignore all exceptions cause we are in the thread. */
	}
	out_.assign( c.get<char>(), totalRead );
	return;
}

}

namespace huginn {

HSystemShell::HJob::HJob( HSystemShell& systemShell_, commands_t&& commands_, EVALUATION_MODE evaluationMode_ )
	: _systemShell( systemShell_ )
	, _description( make_desc( commands_ ) )
	, _commands( yaal::move( commands_ ) )
	, _leader( HPipedChild::PROCESS_GROUP_LEADER )
	, _background( false )
	, _evaluationMode( evaluationMode_ )
	, _capturePipe()
	, _captureThread()
	, _captureBuffer()
	, _sec( call( &HJob::stop_capture, this ) ) {
	if ( _evaluationMode == EVALUATION_MODE::COMMAND_SUBSTITUTION ) {
		_capturePipe = make_resource<HPipe>();
		_captureThread = make_resource<HThread>();
		_commands.back()._out = _capturePipe->in();
		_captureThread->spawn( call( &capture_output, _capturePipe->out(), ref( _captureBuffer ) ) );
	}
	return;
}

yaal::hcore::HString HSystemShell::HJob::make_desc( commands_t const& commands_ ) const {
	M_PROLOG
	HString desc;
	for ( OCommand const& c : commands_ ) {
		if ( ! desc.is_empty() ) {
			desc.append( " | " );
		}
		desc.append( stringify_command( c._tokens ) );
	}
	return ( desc );
	M_EPILOG
}

bool HSystemShell::HJob::start( bool background_ ) {
	M_PROLOG
	bool validShell( false );
	for ( OCommand& c : _commands ) {
		validShell = _systemShell.spawn( c, _leader, ! background_ && ( &c == &_commands.back() ), _evaluationMode ) || validShell;
		if ( ( _leader == HPipedChild::PROCESS_GROUP_LEADER ) && !! c._child ) {
			_leader = c._child->get_pid();
		}
	}
	_background = background_;
	return ( validShell );
	M_EPILOG
}

HPipedChild::STATUS HSystemShell::HJob::wait_for_finish( void ) {
	M_PROLOG
	bool captureHuginn( !! _commands.back()._thread );
	HPipedChild::STATUS exitStatus;
	for ( OCommand& c : _commands ) {
		exitStatus = c.finish();
		if ( exitStatus.type == HPipedChild::STATUS::TYPE::PAUSED ) {
			cerr << "Suspended " << exitStatus.value << endl;
		} else if ( exitStatus.type != HPipedChild::STATUS::TYPE::FINISHED ) {
			cerr << "Abort " << exitStatus.value << endl;
		} else if ( exitStatus.value != 0 ) {
			cout << "Exit " << exitStatus.value << endl;
		}
	}
	if ( _evaluationMode == EVALUATION_MODE::COMMAND_SUBSTITUTION ) {
		_captureThread->finish();
		if ( captureHuginn ) {
			_captureBuffer.append( to_string( _systemShell._lineRunner.huginn()->result(), _systemShell._lineRunner.huginn() ) );
		}
	}
	return ( exitStatus );
	M_EPILOG
}

yaal::tools::HPipedChild::STATUS const& HSystemShell::HJob::status( void ) {
	M_PROLOG
	return ( _commands.back()._child->get_status() );
	M_EPILOG
}

bool HSystemShell::HJob::is_system_command( void ) const {
	return ( ! _commands.is_empty() && !! _commands.back()._child );
}

void HSystemShell::HJob::do_continue( bool background_ ) {
	piped_child_t& child( _commands.back()._child );
	HPipedChild::STATUS s( child->get_status() );
	system::kill( -_leader, SIGCONT );
	_background = background_;
	if ( s.type == HPipedChild::STATUS::TYPE::PAUSED ) {
		child->do_continue();
	}
	return;
}

void HSystemShell::HJob::bring_to_foreground( void ) {
	_background = false;
	return ( _commands.back()._child->bring_to_foreground() );
}

void HSystemShell::HJob::stop_capture( void ) {
	M_PROLOG
	if ( !! _capturePipe && _capturePipe->in()->is_valid() ) {
		const_cast<HRawFile*>( static_cast<HRawFile const*>( _capturePipe->in().raw() ) )->close();
	}
	return;
	M_EPILOG
}

}

