/* Read huginn/LICENSE.md file for copyright and licensing information. */

/*! \file shell.hxx
 * \brief Declaration of HShell class.
 */

#ifndef HUGINN_SHELL_HXX_INCLUDED
#define HUGINN_SHELL_HXX_INCLUDED 1

#include <yaal/hcore/hresource.hxx>
#include <yaal/hcore/hstring.hxx>
#include <yaal/hcore/harray.hxx>

#include "repl.hxx"

namespace huginn {

class HShell {
public:
	typedef HRepl::completions_t completions_t;
public:
	virtual ~HShell( void ) {}
	bool is_valid_command( yaal::hcore::HString const& command_ ) const {
		return ( do_is_valid_command( command_ ) );
	}
	bool try_command( yaal::hcore::HString const& command_ ) {
		return ( do_try_command( command_ ) );
	}
	bool run( yaal::hcore::HString const& command_ ) {
		return ( do_run( command_ ) );
	}
	completions_t gen_completions( yaal::hcore::HString const& context_, yaal::hcore::HString const& prefix_ ) const {
		return ( do_gen_completions( context_, prefix_ ) );
	}
private:
	virtual bool do_is_valid_command( yaal::hcore::HString const& ) const = 0;
	virtual bool do_try_command( yaal::hcore::HString const& ) = 0;
	virtual bool do_run( yaal::hcore::HString const& ) = 0;
	virtual completions_t do_gen_completions( yaal::hcore::HString const&, yaal::hcore::HString const& ) const = 0;
};

typedef yaal::hcore::HResource<HShell> shell_t;

}

#endif /* #ifndef HUGINN_SHELL_HXX_INCLUDED */

