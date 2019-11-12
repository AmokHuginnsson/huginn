/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <yaal/hcore/hrawfile.hxx>

M_VCSID( "$Id: " __ID__ " $" )
M_VCSID( "$Id: " __TID__ " $" )

#include "src/systemshell.hxx"

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;
using namespace yaal::tools::util;
using namespace yaal::tools::string;

namespace huginn {

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
	try {
		lineRunner_.execute();
		lineRunner_.huginn()->set_input_stream( cin );
		lineRunner_.huginn()->set_output_stream( cout );
		lineRunner_.huginn()->set_error_stream( cerr );
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

yaal::tools::HPipedChild::STATUS HSystemShell::OCommand::finish( void ) {
	M_PROLOG
	_in.reset();
	HPipedChild::STATUS s;
	if ( !! _child ) {
		s = _child->wait();
		if ( s.type == HPipedChild::STATUS::TYPE::PAUSED ) {
			_child->restore_parent_term();
			return ( s );
		}
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

}

