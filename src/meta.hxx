/* Read huginn/LICENSE.md file for copyright and licensing information. */

/*! \file meta.hxx
 * \brief Declaration `meta()' function.
 */

#ifndef HUGINN_META_HXX_INCLUDED
#define HUGINN_META_HXX_INCLUDED 1

#include "repl.hxx"

namespace huginn {

bool meta( HLineRunner&, yaal::hcore::HString const&, HRepl* = nullptr );
typedef yaal::hcore::HArray<yaal::hcore::HString> magic_names_t;
magic_names_t magic_names( void );
void banner( HRepl* );

}

#endif /* #ifndef HUGINN_META_HXX_INCLUDED */

