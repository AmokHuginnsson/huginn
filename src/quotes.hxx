/* Read huginn/LICENSE.md file for copyright and licensing information. */

/*! \file quotes.hxx
 * \brief Declaration of help functions for hanglins string with quotes.
 */

#ifndef HUGINN_QUOTES_HXX_INCLUDED
#define HUGINN_QUOTES_HXX_INCLUDED 1

#include <yaal/hcore/hstring.hxx>
#include <yaal/tools/stringalgo.hxx>

namespace huginn {

bool in_quotes( yaal::hcore::HString const& );
yaal::tools::string::tokens_t split_quotes( yaal::hcore::HString const& );

}

#endif /* #ifndef HUGINN_QUOTES_HXX_INCLUDED */

