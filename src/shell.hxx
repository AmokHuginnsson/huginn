/* Read huginn/LICENSE.md file for copyright and licensing information. */

/*! \file shell.hxx
 * \brief Declaration of HShell class.
 */

#ifndef HUGINN_SHELL_HXX_INCLUDED
#define HUGINN_SHELL_HXX_INCLUDED 1

#include <yaal/hcore/hresource.hxx>
#include <yaal/hcore/hstring.hxx>
#include <yaal/hcore/harray.hxx>
#include <yaal/tools/filesystem.hxx>
#include <yaal/tools/hpipedchild.hxx>

#include "repl.hxx"

namespace huginn {

class HShell {
public:
	class HLineResult {
		yaal::tools::HPipedChild::STATUS _exitStatus;
		bool _validShell;
	public:
		explicit HLineResult( bool validShell_ = false, yaal::tools::HPipedChild::STATUS exitStatus_ = yaal::tools::HPipedChild::STATUS() )
			: _exitStatus( exitStatus_ )
			, _validShell( validShell_ ) {
		}
		bool valid_shell( void ) const {
			return ( _validShell );
		}
		yaal::tools::HPipedChild::STATUS exit_status( void ) const {
			return ( _exitStatus );
		}
	};
	typedef HRepl::completions_t completions_t;
public:
	virtual ~HShell( void ) {}
	bool is_valid_command( yaal::hcore::HString const& command_ ) {
		return ( do_is_valid_command( command_ ) );
	}
	bool try_command( yaal::hcore::HString const& command_ ) {
		return ( do_try_command( command_ ) );
	}
	HLineResult run( yaal::hcore::HString const& command_ ) {
		return ( do_run( command_ ) );
	}
	completions_t gen_completions( yaal::hcore::HString const& context_, yaal::hcore::HString const& prefix_, bool hints_ ) const {
		return ( do_gen_completions( context_, prefix_, hints_ ) );
	}
private:
	virtual bool do_is_valid_command( yaal::hcore::HString const& ) = 0;
	virtual bool do_try_command( yaal::hcore::HString const& ) = 0;
	virtual HLineResult do_run( yaal::hcore::HString const& ) = 0;
	virtual completions_t do_gen_completions( yaal::hcore::HString const&, yaal::hcore::HString const&, bool ) const = 0;
};

typedef yaal::hcore::HResource<HShell> shell_t;

}

#endif /* #ifndef HUGINN_SHELL_HXX_INCLUDED */

