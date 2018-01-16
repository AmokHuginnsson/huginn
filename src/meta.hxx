/* Read huginn/LICENSE.md file for copyright and licensing information. */

/*! \file meta.hxx
 * \brief Declaration `meta()' function.
 */

#ifndef HUGINN_META_HXX_INCLUDED
#define HUGINN_META_HXX_INCLUDED 1

#include "linerunner.hxx"

namespace huginn {

bool meta( HLineRunner&, yaal::hcore::HString const& );
typedef yaal::hcore::HArray<yaal::hcore::HString> magic_names_t;
magic_names_t magic_names( void );
void banner( void );

}

#endif /* #ifndef HUGINN_META_HXX_INCLUDED */

