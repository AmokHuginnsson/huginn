#ifndef HUGINN_SHELL_CAPTURE_HXX_INCLUDED
#define HUGINN_SHELL_CAPTURE_HXX_INCLUDED 1

#include <yaal/hcore/hpipe.hxx>
#include <yaal/hcore/hthread.hxx>

#include "src/systemshell.hxx"

namespace huginn {

class HSystemShell::HCapture {
private:
	yaal::hcore::HPipe _pipe;
	yaal::hcore::HThread::call_t _call;
	yaal::hcore::HThread _thread;
	yaal::hcore::HString _buffer;
	QUOTES _quotes;
	yaal::hcore::HMutex _mutex;
public:
	HCapture( QUOTES );
	virtual ~HCapture( void );
	QUOTES quotes( void ) const {
		return ( _quotes );
	}
	void set_call( yaal::hcore::HThread::call_t const& );
	void run( void );
	void task( void );
	void stop( void );
	void finish( void );
	yaal::hcore::HStreamInterface::ptr_t pipe_in( void ) const;
	yaal::hcore::HStreamInterface::ptr_t pipe_out( void ) const;
	void append( yaal::hcore::HString const& );
	yaal::hcore::HString const& buffer( void );
};

}

#endif /* #ifndef HUGINN_SHELL_CAPTURE_HXX_INCLUDED */

