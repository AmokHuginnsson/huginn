/* Read huginn/LICENSE.md file for copyright and licensing information. */

#ifndef HUGINN_COLORIZE_HXX_INCLUDED
#define HUGINN_COLORIZE_HXX_INCLUDED 1

#include <yaal/hcore/hstring.hxx>
#include <yaal/hcore/harray.hxx>
#include <yaal/tools/color.hxx>

#include "setup.hxx"

namespace huginn {

typedef yaal::hcore::HArray<yaal::tools::COLOR::color_t> colors_t;
typedef yaal::hcore::HHashMap<yaal::hcore::HString, yaal::tools::COLOR::color_t> scheme_t;

yaal::hcore::HString colorize( yaal::hcore::HUTF8String const& );
void colorize( yaal::hcore::HUTF8String const&, colors_t& );
void set_color_scheme( BACKGROUND );

}

#endif /* #ifndef HUGINN_COLORIZE_HXX_INCLUDED */

