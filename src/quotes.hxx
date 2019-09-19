/* Read huginn/LICENSE.md file for copyright and licensing information. */

/*! \file quotes.hxx
 * \brief Declaration of help functions for hanglins string with quotes.
 */

#ifndef HUGINN_QUOTES_HXX_INCLUDED
#define HUGINN_QUOTES_HXX_INCLUDED 1

#include <yaal/hcore/hstring.hxx>
#include <yaal/tools/stringalgo.hxx>

namespace huginn {

enum class QUOTES {
	NONE,
	SINGLE,
	DOUBLE
};

enum class REDIR {
	NONE,
	IN,
	OUT,
	OUT_ERR,
	APP,
	APP_ERR,
	PIPE,
	PIPE_ERR,
	PIPE_END
};

extern yaal::code_point_t PATH_SEP;
extern char const PATH_ENV_SEP[];
extern char const SHELL_AND[];
extern char const SHELL_OR[];
extern char const SHELL_PIPE[];

bool in_quotes( yaal::hcore::HString const& );
void strip_quotes( yaal::hcore::HString& );
QUOTES str_to_quotes( yaal::hcore::HString const& );
REDIR str_to_redir( yaal::hcore::HString const& );
yaal::tools::string::tokens_t tokenize_shell( yaal::hcore::HString const& );
yaal::hcore::HString unescape_huginn_code( yaal::hcore::HString const& );

}

#endif /* #ifndef HUGINN_QUOTES_HXX_INCLUDED */

