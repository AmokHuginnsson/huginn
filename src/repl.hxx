/* Read huginn/LICENSE.md file for copyright and licensing information. */

/*! \file repl.hxx
 * \brief Declaration of REPL forwarders.
 */

#ifndef HUHINN_REPL_HXX_INCLUDED
#define HUHINN_REPL_HXX_INCLUDED 1

#include <yaal/hcore/hstring.hxx>
#include <yaal/hcore/htuple.hxx>

#include "config.hxx"

#ifdef USE_REPLXX
#	include <replxx.hxx>
#elif defined( USE_EDITLINE )
#	include <histedit.h>
#endif

#include "linerunner.hxx"
#include "shell.hxx"

namespace huginn {

class HRepl {
public:
	typedef HLineRunner::words_t ( *completion_words_t )( yaal::hcore::HString&&, yaal::hcore::HString&&, void* );
	typedef yaal::hcore::HBoundCall<> action_t;
private:
#ifdef USE_REPLXX
	typedef yaal::hcore::HHashMap<yaal::hcore::HString, char32_t> key_table_t;
	replxx::Replxx _replxx;
	key_table_t _keyTable;
#elif defined( USE_EDITLINE )
	EditLine* _el;
	History* _hist;
	HistEvent _histEvent;
	int _count;
#endif
	HLineRunner* _lineRunner;
	HShell* _shell;
	char const* _prompt; /* Used by editline driver. */
	completion_words_t _completer;
	yaal::hcore::HString _historyPath;
public:
	HRepl( void );
	virtual ~HRepl( void );
	void set_shell( HShell* );
	void set_line_runner( HLineRunner* );
	void set_completer( completion_words_t );
	void set_history_path( yaal::hcore::HString const& );
	HLineRunner* line_runner( void ) const {
		return ( _lineRunner );
	}
	HShell* shell( void ) const {
		return ( _shell );
	}
	char const* prompt( void ) const {
		return ( _prompt );
	}
	bool input( yaal::hcore::HString&, char const* );
	void print( char const* );
	void bind_key( yaal::hcore::HString const&, action_t const& );
	HLineRunner::words_t completion_words( yaal::hcore::HString&&, yaal::hcore::HString&&, bool = true );
private:
#ifdef USE_REPLXX
	replxx::Replxx::ACTION_RESULT run_action( action_t, char32_t );
#endif
	HRepl( HRepl const& ) = delete;
	HRepl& operator = ( HRepl const& ) = delete;
};

}

#endif /* #ifndef HUHINN_REPL_HXX_INCLUDED */

