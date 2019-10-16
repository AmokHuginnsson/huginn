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

enum class CONTEXT_TYPE {
	HUGINN,
	SHELL,
	PATH
};

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
		bool operator == ( HCompletion const& other_ ) const {
			return ( ( _text == other_._text ) && ( _color == other_._color ) );
		}
	};
	class HModel {
		yaal::hcore::HString _line;
		int _position;
	public:
		HModel( yaal::hcore::HString const& line_, int position_ )
			: _line( line_ )
			, _position( position_ ) {
		}
		yaal::hcore::HString const& line( void ) const {
			return ( _line );
		}
		int position( void ) const {
			return ( _position );
		}
	};
	typedef yaal::hcore::HArray<HCompletion> completions_t;
	typedef yaal::hcore::HArray<yaal::hcore::HString> lines_t;
	typedef completions_t ( *completion_words_t )( yaal::hcore::HString&&, yaal::hcore::HString&&, int&, CONTEXT_TYPE&, void* );
	typedef yaal::hcore::HBoundCall<> action_t;
#ifndef USE_REPLXX
#	ifdef USE_EDITLINE
	typedef EditLine* arg_t;
	typedef char unsigned ret_t;
#	else
	typedef int arg_t;
	typedef int ret_t;
#	endif
	using key_binding_dispatch_func_t = ret_t ( arg_t, int );
	struct OKeyBindDispatchInfo {
		char const* _seq;
#	ifdef USE_EDITLINE
		char const* _name;
#	else
		key_binding_dispatch_func_t* _func;
#	endif
		OKeyBindDispatchInfo( OKeyBindDispatchInfo const& ) = default;
		OKeyBindDispatchInfo& operator = ( OKeyBindDispatchInfo const& ) = default;
	};
	typedef yaal::hcore::HHashMap<yaal::hcore::HString, OKeyBindDispatchInfo> key_binding_dispatch_into_t;
#endif
private:
#ifdef USE_REPLXX
	typedef yaal::hcore::HHashMap<yaal::hcore::HString, char32_t> key_table_t;
	replxx::Replxx _replxx;
	key_table_t _keyTable;
#else
# ifdef USE_EDITLINE
	EditLine* _el;
	History* _hist;
	HistEvent _histEvent;
	int _count;
#	endif
	typedef yaal::hcore::HHashMap<yaal::hcore::HString, action_t> key_table_t;
	key_table_t _keyTable;
	key_binding_dispatch_into_t _keyBindingDispatchInfo;
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
	void set_hint_delay( int );
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
	bool bind_key( yaal::hcore::HString const&, action_t const& );
	completions_t completion_words( yaal::hcore::HString&&, yaal::hcore::HString&&, int&, CONTEXT_TYPE&, bool = true );
	void save_history( void );
	void clear_history( void );
	lines_t history( void ) const;
private:
	HModel get_model( void ) const;
	void set_model( HModel const& );
	void model_to_env( void );
	void env_to_model( void );
#ifdef USE_REPLXX
	replxx::Replxx::ACTION_RESULT run_action( action_t, char32_t );
