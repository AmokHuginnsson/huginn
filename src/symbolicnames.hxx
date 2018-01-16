/* Read huginn/LICENSE.md file for copyright and licensing information. */

#ifndef HUGINN_SYMBOLICNAMES_HXX_INCLUDED
#define HUGINN_SYMBOLICNAMES_HXX_INCLUDED 1

#include <yaal/hcore/hstring.hxx>
#include <yaal/hcore/harray.hxx>

namespace huginn {

typedef yaal::hcore::HArray<yaal::hcore::HString> symbolic_names_t;
char const* symbol_from_name( yaal::hcore::HString const& );
symbolic_names_t symbol_name_completions( yaal::hcore::HString const& );

}

#endif /* #ifndef HUGINN_SYMBOLICNAMES_HXX_INCLUDED */

