/* Read huginn/LICENSE.md file for copyright and licensing information. */

#include <yaal/tools/huginn/helper.hxx>
#include <yaal/tools/executingparser.hxx>
#include <yaal/tools/stringalgo.hxx>

M_VCSID( "$Id: " __ID__ " $" )
#include "reformat.hxx"
#include "huginn.hxx"
#include "setup.hxx"

using namespace yaal;
using namespace yaal::hcore;
using namespace yaal::tools;
using namespace yaal::tools::executing_parser;
using namespace yaal::tools::huginn;

namespace huginn {

inline bool is_statement( yaal::hcore::HString const& token_ ) {
	return (
		( token_ != "constructor" )
		&& ( token_ != "destructor" )
		&& ( token_ != "assert" )
		&& is_keyword( token_ )
	);
}

class HFormatter {
private:
	enum class STATE {
		NORMAL,
		COMMENT,
		CASE,
		IF,
		TRY,
		CATCH,
		STICKY
	};
	typedef HStack<code_point_t> scopes_t;
	HExecutingParser _engine;
	scopes_t _scopes;
	int _indentLevel;
	STATE _state;
	tools::string::tokens_t _line;
	yaal::hcore::HString _formatted;
public:
	HFormatter( void )
		: _engine( make_engine() )
		, _scopes()
		, _indentLevel( 0 )
		, _state( STATE::NORMAL )
		, _line()
		, _formatted() {
	}
	yaal::hcore::HString const& reformat( yaal::hcore::HString const& raw_ ) {
		if ( ! _engine( raw_ ) ) {
			cerr << _engine.error_position() << endl;
			for ( yaal::hcore::HString const& errMsg : _engine.error_messages() ) {
				cerr << errMsg << endl;
			}
			return ( _formatted );
		}
		_engine();
		if ( ! _formatted.is_empty() && ( _formatted.back() != '\n' ) ) {
			_formatted.push_back( '\n'_ycp );
		}
		return ( _formatted );
	}
private:
	void commit( void ) {
		_formatted.append( _indentLevel, '\t'_ycp );
		hcore::HString prev;
		bool inCase( false );
		char const noSpaceMinusOp[][3] = {
			"(", "{", "[", "?", ":", "+", "*", "/", "%", "^", "=", "==", "!=", "<=", ">=", "+=", "-=", "*=", "/=", "%=", "^=", "||", "&&", "^^"
		};
		int hadNoSpaceMinusOp( 100000 );
		for ( hcore::HString const& tok : _line ) {
			bool hasPunctation( prev.find_one_of( character_class<CHARACTER_CLASS::PUNCTATION>().data() ) != hcore::HString::npos );
			bool isStatement( is_statement( prev ) );
			if (
				( find( begin( noSpaceMinusOp ), end( noSpaceMinusOp ), prev ) != end( noSpaceMinusOp ) )
			) {
				hadNoSpaceMinusOp = 0;
			}
			inCase = inCase || ( tok == "case" ) || ( tok == "default" );
			if (
				( _state != STATE::COMMENT )
				&& ! prev.is_empty()
				&& ( ( ( tok != "(" ) && ( tok != "[" ) ) || isStatement || hasPunctation )
				&& ( tok != "," )
				&& ( tok != ";" )
				&& ( prev != "." )
				&& ( prev != "@" )
				&& ( tok != "." )
				&& ( ! inCase || ( tok != ":" ) )
				&& ( ( prev != ")" ) || ( tok != "[" ) )
				&& ( ( prev != "]" ) || ( tok != "[" ) )
				&& ( ( prev != "]" ) || ( tok != "(" ) )
				&& ( ( prev != "(" ) || ( tok != ")" ) )
				&& ( ( prev != "[" ) || ( tok != "]" ) )
				&& ( ( prev != "{" ) || ( tok != "}" ) )
				&& ( prev != "[" )
				&& ( tok != "]" )
				&& ( ( hadNoSpaceMinusOp != 1 ) || ( prev != "-" ) )
			) {
				_formatted.append( " " );
			}
			_formatted.append( tok );
			++ hadNoSpaceMinusOp;
			if ( tok == ":" ) {
				inCase = false;
			}
			prev.assign( tok );
		}
		_line.clear();
	}
	void newline( int count_ = 1 ) {
		commit();
		_formatted.append( count_, '\n'_ycp );
	}
	void do_open( code_point_t c_ ) {
		if ( _state == STATE::STICKY ) {
			newline();
			_state = STATE::NORMAL;
		}
		if ( _state != STATE::COMMENT ) {
			code_point_t c( c_ );
			if ( c == '{' ) {
				if ( _state == STATE::CASE ) {
					c = 'c'_ycp;
				} else if ( _state == STATE::IF ) {
					c = 'i'_ycp;
				} else if ( _state == STATE::TRY ) {
					c = 't'_ycp;
				}
				_state = STATE::NORMAL;
			}
			_scopes.push( c );
		}
		append( c_ );
		if ( c_ == '{' ) {
			newline();
			++ _indentLevel;
		}
	}
	void do_close( code_point_t c ) {
		if ( _state == STATE::STICKY ) {
			newline();
			_state = STATE::NORMAL;
		}
		if ( _state != STATE::COMMENT ) {
			if ( _scopes.is_empty() || ( matching( _scopes.top() ) != c ) ) {
				throw HRuntimeException( "Syntax error" );
			}
			code_point_t scope( _scopes.top() );
			_scopes.pop();
			if ( ( scope == 'c' ) || ( scope == 'i' ) || ( scope == 't' ) ) {
				_state = STATE::STICKY;
			}
		}
		if ( c == '}' ) {
			-- _indentLevel;
			M_ASSERT( _indentLevel >= 0 );
			append( c );
			if ( _state != STATE::STICKY ) {
				newline( _indentLevel ? 1 : 2 );
			}
		} else {
			append( c );
		}
	}
	void do_oper( code_point_t c ) {
		if ( _state == STATE::STICKY ) {
			newline();
			_state = STATE::NORMAL;
		}
		append( c );
		if ( c == ';' ) {
			newline();
		}
	}
	void start_comment( void ) {
		append( "/*" );
		_state = STATE::COMMENT;
	}
	void end_comment( void ) {
		commit();
		_state = STATE::NORMAL;
		append( "*/" );
		newline();
	}
	void do_identifier( yaal::hcore::HString const& word_ ) {
		if ( _state == STATE::STICKY ) {
			if ( ( word_ != "else" ) && ( word_ != "break" ) && ( word_ != "catch" ) ) {
				newline();
			}
			_state = STATE::NORMAL;
		}
		if ( _state != STATE::COMMENT ) {
			if ( word_ == "case" ) {
				_state = STATE::CASE;
			} else if ( word_ == "if" ) {
				_state = STATE::IF;
			} else if ( word_ == "try" ) {
				_state = STATE::TRY;
			}
		}
		append( word_ );
	}
	void do_string_literal( yaal::hcore::HString const& literal_ ) {
		append( "\""_ys.append( literal_ ).append( '"' ) );
		if ( _state == STATE::STICKY ) {
			newline();
			_state = STATE::NORMAL;
		}
	}
	void do_character_literal( yaal::hcore::HString const& literal_ ) {
		append( "'"_ys.append( literal_ ).append( "'" ) );
		if ( _state == STATE::STICKY ) {
			newline();
			_state = STATE::NORMAL;
		}
	}
	void do_white( yaal::hcore::HString const& white_ ) {
		if ( _state == STATE::COMMENT ) {
			append( white_ );
		}
	}
	template<typename T>
	HFormatter& append( T const& tok_ ) {
		_line.push_back( tok_ );
		return ( *this );
	}
private:
	HRule make_engine( void ) {
		namespace e_p = yaal::tools::executing_parser;
		HRule white( regex( "[[:space:]]+", e_p::HRegex::action_string_t( hcore::call( &HFormatter::do_white, this, _1 ) ), false ) );
		HRule open( characters( "{([", e_p::HCharacter::action_character_t( hcore::call( &HFormatter::do_open, this, _1 ) ) ) );
		HRule close( characters( "])}", e_p::HCharacter::action_character_t( hcore::call( &HFormatter::do_close, this, _1 ) ) ) );
		HRule longOper(
			e_p::constant(
				{ "==", "!=", "<=", ">=", "+=", "-=", "*=", "/=", "%=", "^=", "&&", "||", "^^" },
				e_p::HString::action_string_t( hcore::call( &HFormatter::do_identifier, this, _1 ) )
			)
		);
		HRule oper( characters( "+\\-*/%!|\\^?:=<>.@~,;", e_p::HCharacter::action_character_t( hcore::call( &HFormatter::do_oper, this, _1 ) ) ) );
		HRule identifier( regex( "[^ \\t\\r\\n\\[\\](){}+*/%!|\\^?:=<>,.;@~'\"-]+", HStringLiteral::action_string_t( hcore::call( &HFormatter::do_identifier, this, _1 ) ) ) );
		HRule startComment( e_p::string( "/*", e_p::HString::action_t( hcore::call( &HFormatter::start_comment, this ) ) ) );
		HRule endComment( e_p::string( "*/", e_p::HString::action_t( hcore::call( &HFormatter::end_comment, this ) ) ) );
		HRule lexemes(
			white
			| startComment
			| endComment
			| open
			| close
			| longOper
			| oper
			| string_literal( HStringLiteral::SEMANTIC::RAW )[HStringLiteral::action_string_t( hcore::call( &HFormatter::do_string_literal, this, _1 ) )]
			| character_literal( HCharacterLiteral::SEMANTIC::RAW )[HCharacterLiteral::action_string_t( hcore::call( &HFormatter::do_character_literal, this, _1 ) )]
			| identifier
		);
		return ( *lexemes );
	}
	static code_point_t matching( code_point_t c_ ) {
		switch ( c_.get() ) {
			case ( 'c' ): return ( '}'_ycp );
			case ( 'i' ): return ( '}'_ycp );
			case ( 't' ): return ( '}'_ycp );
			case ( '{' ): return ( '}'_ycp );
			case ( '[' ): return ( ']'_ycp );
			case ( '(' ): return ( ')'_ycp );
		}
		return ( c_ );
	}
};

int reformat( char const* script_ ) {
	buffer_t source( ::huginn::load( script_ ) );
	HFormatter formatter;
	hcore::HString s( source.data(), source.get_size() );
	cout << formatter.reformat( s ) << flush;
	return ( 0 );
}

}

