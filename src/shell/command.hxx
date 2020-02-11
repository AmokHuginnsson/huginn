#ifndef HUGINN_SHELL_COMMAND_HXX_INCLUDED
#define HUGINN_SHELL_COMMAND_HXX_INCLUDED 1

#include <yaal/hcore/hfile.hxx>
#include <yaal/hcore/hpipe.hxx>
#include <yaal/tools/hfuture.hxx>

#include "src/systemshell.hxx"

namespace huginn {

struct HSystemShell::OCommand {
	typedef yaal::tools::HFuture<yaal::tools::HPipedChild::STATUS> future_t;
	typedef yaal::hcore::HResource<future_t> promise_t;
	HSystemShell& _systemShell;
	yaal::hcore::HStreamInterface::ptr_t _in;
	yaal::hcore::HStreamInterface::ptr_t _out;
	yaal::hcore::HStreamInterface::ptr_t _err;
	tokens_t _tokens;
	promise_t _promise;
	piped_child_t _child;
	yaal::hcore::HPipe::ptr_t _pipe;
	bool _isShellCommand;
	bool _huginnExecuted;
	yaal::tools::HPipedChild::STATUS _status;
	yaal::hcore::HString _failureMessage;
	OCommand( HSystemShell& systemShell_ )
		: _systemShell( systemShell_ )
		, _in()
		, _out()
		, _err()
		, _tokens()
		, _promise()
		, _child()
		, _pipe()
		, _isShellCommand( false )
		, _huginnExecuted( false )
		, _status()
		, _failureMessage() {
	}
	template<typename T>
	yaal::hcore::HStreamInterface& operator << ( T const& val_ ) {
		yaal::hcore::HStreamInterface* s( !! _out ? _out.raw() : &_systemShell.repl() );
		*s << val_;
		return ( *s );
	}
	bool compile( EVALUATION_MODE );
	bool spawn( int, bool, bool );
	bool spawn_huginn( bool );
	yaal::tools::HPipedChild::STATUS run_huginn( HLineRunner& );
	yaal::tools::HPipedChild::STATUS run_builtin( builtin_t const& );
	yaal::tools::HPipedChild::STATUS finish( bool );
	bool is_shell_command( void ) const;
	yaal::hcore::HString const& failure_message( void ) const;
	yaal::tools::HPipedChild::STATUS const& get_status( void );
private:
	yaal::tools::HPipedChild::STATUS do_finish( void );
};

}

#endif /* #ifndef HUGINN_SHELL_COMMAND_HXX_INCLUDED */
