/* Read huginn/LICENSE.md file for copyright and licensing information. */

/*! \file systemshell.hxx
 * \brief Declaration of HSystemShell class.
 */

#ifndef HUGINN_SYSTEMSHELL_HXX_INCLUDED
#define HUGINN_SYSTEMSHELL_HXX_INCLUDED 1

#include <yaal/hcore/hstring.hxx>
#include <yaal/hcore/hpipe.hxx>
#include <yaal/tools/stringalgo.hxx>
#include <yaal/tools/filesystem.hxx>

#include "shell.hxx"
#include "linerunner.hxx"
#include "quotes.hxx"
#include "repl.hxx"

namespace huginn {

class HSystemShell : public HShell {
public:
	typedef yaal::tools::string::tokens_t tokens_t;
	typedef yaal::hcore::HResource<yaal::tools::HPipedChild> piped_child_t;
	typedef yaal::hcore::HPointer<yaal::hcore::HThread> thread_t;
	typedef void ( HSystemShell::* setopt_handler_t )( tokens_t& );
	struct OCommand;
	typedef yaal::hcore::HBoundCall<void ( OCommand& )> builtin_t;
	enum class EVALUATION_MODE {
		DIRECT,
		COMMAND_SUBSTITUTION,
		TRIAL
	};
	typedef yaal::hcore::HResource<OCommand> command_t;
	typedef yaal::hcore::HArray<command_t> commands_t;
	class HJob;
	class HCapture;
	typedef yaal::hcore::HPointer<HCapture> capture_t;
	struct OChain {
		tokens_t _tokens;
		bool _background;
		OChain( tokens_t&& tokens_ )
			: _tokens( yaal::move( tokens_ ) )
			, _background( false ) {
		}
	};
	typedef yaal::hcore::HResource<HJob> job_t;
	typedef yaal::hcore::HArray<job_t> jobs_t;
	typedef yaal::hcore::HStack<yaal::hcore::HString> substitutions_t;
	typedef yaal::hcore::HMap<yaal::hcore::HString, yaal::hcore::HString> system_commands_t;
	typedef yaal::hcore::HMap<yaal::hcore::HString, builtin_t> builtins_t;
	typedef yaal::hcore::HMap<yaal::hcore::HString, tokens_t> aliases_t;
	typedef yaal::hcore::HMap<yaal::hcore::HString, yaal::hcore::HString> key_bindings_t;
	typedef yaal::hcore::HHashMap<yaal::hcore::HString, setopt_handler_t> setopt_handlers_t;
	typedef yaal::hcore::HHashSet<yaal::tools::filesystem::path_t> actively_sourced_t;
	typedef yaal::hcore::HStack<yaal::tools::filesystem::path_t> actively_sourced_stack_t;
	typedef yaal::hcore::HArray<OChain> chains_t;
	typedef yaal::hcore::HStack<tokens_t> argvs_t;
private:
	HLineRunner& _lineRunner;
	HRepl& _repl;
	system_commands_t _systemCommands;
	system_commands_t _systemSuperUserCommands;
	builtins_t _builtins;
	aliases_t _aliases;
	key_bindings_t _keyBindings;
	setopt_handlers_t _setoptHandlers;
	yaal::tools::filesystem::paths_t _superUserPaths;
	yaal::tools::filesystem::paths_t _dirStack;
	substitutions_t _substitutions;
	yaal::hcore::HRegex _ignoredFiles;
	jobs_t _jobs;
	actively_sourced_t _activelySourced;
	actively_sourced_stack_t _activelySourcedStack;
	tokens_t _failureMessages;
	int _previousOwner;
	bool _background;
	bool _loaded;
	argvs_t _argvs;
public:
	HSystemShell( HLineRunner&, HRepl&, int = 0, char** = nullptr );
	~HSystemShell( void );
	system_commands_t const& system_commands( void ) const;
	aliases_t const& aliases( void ) const;
	builtins_t const& builtins( void ) const;
	HLineRunner& line_runner( void );
	bool loaded( void ) const;
	int job_count( void ) const;
	void session_start( void );
	void session_stop( void );
	bool has_huginn_jobs( void ) const;
	bool finalized( void );
	HRepl& repl( void ) const;
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
	void fg( OCommand& );
	void source( OCommand& );
	void eval( OCommand& );
	void exec [[noreturn]]( OCommand& );
	void exit( OCommand& );
	void help( OCommand& );
private:
	void source_global( char const* );
	HLineResult run_line( yaal::hcore::HString const&, EVALUATION_MODE );
	HLineResult run_chain( tokens_t const&, bool, capture_t const&, EVALUATION_MODE, bool );
	HLineResult run_pipe( tokens_t&, bool, capture_t const&, EVALUATION_MODE, bool, bool );
	bool spawn( OCommand&, int, bool, EVALUATION_MODE );
	void resolve_aliases( tokens_t& ) const;
	void resolve_string_aliases( tokens_t&, tokens_t::iterator ) const;
	void substitute_variable( yaal::hcore::HString& ) const;
	tokens_t denormalize( tokens_t const&, EVALUATION_MODE );
	tokens_t interpolate( yaal::hcore::HString const&, EVALUATION_MODE );
	bool is_command( yaal::hcore::HString const& );
	void attach_terminal( void );
	void load_init( void );
	void load_rc( void );
	void set_environment( void );
	void register_commands( void );
	void learn_system_commands( void );
	void learn_system_commands( system_commands_t&, yaal::tools::filesystem::paths_t const& );
	void run_bound( yaal::hcore::HString const& );
	int run_result( yaal::hcore::HString const& );
	bool has_command( yaal::hcore::HString const& ) const;
	enum class FILENAME_COMPLETIONS {
		FILE,
		DIRECTORY,
		EXECUTABLE
	};
	bool fallback_completions( tokens_t const&, yaal::hcore::HString const&, completions_t& ) const;
	void filename_completions( tokens_t const&, yaal::hcore::HString const&, FILENAME_COMPLETIONS, completions_t&, bool, bool ) const;
	void user_completions( yaal::tools::HHuginn::value_t const&, tokens_t const&, yaal::hcore::HString const&, completions_t& ) const;
	void completions_from_string( yaal::hcore::HString const&, tokens_t const&, yaal::hcore::HString const&, completions_t& ) const;
	void completions_from_commands( yaal::hcore::HString const&, yaal::hcore::HString const&, completions_t& ) const;
	void completions_from_su_commands( yaal::hcore::HString const&, yaal::hcore::HString const&, completions_t& ) const;
	bool is_prefix( yaal::hcore::HString const& ) const;
	void setopt_ignore_filenames( tokens_t& );
	void setopt_history_path( tokens_t& );
	void setopt_history_max_size( tokens_t& );
	void setopt_super_user_paths( tokens_t& );
	void cleanup_jobs( void );
private:
	virtual bool do_is_valid_command( yaal::hcore::HString const& ) override;
	virtual bool do_try_command( yaal::hcore::HString const& ) override;
	virtual HLineResult do_run( yaal::hcore::HString const& ) override;
	void flush_faliures( job_t const& );
	void substitute_from_shell( yaal::hcore::HString&, QUOTES ) const;
	bool substitute_arg_at( tokens_t&, yaal::hcore::HString&, yaal::hcore::HString& ) const;
	void substitute_command( yaal::hcore::HString& );
	virtual completions_t do_gen_completions( yaal::hcore::HString const&, yaal::hcore::HString const&, bool ) const override;
	void do_source( tokens_t const& );
	int get_job_no( char const*, OCommand&, bool );
	chains_t split_chains( yaal::hcore::HString const&, EVALUATION_MODE ) const;
	yaal::hcore::HString expand( yaal::hcore::HString&& );
	friend class HJob;
private:
	HSystemShell( HSystemShell const& ) = delete;
	HSystemShell& operator = ( HSystemShell const& ) = delete;
};

}

#endif /* #ifndef HUGINN_SYSTEMSHELL_HXX_INCLUDED */

