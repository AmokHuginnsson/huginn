/* Read huginn/LICENSE.md file for copyright and licensing information. */

/*! \file pager.hxx
 * \brief Declaration pager() function.
 */

#ifndef HUGINN_PAGER_HXX_INCLUDED
#define HUGINN_PAGER_HXX_INCLUDED 1

#include <yaal/hcore/hstring.hxx>

namespace huginn {

enum class PAGER_MODE {
	KEEP_GOING,
	EXIT,
};
void pager( yaal::hcore::HString const&, PAGER_MODE = PAGER_MODE::KEEP_GOING );

}

#endif /* #ifndef HUGINN_PAGER_HXX_INCLUDED */

