/* Read huginn/LICENSE.md file for copyright and licensing information. */

#ifndef HUGINN_BRACEEXPANSION_HXX_INCLUDED
#define HUGINN_BRACEEXPANSION_HXX_INCLUDED 1

#include <yaal/hcore/hstring.hxx>
#include <yaal/hcore/harray.hxx>
#include <yaal/tools/stringalgo.hxx>

namespace huginn {

yaal::tools::string::tokens_t brace_expansion( yaal::hcore::HString const& );

}

#endif /* #ifndef HUGINN_BRACEEXPANSION_HXX_INCLUDED */

