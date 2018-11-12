/* Read huginn/LICENSE.md file for copyright and licensing information. */

/*! \file shell.hxx
 * \brief Declaration of shell function.
 */

#ifndef HUGINN_SHELL_HXX_INCLUDED
#define HUGINN_SHELL_HXX_INCLUDED 1

#include <yaal/hcore/hresource.hxx>
#include <yaal/hcore/hstring.hxx>
#include <yaal/hcore/hfile.hxx>
#include <yaal/hcore/hpipe.hxx>
#include <yaal/tools/stringalgo.hxx>
#include <yaal/tools/hpipedchild.hxx>
#include <yaal/tools/filesystem.hxx>

#include "linerunner.hxx"

namespace huginn {

class HShell {
public:
	typedef yaal::tools::string::tokens_t tokens_t;
	typedef yaal::hcore::HPointer<yaal::tools::HPipedChild> piped_child_t;
	typedef yaal::hcore::HPointer<yaal::hcore::HThread> thread_t;
	struct OSpawnResult {
		yaal::tools::HPipedChild::STATUS _exitStatus;
		bool _validShell;
		OSpawnResult( yaal::tools::HPipedChild::STATUS exitStatus_ = yaal::tools::HPipedChild::STATUS(), bool validShell_ = false )
			: _exitStatus( exitStatus_ )
			, _validShell( validShell_ ) {
		}
	};
	struct OCommand {
		yaal::hcore::HStreamInterface::ptr_t _in;
		yaal::hcore::HStreamInterface::ptr_t _out;
		tokens_t _tokens;
		thread_t _thread;
		piped_child_t _child;
		yaal::hcore::HPipe::ptr_t _pipe;
		OCommand( void )
			: _in()
			, _out()
			, _tokens()
			, _thread()
			, _child()
			, _pipe() {
		}
		template<typename T>
		yaal::hcore::HStreamInterface& operator << ( T const& val_ ) {
			yaal::hcore::HStreamInterface* s( !! _out ? _out.raw() : &yaal::hcore::cout );
			*s << val_;
			return ( *s );
		}
		yaal::tools::HPipedChild::STATUS finish( void );
	};
	typedef yaal::hcore::HHashMap<yaal::hcore::HString, yaal::hcore::HString> system_commands_t;
	typedef yaal::hcore::HBoundCall<void ( OCommand& )> builtin_t;
	typedef yaal::hcore::HHashMap<yaal::hcore::HString, builtin_t> builtins_t;
	typedef yaal::hcore::HMap<yaal::hcore::HString, tokens_t> aliases_t;
	typedef yaal::hcore::HArray<yaal::tools::filesystem::path_t> dir_stack_t;
private:
	HLineRunner& _lineRunner;
	system_commands_t _systemCommands;
	builtins_t _builtins;
	aliases_t _aliases;
	dir_stack_t _dirStack;
public:
	HShell( HLineRunner& );
	system_commands_t const& system_commands( void ) const;
	builtins_t const& builtins( void ) const {
		return ( _builtins );
	}
	aliases_t const& aliases( void ) const {
		return ( _aliases );
	}
	bool run( yaal::hcore::HString const& );
	HLineRunner::words_t filename_completions( yaal::hcore::HString const&, yaal::hcore::HString const& ) const;
	bool is_command( yaal::hcore::HString const& ) const;
private:
	void alias( OCommand& );
	void unalias( OCommand& );
	void cd( OCommand& );
	void setenv( OCommand& );
	void unsetenv( OCommand& );
private:
	bool run_chain( tokens_t const& );
	OSpawnResult run_pipe( tokens_t& );
	bool spawn( OCommand& );
	void resolve_aliases( tokens_t& );
	void substitute_variable( yaal::hcore::HString& );
	tokens_t explode( yaal::hcore::HString const& ) const;
	void denormalize( tokens_t& );
};
typedef yaal::hcore::HResource<HShell> shell_t;

}

#endif /* #ifndef HUGINN_SHELL_HXX_INCLUDED */

