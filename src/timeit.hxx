/* Read huginn/LICENSE.md file for copyright and licensing information. */

#ifndef HUGINN_TIMEIT_HXX_INCLUDED
#define HUGINN_TIMEIT_HXX_INCLUDED 1

#include <yaal/hcore/duration.hxx>
#include <yaal/tools/hhuginn.hxx>

namespace huginn {

bool timeit( yaal::tools::HHuginn&, yaal::hcore::time::duration_t&, int& );

void report_timeit(
	yaal::hcore::time::duration_t const&,
	yaal::hcore::time::duration_t const&,
	yaal::hcore::time::duration_t const&,
	yaal::hcore::time::duration_t const&,
	yaal::hcore::time::duration_t const&,
	yaal::hcore::time::duration_t const&,
	yaal::hcore::time::duration_t const&,
	int
);

}

#endif /* #ifndef HUGINN_TIMEIT_HXX_INCLUDED */

