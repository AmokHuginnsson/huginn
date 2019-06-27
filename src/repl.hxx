/* Read huginn/LICENSE.md file for copyright and licensing information. */

/*! \file repl.hxx
 * \brief Declaration of REPL forwarders.
 */

#ifndef HUHINN_REPL_HXX_INCLUDED
#define HUHINN_REPL_HXX_INCLUDED 1

#include <yaal/hcore/hstring.hxx>
#include <yaal/hcore/htuple.hxx>
#include <yaal/tools/color.hxx>

#include "config.hxx"

#ifdef USE_REPLXX
#	include <replxx.hxx>
#elif defined( USE_EDITLINE )
#	include <histedit.h>
#endif

#include "linerunner.hxx"

namespace huginn {

class HShell;

class HRepl {
public:
	class HCompletion {
		yaal::hcore::HString _text;
		yaal::tools::COLOR::color_t _color;
	public:
		HCompletion( yaal::hcore::HString const& text_, yaal::tools::COLOR::color_t color_ = yaal::tools::COLOR::ATTR_DEFAULT )
			: _text( text_ )
			, _color( color_ ) {
		}
		yaal::hcore::HString const& text( void ) const {
			return ( _text );
		}
		yaal::tools::COLOR::color_t color( void ) const {
			return ( _color );
		}
		bool operator < ( HCompletion const& other_ ) const {
			return ( _text < other_._text );
		}
	};
	typedef yaal::hcore::HArray<HCompletion> completions_t;
	typedef completions_t ( *completion_words_t )( yaal::hcore::HString&&, yaal::hcore::HString&&, void* );
	typedef yaal::hcore::HBoundCall<> action_t;
private:
#ifdef USE_REPLXX
	typedef yaal::hcore::HHashMap<yaal::hcore::HString, char32_t> key_table_t;
	replxx::Replxx _replxx;
	key_table_t _keyTable;
#elif defined( USE_EDITLINE )
	typedef yaal::hcore::HHashMap<yaal::hcore::HString, action_t> key_table_t;
	EditLine* _el;
	History* _hist;
	HistEvent _histEvent;
	int _count;
	key_table_t _keyTable;
#else
	typedef yaal::hcore::HHashMap<yaal::hcore::HString, action_t> key_table_t;
	key_table_t _keyTable;
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
	completions_t completion_words( yaal::hcore::HString&&, yaal::hcore::HString&&, bool = true );
private:
#ifdef USE_REPLXX
	replxx::Replxx::ACTION_RESULT run_action( action_t, char32_t );
#else
#	if defined( USE_EDITLINE )
	typedef EditLine* arg_t;
	typedef char unsigned ret_t;
#	else
	typedef int arg_t;
	typedef int ret_t;
#	endif
	ret_t handle_key( char const* );
	static ret_t handle_key_F1( arg_t, int );
	static ret_t handle_key_F2( arg_t, int );
	static ret_t handle_key_F3( arg_t, int );
	static ret_t handle_key_F4( arg_t, int );
	static ret_t handle_key_F5( arg_t, int );
	static ret_t handle_key_F6( arg_t, int );
	static ret_t handle_key_F7( arg_t, int );
	static ret_t handle_key_F8( arg_t, int );
	static ret_t handle_key_F9( arg_t, int );
	static ret_t handle_key_F10( arg_t, int );
	static ret_t handle_key_F11( arg_t, int );
	static ret_t handle_key_F12( arg_t, int );
	static ret_t handle_key_SF1( arg_t, int );
	static ret_t handle_key_SF2( arg_t, int );
	static ret_t handle_key_SF3( arg_t, int );
	static ret_t handle_key_SF4( arg_t, int );
	static ret_t handle_key_SF5( arg_t, int );
	static ret_t handle_key_SF6( arg_t, int );
	static ret_t handle_key_SF7( arg_t, int );
	static ret_t handle_key_SF8( arg_t, int );
	static ret_t handle_key_SF9( arg_t, int );
	static ret_t handle_key_SF10( arg_t, int );
	static ret_t handle_key_SF11( arg_t, int );
	static ret_t handle_key_SF12( arg_t, int );
#endif
	HRepl( HRepl const& ) = delete;
	HRepl& operator = ( HRepl const& ) = delete;
};

}

#endif /* #ifndef HUHINN_REPL_HXX_INCLUDED */

