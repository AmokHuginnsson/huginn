#ifndef HUGINN_SHELL_JOB_HXX_INCLUDED
#define HUGINN_SHELL_JOB_HXX_INCLUDED 1

#include <yaal/tools/util.hxx>

#include "src/systemshell.hxx"
#include "src/shell/capture.hxx"

namespace huginn {

class HSystemShell::HJob {
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
	HSystemShell::capture_t _capture;
	yaal::tools::util::HScopeExitCall _sec;
public:
	HJob( HSystemShell&, commands_t&&, HSystemShell::capture_t const&, EVALUATION_MODE, bool, bool );
	bool start( bool );
	yaal::tools::HPipedChild::STATUS wait_for_finish( void );
	yaal::hcore::HString const& desciption( void ) const {
		return ( _description );
	}
	int leader( void ) const {
		return ( _leader );
	}
	bool in_background( void ) {
		return ( _background );
	}
	bool is_direct_evaluation( void ) {
		return ( _evaluationMode == EVALUATION_MODE::DIRECT );
	}
	yaal::tools::HPipedChild::STATUS const& status( void );
	void do_continue( bool );
	void bring_to_foreground( void );
	bool has_huginn_jobs( void ) const;
	bool can_orphan( void );
	void orphan( void );
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

}

#endif /* #ifndef HUGINN_SHELL_JOB_HXX_INCLUDED */

