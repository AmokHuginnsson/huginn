/* Read huginn/LICENSE.md file for copyright and licensing information. */

/*! \file shell.hxx
 * \brief Declaration of shell function.
 */

#ifndef HUGINN_SHELL_HXX_INCLUDED
#define HUGINN_SHELL_HXX_INCLUDED 1

#include <yaal/hcore/hresource.hxx>
#include <yaal/hcore/hstring.hxx>
#include "linerunner.hxx"

namespace huginn {

class HShell {
public:
	typedef yaal::hcore::HHashMap<yaal::hcore::HString, yaal::hcore::HString> system_commands_t;
private:
	system_commands_t _systemCommands;
public:
	HShell( void );
	system_commands_t const& system_commands( void ) const;
	bool run( yaal::hcore::HString const&, HLineRunner& ) const;
	HLineRunner::words_t filename_completions( yaal::hcore::HString const&, yaal::hcore::HString const& ) const;
};
typedef yaal::hcore::HResource<HShell> shell_t;

}

#endif /* #ifndef HUGINN_SHELL_HXX_INCLUDED */

