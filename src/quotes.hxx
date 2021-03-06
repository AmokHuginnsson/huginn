/* Read huginn/LICENSE.md file for copyright and licensing information. */

/*! \file quotes.hxx
 * \brief Declaration of help functions for hanglins string with quotes.
 */

#ifndef HUGINN_QUOTES_HXX_INCLUDED
#define HUGINN_QUOTES_HXX_INCLUDED 1

#include <yaal/hcore/hstring.hxx>
#include <yaal/tools/stringalgo.hxx>
#include <yaal/tools/filesystem.hxx>

namespace huginn {

enum class QUOTES {
	NONE,
	SINGLE,
	DOUBLE,
	EXEC,
	EXEC_SOURCE,
	EXEC_SINK
};

enum class REDIR {
	NONE,
	IN,
	OUT,
	ERR,
	OUT_ERR,
	APP_OUT,
	APP_ERR,
	APP_OUT_ERR,
	ERR_OUT,
	PIPE,
	PIPE_ERR
};

extern yaal::code_point_t PATH_SEP;
extern char const PATH_ENV_SEP[];
extern char const REDIR_IN[];
extern char const REDIR_OUT[];
extern char const REDIR_ERR[];
extern char const REDIR_OUT_ERR[];
extern char const REDIR_APP_OUT[];
extern char const REDIR_APP_ERR[];
extern char const REDIR_APP_OUT_ERR[];
extern char const REDIR_ERR_OUT[];
extern char const SHELL_AND[];
extern char const SHELL_OR[];
extern char const SHELL_PIPE[];
extern char const SHELL_PIPE_ERR[];
extern char const SHELL_TERM[];
extern char const SHELL_BG[];

class HQuoteObserver {
	bool _evalDoubleQuotes;
	bool _escaped;
	bool _inSingleQuotes;
	bool _inDoubleQuotes;
public:
	HQuoteObserver( bool = false );
	bool notice( yaal::code_point_t );
	void reset( void );
};

bool in_quotes( yaal::hcore::HString const& );
bool is_shell_token( yaal::hcore::HString const& );
void strip_quotes( yaal::hcore::HString& );
QUOTES str_to_quotes( yaal::hcore::HString const& );
REDIR str_to_redir( yaal::hcore::HString const& );
yaal::tools::string::tokens_t tokenize_shell( yaal::hcore::HString const& );
yaal::tools::string::tokens_t tokenize_quotes( yaal::hcore::HString const& );
yaal::tools::string::tokens_t split_quoted( yaal::hcore::HString const& );
yaal::hcore::HString unescape_huginn_code( yaal::hcore::HString const& );
yaal::hcore::HString&& unescape_system( yaal::hcore::HString&& );
void denormalize_path( yaal::tools::filesystem::path_t&, bool = false );

}

#endif /* #ifndef HUGINN_QUOTES_HXX_INCLUDED */

