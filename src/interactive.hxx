/* Read huginn/LICENSE.md file for copyright and licensing information. */

/*! \file interactive.hxx
 * \brief Declaration...
 */

#ifndef HUGINN_INTERACTIVE_HXX_INCLUDED
#define HUGINN_INTERACTIVE_HXX_INCLUDED 1

#include <yaal/hcore/hstring.hxx>

namespace huginn {

class HSystemShell;

int interactive_session( void );
void make_prompt( yaal::hcore::HString const&, char*, int, int, HSystemShell* );

}

#endif /* #ifndef HUGINN_INTERACTIVE_HXX_INCLUDED */

