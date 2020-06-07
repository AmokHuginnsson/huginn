#ifndef HUGINN_SHELL_CAPTURE_HXX_INCLUDED
#define HUGINN_SHELL_CAPTURE_HXX_INCLUDED 1

#include <yaal/hcore/hpipe.hxx>
#include <yaal/hcore/hthread.hxx>

#include "src/systemshell.hxx"

namespace huginn {

class HSystemShell::HCapture {
private:
	yaal::hcore::HPipe _pipe;
	yaal::hcore::HResource<yaal::hcore::HThread> _thread;
	yaal::hcore::HString _buffer;
	QUOTES _quotes;
	yaal::hcore::HMutex _mutex;
public:
	HCapture( QUOTES );
	virtual ~HCapture( void );
	void task( void );
	void stop( void );
	void finish( void );
	yaal::hcore::HStreamInterface::ptr_t pipe_in( void ) const;
	void append( yaal::hcore::HString const& );
	yaal::hcore::HString const& buffer( void ) const {
		return ( _buffer );
	}
};

}

#endif /* #ifndef HUGINN_SHELL_CAPTURE_HXX_INCLUDED */

