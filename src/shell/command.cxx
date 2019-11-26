/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <yaal/hcore/hrawfile.hxx>
#include <yaal/tools/huginn/helper.hxx>

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

yaal::tools::HPipedChild::STATUS HSystemShell::OCommand::finish( void ) {
	M_PROLOG
	yaal::tools::HPipedChild::STATUS exitStatus( do_finish() );
	if ( exitStatus.type == HPipedChild::STATUS::TYPE::PAUSED ) {
		cerr << "Suspended " << exitStatus.value << endl;
	} else if ( exitStatus.type != HPipedChild::STATUS::TYPE::FINISHED ) {
		cerr << "Abort " << exitStatus.value << endl;
	} else if ( exitStatus.value != 0 ) {
		cout << "Exit " << exitStatus.value << endl;
	}
	return ( exitStatus );
	M_EPILOG
}

}

