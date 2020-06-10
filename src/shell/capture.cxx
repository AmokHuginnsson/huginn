/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <yaal/hcore/system.hxx>
#include <yaal/hcore/hrawfile.hxx>

M_VCSID( "$Id: " __ID__ " $" )
M_VCSID( "$Id: " __TID__ " $" )

#include "capture.hxx"

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;
using namespace yaal::tools::util;
using namespace yaal::tools::string;

namespace huginn {

HSystemShell::HCapture::HCapture( QUOTES quotes_ )
	: _pipe()
	, _thread()
	, _buffer()
	, _quotes( quotes_ )
	, _mutex() {
	M_PROLOG
	if ( quotes_ == QUOTES::EXEC ) {
		_thread.spawn( call( &HCapture::task, this ) );
	} else {
		int fd( static_cast<HRawFile const*>( ( quotes_ == QUOTES::EXEC_SOURCE ? _pipe.out() : _pipe.in() ).get() )->get_file_descriptor() );
		system::set_close_on_exec( fd, false );
		_buffer.assign( "/dev/fd/" ).append( fd );
	}
	return;
	M_EPILOG
}

HSystemShell::HCapture::~HCapture( void ) {
	M_PROLOG
	finish();
	return;
	M_DESTRUCTOR_EPILOG
}

HStreamInterface::ptr_t HSystemShell::HCapture::pipe_in( void ) const {
	M_PROLOG
	return ( _pipe.in() );
	M_EPILOG
}

HStreamInterface::ptr_t HSystemShell::HCapture::pipe_out( void ) const {
	M_PROLOG
	return ( _pipe.out() );
	M_EPILOG
}

void HSystemShell::HCapture::run( yaal::hcore::HThread::call_t const& call_ ) {
	M_PROLOG
	_thread.spawn( call_ );
	return;
	M_EPILOG
}

void HSystemShell::HCapture::task( void ) {
	M_PROLOG
	HChunk c;
	int long totalRead( 0 );
	try {
		int long toRead( system::get_page_size() );
		HStreamInterface::ptr_t outPtr( _pipe.out() );
		HStreamInterface& out( *outPtr );
		while ( true ) {
			c.realloc( totalRead + toRead, HChunk::STRATEGY::EXACT );
			int long nRead( out.read( c.get<char>() + totalRead, toRead ) );
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
	HLock l( _mutex );
	_buffer.assign( c.get<char>(), totalRead );
	return;
	M_EPILOG
}

namespace {

void permissive_close( HStreamInterface::ptr_t const& stream_ ) {
	M_PROLOG
	if ( ! stream_->is_valid() ) {
		return;
	}
	const_cast<HRawFile*>( static_cast<HRawFile const*>( stream_.raw() ) )->close();
	return;
	M_EPILOG
}

}

void HSystemShell::HCapture::close_dangling( void ) {
	M_PROLOG
	if ( _quotes == QUOTES::EXEC_SOURCE ) {
		permissive_close( _pipe.out() );
	} else if ( _quotes == QUOTES::EXEC_SINK ) {
		permissive_close( _pipe.in() );
	}
	return;
	M_EPILOG
}

void HSystemShell::HCapture::stop( void ) {
	M_PROLOG
	HLock l( _mutex );
	_pipe.flush();
	if ( _quotes != QUOTES::EXEC_SINK ) {
		permissive_close( _pipe.in() );
		permissive_close( _pipe.out() );
	}
	return;
	M_EPILOG
}

void HSystemShell::HCapture::finish( void ) {
	M_PROLOG
	stop();
	if ( _thread.is_alive() ) {
		_thread.finish();
	}
	return;
	M_EPILOG
}

void HSystemShell::HCapture::append( yaal::hcore::HString const& str_ ) {
	M_PROLOG
	HLock l( _mutex );
	if ( _quotes == QUOTES::EXEC ) {
		_buffer.append( str_ );
	} else if ( _quotes == QUOTES::EXEC_SOURCE ) {
		*const_cast<HStreamInterface*>( _pipe.in().get() ) << str_ << flush;
	}
	return;
	M_EPILOG
}

yaal::hcore::HString const& HSystemShell::HCapture::buffer( void ) {
	HLock l( _mutex );
	_buffer.trim();
	return ( _buffer );
}

}

