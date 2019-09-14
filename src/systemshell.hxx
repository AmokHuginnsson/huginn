/* Read huginn/LICENSE.md file for copyright and licensing information. */

/*! \file systemshell.hxx
 * \brief Declaration of HSystemShell class.
 */

#ifndef HUGINN_SYSTEMSHELL_HXX_INCLUDED
#define HUGINN_SYSTEMSHELL_HXX_INCLUDED 1

#include <yaal/hcore/hstring.hxx>
#include <yaal/hcore/hfile.hxx>
#include <yaal/hcore/hpipe.hxx>
#include <yaal/tools/stringalgo.hxx>
#include <yaal/tools/hpipedchild.hxx>
#include <yaal/tools/filesystem.hxx>

#include "shell.hxx"
#include "linerunner.hxx"
#include "repl.hxx"

namespace huginn {

class HSystemShell : public HShell {
public:
	typedef yaal::tools::string::tokens_t tokens_t;
	typedef yaal::hcore::HPointer<yaal::tools::HPipedChild> piped_child_t;
	typedef yaal::hcore::HPointer<yaal::hcore::HThread> thread_t;
	struct OSpawnResult {
		yaal::tools::HPipedChild::STATUS _exitStatus;
		bool _validShell;
		explicit OSpawnResult( yaal::tools::HPipedChild::STATUS exitStatus_ = yaal::tools::HPipedChild::STATUS(), bool validShell_ = false )
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
	typedef yaal::hcore::HMap<yaal::hcore::HString, yaal::hcore::HString> system_commands_t;
	typedef yaal::hcore::HBoundCall<void ( OCommand& )> builtin_t;
	typedef yaal::hcore::HMap<yaal::hcore::HString, builtin_t> builtins_t;
	typedef yaal::hcore::HMap<yaal::hcore::HString, tokens_t> aliases_t;
	typedef yaal::hcore::HArray<yaal::tools::filesystem::path_t> dir_stack_t;
	typedef yaal::hcore::HArray<tokens_t> chains_t;
private:
	HLineRunner& _lineRunner;
	HRepl& _repl;
	system_commands_t _systemCommands;
	builtins_t _builtins;
	aliases_t _aliases;
	dir_stack_t _dirStack;
public:
	HSystemShell( HLineRunner&, HRepl& );
	system_commands_t const& system_commands( void ) const;
	aliases_t const& aliases( void ) const;
	builtins_t const& builtins( void ) const;
private:
	void alias( OCommand& );
	void unalias( OCommand& );
	void cd( OCommand& );
	void setenv( OCommand& );
	void unsetenv( OCommand& );
	void bind_key( OCommand& );
	void dir_stack( OCommand& );
	void rehash( OCommand& );
private:
	void load_init( void );
	bool run_line( yaal::hcore::HString const& );
	bool run_chain( tokens_t const& );
	OSpawnResult run_pipe( tokens_t& );
	bool spawn( OCommand&, int, bool );
	void resolve_aliases( tokens_t& );
	void substitute_variable( yaal::hcore::HString& ) const;
	tokens_t explode( yaal::hcore::HString const& ) const;
	tokens_t denormalize( tokens_t const& ) const;
	bool is_command( yaal::hcore::HString const& ) const;
	void run_huginn( void );
	void learn_system_commands( void );
	void run_bound( yaal::hcore::HString const& );
	enum class FILENAME_COMPLETIONS {
		FILE,
		DIRECTORY,
		EXECUTABLE
	};
	bool fallback_completions( tokens_t const&, completions_t& ) const;
	void filename_completions( tokens_t const&, yaal::hcore::HString const&, FILENAME_COMPLETIONS, completions_t&, bool = false ) const;
	void user_completions( yaal::tools::HHuginn::value_t const&, tokens_t const&, yaal::hcore::HString const&, completions_t& ) const;
	bool is_prefix( yaal::hcore::HString const& ) const;
private:
	virtual bool do_is_valid_command( yaal::hcore::HString const& ) const override;
	virtual bool do_try_command( yaal::hcore::HString const& ) override;
	virtual bool do_run( yaal::hcore::HString const& ) override;
	virtual completions_t do_gen_completions( yaal::hcore::HString const&, yaal::hcore::HString const& ) const override;
};

}

#endif /* #ifndef HUGINN_SYSTEMSHELL_HXX_INCLUDED */

