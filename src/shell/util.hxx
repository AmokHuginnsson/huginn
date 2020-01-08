#ifndef HUGINN_SHELL_UTIL_INCLUDED
#define HUGINN_SHELL_UTIL_INCLUDED 1

#include <yaal/tools/stringalgo.hxx>
#include <yaal/tools/filesystem.hxx>

namespace huginn {

yaal::hcore::HString stringify_command( yaal::tools::string::tokens_t const&, int = 0 );
yaal::tools::filesystem::path_t compact_path( yaal::tools::filesystem::path_t const& );
bool is_suid( yaal::tools::filesystem::path_t const& );

}

#endif /* #ifndef HUGINN_SHELL_UTIL_INCLUDED */

