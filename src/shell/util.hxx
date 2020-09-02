#ifndef HUGINN_SHELL_UTIL_HXX_INCLUDED
#define HUGINN_SHELL_UTIL_HXX_INCLUDED 1

#include <yaal/tools/stringalgo.hxx>
#include <yaal/tools/filesystem.hxx>

namespace huginn {

extern char const ARG_AT[];
yaal::hcore::HString stringify_command( yaal::tools::string::tokens_t const&, int = 0 );
yaal::tools::filesystem::path_t compact_path( yaal::tools::filesystem::path_t const& );
yaal::tools::filesystem::path_t escape_path( yaal::tools::filesystem::path_t const& );
bool is_suid( yaal::tools::filesystem::path_t const& );
void apply_glob( yaal::tools::string::tokens_t&, yaal::hcore::HString&&, bool );

}

#endif /* #ifndef HUGINN_SHELL_UTIL_HXX_INCLUDED */

