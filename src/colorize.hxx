/* Read huginn/LICENSE.md file for copyright and licensing information. */

#ifndef HUGINN_COLORIZE_HXX_INCLUDED
#define HUGINN_COLORIZE_HXX_INCLUDED 1

#include <yaal/hcore/hstring.hxx>
#include <yaal/hcore/harray.hxx>
#include <yaal/tools/color.hxx>

#include "setup.hxx"

namespace huginn {

enum class GROUP {
	KEYWORDS,
	BUILTINS,
	CLASSES,
	ENUMS,
	FIELDS,
	ARGUMENTS,
	LITERALS,
	COMMENTS,
	IMPORT,
	OPERATORS,
	SWITCHES,
	ENVIRONMENT,
	PIPES,
	ESCAPE,
	PROMPT,
	PROMPT_MARK,
	HINT
};

typedef yaal::hcore::HArray<yaal::tools::COLOR::color_t> colors_t;
typedef yaal::hcore::HHashMap<GROUP, yaal::tools::COLOR::color_t> scheme_t;

yaal::hcore::HString colorize( yaal::hcore::HUTF8String const& );
yaal::tools::COLOR::color_t color( GROUP );
char const* ansi_color( GROUP );
void colorize( yaal::hcore::HUTF8String const&, colors_t& );
void shell_colorize( yaal::hcore::HUTF8String const&, colors_t& );
void set_color_scheme( yaal::hcore::HString const& );

}

#endif /* #ifndef HUGINN_COLORIZE_HXX_INCLUDED */

