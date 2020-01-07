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
#include <yaal/tools/filesystem.hxx>
#include <yaal/tools/util.hxx>

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
	struct OCommand {
		HSystemShell& _systemShell;
		yaal::hcore::HStreamInterface::ptr_t _in;
		yaal::hcore::HStreamInterface::ptr_t _out;
		yaal::hcore::HStreamInterface::ptr_t _err;
		tokens_t _tokens;
		thread_t _thread;
		piped_child_t _child;
		yaal::hcore::HPipe::ptr_t _pipe;
		bool _isSystemCommand;
		bool _huginnExecuted;
		yaal::tools::HPipedChild::STATUS _status;
		yaal::hcore::HString _failureMessage;
		OCommand( HSystemShell& systemShell_ )
			: _systemShell( systemShell_ )
			, _in()
			, _out()
			, _err()
			, _tokens()
			, _thread()
			, _child()
			, _pipe()
			, _isSystemCommand( false )
			, _huginnExecuted( false )
			, _status()
			, _failureMessage() {
		}
		template<typename T>
		yaal::hcore::HStreamInterface& operator << ( T const& val_ ) {
			yaal::hcore::HStreamInterface* s( !! _out ? _out.raw() : &yaal::hcore::cout );
			*s << val_;
			return ( *s );
		}
		bool compile( EVALUATION_MODE );
		bool spawn( int, bool, bool );
		bool spawn_huginn( bool );
		void run_huginn( HLineRunner& );
		void run_builtin( builtin_t const& );
		yaal::tools::HPipedChild::STATUS finish( bool );
		bool is_system_command( void ) const;
		yaal::hcore::HString const& failure_message( void ) const;
	private:
		yaal::tools::HPipedChild::STATUS do_finish( void );
	};
	typedef yaal::hcore::HResource<OCommand> command_t;
	typedef yaal::hcore::HArray<command_t> commands_t;
	class HJob {
	private:
		HSystemShell& _systemShell;
		yaal::hcore::HString _description;
		commands_t _commands;
		int _leader;
		bool _background;
		EVALUATION_MODE _evaluationMode;
		bool _predecessor;
		bool _lastChain;
		tokens_t _failureMessages;
		yaal::hcore::HResource<yaal::hcore::HPipe> _capturePipe;
		yaal::hcore::HResource<yaal::hcore::HThread> _captureThread;
		yaal::hcore::HString _captureBuffer;
		yaal::tools::util::HScopeExitCall _sec;
	public:
		HJob( HSystemShell&, commands_t&&, EVALUATION_MODE, bool, bool );
		bool start( bool );
		yaal::tools::HPipedChild::STATUS wait_for_finish( void );
		yaal::hcore::HString const& output( void ) const {
			return ( _captureBuffer );
		}
		yaal::hcore::HString const& desciption( void ) const {
			return ( _description );
		}
		int leader( void ) const {
			return ( _leader );
		}
		bool in_background( void ) {
			return ( _background );
		}
		yaal::tools::HPipedChild::STATUS const& status( void );
		bool is_system_command( void ) const;
		void do_continue( bool );
		void bring_to_foreground( void );
		tokens_t const& failure_messages( void ) const {
			return ( _failureMessages );
		}
	private:
		void stop_capture( void );
		yaal::hcore::HString make_desc( commands_t const& ) const;
		yaal::tools::HPipedChild::process_group_t process_group( void );
		yaal::tools::HPipedChild::STATUS finish_non_process( commands_t::iterator, yaal::tools::HPipedChild::STATUS = yaal::tools::HPipedChild::STATUS() );
		yaal::tools::HPipedChild::STATUS gather_results( command_t& );
		commands_t::iterator process_to_command( yaal::tools::HPipedChild const* );
		HJob( HJob const& ) = delete;
		HJob& operator = ( HJob const& ) = delete;
	};
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
	typedef yaal::hcore::HArray<OChain> chains_t;
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
	tokens_t _failureMessages;
	int _previousOwner;
	bool _background;
	bool _loaded;
public:
	HSystemShell( HLineRunner&, HRepl& );
	~HSystemShell( void );
	system_commands_t const& system_commands( void ) const;
	aliases_t const& aliases( void ) const;
	builtins_t const& builtins( void ) const;
	HLineRunner& line_runner( void );
	bool loaded( void ) const;
	int job_count( void ) const;
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
	HLineResult run_chain( tokens_t const&, bool, EVALUATION_MODE, bool );
	HLineResult run_pipe( tokens_t&, bool, EVALUATION_MODE, bool, bool );
	bool spawn( OCommand&, int, bool, EVALUATION_MODE );
	void resolve_aliases( tokens_t& ) const;
	void resolve_string_aliases( tokens_t&, tokens_t::iterator ) const;
	void substitute_variable( yaal::hcore::HString& ) const;
	tokens_t denormalize( tokens_t const&, EVALUATION_MODE );
	tokens_t interpolate( yaal::hcore::HString const&, EVALUATION_MODE );
	bool is_command( yaal::hcore::HString const& );
	void learn_system_commands( void );
	void learn_system_commands( system_commands_t&, yaal::tools::filesystem::paths_t const& );
	void run_bound( yaal::hcore::HString const& );
	int run_result( yaal::hcore::HString const& );
	enum class FILENAME_COMPLETIONS {
		FILE,
		DIRECTORY,
		EXECUTABLE
	};
	bool fallback_completions( tokens_t const&, yaal::hcore::HString const&, completions_t& ) const;
	void filename_completions( tokens_t const&, yaal::hcore::HString const&, FILENAME_COMPLETIONS, completions_t&, bool, bool ) const;
	void user_completions( yaal::tools::HHuginn::value_t const&, tokens_t const&, yaal::hcore::HString const&, completions_t& ) const;
	void completions_from_string( yaal::hcore::HString const&, tokens_t const&, yaal::hcore::HString const&, completions_t& ) const;
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
	virtual completions_t do_gen_completions( yaal::hcore::HString const&, yaal::hcore::HString const& ) const override;
	void do_source( yaal::hcore::HString const& );
	int get_job_no( char const*, OCommand&, bool );
	chains_t split_chains( yaal::hcore::HString const&, EVALUATION_MODE ) const;
	yaal::hcore::HString expand( yaal::hcore::HString&& );
	friend yaal::tools::HPipedChild::STATUS HJob::wait_for_finish( void );
};

}

#endif /* #ifndef HUGINN_SYSTEMSHELL_HXX_INCLUDED */

