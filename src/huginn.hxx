/* Read huginn/LICENSE.md file for copyright and licensing information. */

#ifndef HUGINN_HUGINN_HXX_INCLUDED
#define HUGINN_HUGINN_HXX_INCLUDED 1

#include <yaal/tools/hhuginn.hxx>

namespace huginn {

int run_huginn( int, char** );
yaal::tools::HHuginn::ptr_t load( yaal::hcore::HString const& );
yaal::tools::HHuginn::ptr_t load( yaal::hcore::HString const&, yaal::hcore::HChunk&, int& );

}

#endif /* #ifndef HUGINN_HUGINN_HXX_INCLUDED */

