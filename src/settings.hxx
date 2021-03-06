/* Read huginn/LICENSE.md file for copyright and licensing information. */

/*! \file settings.hxx
 * \brief Declaration of apply_setting() function.
 */

#ifndef HUGINN_SETTINGS_HXX_INCLUDED
#define HUGINN_SETTINGS_HXX_INCLUDED 1

#include <yaal/tools/hhuginn.hxx>
#include <yaal/tools/filesystem.hxx>

namespace huginn {

void apply_setting( yaal::tools::HHuginn&, yaal::hcore::HString const& );

struct OSettingObserver {
	int _maxCallStackSize;
	yaal::tools::HHuginn::paths_t _modulePath;
	yaal::hcore::HString _colorScheme;
	OSettingObserver( void );
} extern settingsObserver;

typedef yaal::hcore::HMap<yaal::hcore::HString, yaal::hcore::HString> rt_settings_t;

rt_settings_t rt_settings( bool = false );

yaal::tools::filesystem::path_t make_conf_path( char const* );

}

#endif /* #ifndef HUGINN_SETTINGS_HXX_INCLUDED */

