/* Read huginn/LICENSE.md file for copyright and licensing information. */

#ifndef HUGINN_OPTIONS_HXX_INCLUDED
#define HUGINN_OPTIONS_HXX_INCLUDED 1

#include <yaal/hcore/algorithm.hxx>

namespace huginn {

template<typename T>
void arrange( T& col_ ) {
	yaal::sort( col_.begin(), col_.end() );
	col_.erase( yaal::unique( col_.begin(), col_.end() ), col_.end() );
}

template<typename T>
void concat( T& col_, T const& tail_ ) {
	col_.insert( col_.end(), tail_.begin(), tail_.end() );
}

}

#endif /* #ifndef HUGINN_OPTIONS_HXX_INCLUDED */

