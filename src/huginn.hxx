/* Read huginn/LICENSE.md file for copyright and licensing information. */

#ifndef HUGINN_HUGINN_HXX_INCLUDED
#define HUGINN_HUGINN_HXX_INCLUDED 1

#include <yaal/tools/hhuginn.hxx>

namespace huginn {

int run_huginn( int, char** );
typedef yaal::hcore::HArray<char> buffer_t;
buffer_t load( char const*, int* = nullptr );

}

#endif /* #ifndef HUGINN_HUGINN_HXX_INCLUDED */