#else
	ret_t handle_key( char const* );
	static ret_t handle_key_C_a( arg_t, int );
	static ret_t handle_key_C_b( arg_t, int );
	static ret_t handle_key_C_c( arg_t, int );
	static ret_t handle_key_C_d( arg_t, int );
	static ret_t handle_key_C_e( arg_t, int );
	static ret_t handle_key_C_f( arg_t, int );
	static ret_t handle_key_C_g( arg_t, int );
	static ret_t handle_key_C_h( arg_t, int );
	static ret_t handle_key_C_i( arg_t, int );
	static ret_t handle_key_C_j( arg_t, int );
	static ret_t handle_key_C_k( arg_t, int );
	static ret_t handle_key_C_l( arg_t, int );
	static ret_t handle_key_C_m( arg_t, int );
	static ret_t handle_key_C_n( arg_t, int );
	static ret_t handle_key_C_o( arg_t, int );
	static ret_t handle_key_C_p( arg_t, int );
	static ret_t handle_key_C_q( arg_t, int );
	static ret_t handle_key_C_r( arg_t, int );
	static ret_t handle_key_C_s( arg_t, int );
	static ret_t handle_key_C_t( arg_t, int );
	static ret_t handle_key_C_u( arg_t, int );
	static ret_t handle_key_C_v( arg_t, int );
	static ret_t handle_key_C_w( arg_t, int );
	static ret_t handle_key_C_x( arg_t, int );
	static ret_t handle_key_C_y( arg_t, int );
	static ret_t handle_key_C_z( arg_t, int );
	static ret_t handle_key_C_0( arg_t, int );
	static ret_t handle_key_C_1( arg_t, int );
	static ret_t handle_key_C_2( arg_t, int );
	static ret_t handle_key_C_3( arg_t, int );
	static ret_t handle_key_C_4( arg_t, int );
	static ret_t handle_key_C_5( arg_t, int );
	static ret_t handle_key_C_6( arg_t, int );
	static ret_t handle_key_C_7( arg_t, int );
	static ret_t handle_key_C_8( arg_t, int );
	static ret_t handle_key_C_9( arg_t, int );
	static ret_t handle_key_M_a( arg_t, int );
	static ret_t handle_key_M_b( arg_t, int );
	static ret_t handle_key_M_c( arg_t, int );
	static ret_t handle_key_M_d( arg_t, int );
	static ret_t handle_key_M_e( arg_t, int );
	static ret_t handle_key_M_f( arg_t, int );
	static ret_t handle_key_M_g( arg_t, int );
	static ret_t handle_key_M_h( arg_t, int );
	static ret_t handle_key_M_i( arg_t, int );
	static ret_t handle_key_M_j( arg_t, int );
	static ret_t handle_key_M_k( arg_t, int );
	static ret_t handle_key_M_l( arg_t, int );
	static ret_t handle_key_M_m( arg_t, int );
	static ret_t handle_key_M_n( arg_t, int );
	static ret_t handle_key_M_o( arg_t, int );
	static ret_t handle_key_M_p( arg_t, int );
	static ret_t handle_key_M_q( arg_t, int );
	static ret_t handle_key_M_r( arg_t, int );
	static ret_t handle_key_M_s( arg_t, int );
	static ret_t handle_key_M_t( arg_t, int );
	static ret_t handle_key_M_u( arg_t, int );
	static ret_t handle_key_M_v( arg_t, int );
	static ret_t handle_key_M_w( arg_t, int );
	static ret_t handle_key_M_x( arg_t, int );
	static ret_t handle_key_M_y( arg_t, int );
	static ret_t handle_key_M_z( arg_t, int );
	static ret_t handle_key_M_0( arg_t, int );
	static ret_t handle_key_M_1( arg_t, int );
	static ret_t handle_key_M_2( arg_t, int );
	static ret_t handle_key_M_3( arg_t, int );
	static ret_t handle_key_M_4( arg_t, int );
	static ret_t handle_key_M_5( arg_t, int );
	static ret_t handle_key_M_6( arg_t, int );
	static ret_t handle_key_M_7( arg_t, int );
	static ret_t handle_key_M_8( arg_t, int );
	static ret_t handle_key_M_9( arg_t, int );
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
	static ret_t handle_key_CF1( arg_t, int );
	static ret_t handle_key_CF2( arg_t, int );
	static ret_t handle_key_CF3( arg_t, int );
	static ret_t handle_key_CF4( arg_t, int );
	static ret_t handle_key_CF5( arg_t, int );
	static ret_t handle_key_CF6( arg_t, int );
	static ret_t handle_key_CF7( arg_t, int );
	static ret_t handle_key_CF8( arg_t, int );
	static ret_t handle_key_CF9( arg_t, int );
	static ret_t handle_key_CF10( arg_t, int );
	static ret_t handle_key_CF11( arg_t, int );
	static ret_t handle_key_CF12( arg_t, int );
#endif
	HRepl( HRepl const& ) = delete;
	HRepl& operator = ( HRepl const& ) = delete;
};

int context_length( yaal::hcore::HString const&, CONTEXT_TYPE );

}

#endif /* #ifndef HUHINN_REPL_HXX_INCLUDED */

