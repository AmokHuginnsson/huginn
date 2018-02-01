/* Read huginn/LICENSE.md file for copyright and licensing information. */

/*! \file shell.hxx
 * \brief Declaration of shell function.
 */

#ifndef HUGINN_SHELL_HXX_INCLUDED
#define HUGINN_SHELL_HXX_INCLUDED 1

#include <yaal/hcore/hresource.hxx>
#include <yaal/hcore/hstring.hxx>
#include <yaal/tools/stringalgo.hxx>

#include "linerunner.hxx"

namespace huginn {

class HShell {
public:
	typedef yaal::tools::string::tokens_t tokens_t;
	typedef yaal::hcore::HHashMap<yaal::hcore::HString, yaal::hcore::HString> system_commands_t;
	typedef yaal::hcore::HBoundCall<void ( tokens_t const& )> builtin_t;
	typedef yaal::hcore::HHashMap<yaal::hcore::HString, builtin_t> builtins_t;
	typedef yaal::hcore::HHashMap<yaal::hcore::HString, tokens_t> aliases_t;
private:
	system_commands_t _systemCommands;
	builtins_t _builtins;
	aliases_t _aliases;
public:
	HShell( void );
	system_commands_t const& system_commands( void ) const;
	bool run( yaal::hcore::HString const&, HLineRunner& ) const;
	HLineRunner::words_t filename_completions( yaal::hcore::HString const&, yaal::hcore::HString const& ) const;
private:
	void alias( tokens_t const& );
	void cd( tokens_t const& );
};
typedef yaal::hcore::HResource<HShell> shell_t;

}

#endif /* #ifndef HUGINN_SHELL_HXX_INCLUDED */

