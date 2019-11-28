/* Read huginn/LICENSE.md file for copyright and licensing information. */

#ifndef HUGINN_COLORIZE_HXX_INCLUDED
#define HUGINN_COLORIZE_HXX_INCLUDED 1

#include <yaal/hcore/hstring.hxx>
#include <yaal/hcore/harray.hxx>
#include <yaal/tools/color.hxx>
#include <yaal/tools/filesystem.hxx>

#include "setup.hxx"

namespace huginn {

enum class GROUP {
	KEYWORDS,
	BUILTINS,
	CLASSES,
	ENUMS,
	FIELDS,
	ARGUMENTS,
	GLOBALS,
	LITERALS,
	COMMENTS,
	IMPORT,
	OPERATORS,
	SWITCHES,
	ENVIRONMENT,
	PIPES,
	SUBSTITUTION,
	ESCAPE,
	PROMPT,
	PROMPT_MARK,
	HINT
};

typedef yaal::hcore::HArray<yaal::tools::COLOR::color_t> colors_t;
typedef yaal::hcore::HHashMap<GROUP, yaal::tools::COLOR::color_t> scheme_t;
class HShell;
class HSystemShell;

yaal::tools::COLOR::color_t file_color( yaal::tools::filesystem::path_t&&, HSystemShell const*, yaal::tools::COLOR::color_t = yaal::tools::COLOR::ATTR_DEFAULT );
yaal::hcore::HString colorize( yaal::hcore::HString const&, HShell const* = nullptr );
yaal::tools::COLOR::color_t color( GROUP );
char const* ansi_color( GROUP );
void colorize( yaal::hcore::HString const&, colors_t&, HShell const* = nullptr );
void set_color_scheme( yaal::hcore::HString const& );

yaal::hcore::HString colorize( yaal::tools::HHuginn::HCallSite const& );
yaal::hcore::HString colorize_error( yaal::hcore::HString const& );

}

#endif /* #ifndef HUGINN_COLORIZE_HXX_INCLUDED */

