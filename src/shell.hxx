/* Read huginn/LICENSE.md file for copyright and licensing information. */

/*! \file shell.hxx
 * \brief Declaration of shell function.
 */

#ifndef HUGINN_SHELL_HXX_INCLUDED
#define HUGINN_SHELL_HXX_INCLUDED 1

#include <yaal/hcore/hstring.hxx>
#include "linerunner.hxx"

namespace huginn {

typedef yaal::hcore::HHashMap<yaal::hcore::HString, yaal::hcore::HString> system_commands_t;
system_commands_t get_system_commands( void );
bool shell( yaal::hcore::HString const&, HLineRunner&, system_commands_t const& );
HLineRunner::words_t filename_completions( yaal::hcore::HString const&, yaal::hcore::HString const& );

}

#endif /* #ifndef HUGINN_SHELL_HXX_INCLUDED */

