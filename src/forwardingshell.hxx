/* Read huginn/LICENSE.md file for copyright and licensing information. */

/*! \file forwardingshell.hxx
 * \brief Declaration of HForwardingShell class.
 */

#ifndef HUGINN_FORWARDINGSHELL_HXX_INCLUDED
#define HUGINN_FORWARDINGSHELL_HXX_INCLUDED 1

#include "shell.hxx"

namespace huginn {

class HForwardingShell : public HShell {
private:
	virtual bool do_is_valid_command( yaal::hcore::HString const& ) override {
		return ( false );
	}
	virtual bool do_try_command( yaal::hcore::HString const& ) override;
	virtual HLineResult do_run( yaal::hcore::HString const& ) override {
		return ( HLineResult( true ) );
	}
	virtual completions_t do_gen_completions( yaal::hcore::HString const&, yaal::hcore::HString const& ) const override {
		return ( completions_t() );
	}
};

}

#endif /* #ifndef HUGINN_FORWARDINGSHELL_HXX_INCLUDED */

