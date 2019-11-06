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
	typedef void ( HSystemShell::* setopt_handler_t )( tokens_t& );
	struct OCommand;
	typedef yaal::hcore::HBoundCall<void ( OCommand& )> builtin_t;
	enum class EVALUATION_MODE {
		DIRECT,
		COMMAND_SUBSTITUTION,
		TRIAL
	};
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
		yaal::hcore::HStreamInterface::ptr_t _err;
		tokens_t _tokens;
		thread_t _thread;
		piped_child_t _child;
		yaal::hcore::HPipe::ptr_t _pipe;
		yaal::tools::HPipedChild::STATUS _status;
		OCommand( void )
			: _in()
			, _out()
			, _err()
			, _tokens()
			, _thread()
			, _child()
			, _pipe()
			, _status() {
		}
		template<typename T>
		yaal::hcore::HStreamInterface& operator << ( T const& val_ ) {
			yaal::hcore::HStreamInterface* s( !! _out ? _out.raw() : &yaal::hcore::cout );
			*s << val_;
			return ( *s );
		}
		void run_huginn( HLineRunner& );
		void run_builtin( builtin_t const& );
		yaal::tools::HPipedChild::STATUS finish( void );
	};
	typedef yaal::hcore::HArray<OCommand> commands_t;
	class HJob {
	private:
		yaal::hcore::HString _description;
		commands_t _commands;
		int _leader;
		EVALUATION_MODE _evaluationMode;
		yaal::hcore::HResource<yaal::hcore::HPipe> _capturePipe;
		yaal::hcore::HResource<yaal::hcore::HThread> _captureThread;
		yaal::hcore::HString _captureBuffer;
	public:
		HJob( commands_t&&, EVALUATION_MODE );
		OSpawnResult run( HSystemShell& );
		yaal::hcore::HString const& output( void ) const {
			return ( _captureBuffer );
		}
		yaal::hcore::HString const& desciption( void ) const {
			return ( _description );
		}
		int leader( void ) const {
			return ( _leader );
		}
		yaal::tools::HPipedChild::STATUS const& status( void );
		bool is_system_command( void ) const;
	private:
		void stop_capture( void );
		yaal::hcore::HString make_desc( commands_t const& ) const;
	};
	typedef yaal::hcore::HResource<HJob> job_t;
	typedef yaal::hcore::HArray<job_t> jobs_t;
	typedef yaal::hcore::HStack<yaal::hcore::HString> substitutions_t;
	typedef yaal::hcore::HMap<yaal::hcore::HString, yaal::hcore::HString> system_commands_t;
	typedef yaal::hcore::HMap<yaal::hcore::HString, builtin_t> builtins_t;
	typedef yaal::hcore::HMap<yaal::hcore::HString, tokens_t> aliases_t;
	typedef yaal::hcore::HMap<yaal::hcore::HString, yaal::hcore::HString> key_bindings_t;
	typedef yaal::hcore::HHashMap<yaal::hcore::HString, setopt_handler_t> setopt_handlers_t;
	typedef yaal::hcore::HArray<yaal::tools::filesystem::path_t> dir_stack_t;
	typedef yaal::hcore::HArray<tokens_t> chains_t;
private:
	HLineRunner& _lineRunner;
	HRepl& _repl;
	system_commands_t _systemCommands;
	builtins_t _builtins;
	aliases_t _aliases;
	key_bindings_t _keyBindings;
	setopt_handlers_t _setoptHandlers;
	dir_stack_t _dirStack;
	substitutions_t _substitutions;
	yaal::hcore::HRegex _ignoredFiles;
	bool _loaded;
	jobs_t _jobs;
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
	void setopt( OCommand& );
	void history( OCommand& );
	void jobs( OCommand& );
	void bg( OCommand& );
private:
	void load_init( void );
	bool run_line( yaal::hcore::HString const&, EVALUATION_MODE );
	bool run_chain( tokens_t const&, EVALUATION_MODE );
	OSpawnResult run_pipe( tokens_t&, EVALUATION_MODE );
	bool spawn( OCommand&, int, bool, EVALUATION_MODE );
	void resolve_aliases( tokens_t& );
	void substitute_variable( yaal::hcore::HString& ) const;
	tokens_t denormalize( tokens_t const&, EVALUATION_MODE );
	tokens_t interpolate( yaal::hcore::HString const&, EVALUATION_MODE );
	bool is_command( yaal::hcore::HString const& );
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
	void setopt_ignore_filenames( tokens_t& );
	void cleanup_jobs( void );
private:
	virtual bool do_is_valid_command( yaal::hcore::HString const& ) override;
	virtual bool do_try_command( yaal::hcore::HString const& ) override;
	virtual bool do_run( yaal::hcore::HString const& ) override;
	virtual completions_t do_gen_completions( yaal::hcore::HString const&, yaal::hcore::HString const& ) const override;
	friend OSpawnResult HJob::run( HSystemShell& );
};

}

#endif /* #ifndef HUGINN_SYSTEMSHELL_HXX_INCLUDED */

