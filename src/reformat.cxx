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
		LIST,
		PARENTHESES,
		SCOPE,
		IF,
		IF_BODY,
		WHILE,
		WHILE_BODY,
		FOR,
		FOR_BODY,
		SWITCH,
		SWITCH_BODY,
		CASE,
		CASE_BODY,
		TRY,
		TRY_BODY,
		CATCH,
		CATCH_BODY,
		LAMBDA,
		EXPRESSION,
		STICKY
	};
	typedef HStack<STATE> scopes_t;
	HExecutingParser _engine;
	scopes_t _scopes;
	int _indentLevel;
	tools::string::tokens_t _line;
	yaal::hcore::HString _formatted;
public:
	HFormatter( void )
		: _engine( make_engine() )
		, _scopes()
		, _indentLevel( 0 )
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
	STATE state( void ) const {
		return ( ! _scopes.is_empty() ? _scopes.top() : STATE::NORMAL );
	}
	bool in_comment( void ) const {
		return ( state() == STATE::COMMENT );
	}
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
				( state() != STATE::COMMENT )
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
		if ( state() == STATE::STICKY ) {
			newline();
			_scopes.pop();
		}
		STATE s( state() );
		if ( s != STATE::COMMENT ) {
			bool doUpdate( false );
			if ( c_ == '(' ) {
				s = STATE::PARENTHESES;
			} else if ( c_ == '[' ) {
				s = STATE::LIST;
			} else if ( s == STATE::IF ) {
				s = STATE::IF_BODY;
				doUpdate = true;
			} else if ( s == STATE::CASE ) {
				s = STATE::CASE_BODY;
				doUpdate = true;
			} else if ( s == STATE::TRY ) {
				s = STATE::TRY_BODY;
				doUpdate = true;
			} else if ( c_ == '{' ) {
				s = STATE::SCOPE;
			}
			if ( doUpdate ) {
				_scopes.pop();
			}
			_scopes.push( s );
		}
		append( c_ );
		if ( c_ == '{' ) {
			newline();
			++ _indentLevel;
		}
	}
	void do_close( code_point_t c ) {
		if ( state() == STATE::STICKY ) {
			newline();
			_scopes.pop();
		}
		if ( state() != STATE::COMMENT ) {
			if ( _scopes.is_empty() || ( matching( _scopes.top() ) != c ) ) {
				throw HRuntimeException( "Syntax error" );
			}
			STATE state( _scopes.top() );
			_scopes.pop();
			if ( ( state == STATE::IF_BODY ) || ( state == STATE::CASE_BODY ) || ( state == STATE::TRY_BODY ) ) {
				_scopes.push( STATE::STICKY );
			}
		}
		if ( c == '}' ) {
			-- _indentLevel;
			M_ASSERT( _indentLevel >= 0 );
			append( c );
			if ( state() != STATE::STICKY ) {
				newline( _indentLevel ? 1 : 2 );
			}
		} else {
			append( c );
		}
	}
	void do_oper( code_point_t c ) {
		if ( state() == STATE::STICKY ) {
			newline();
			_scopes.pop();
		}
		append( c );
		if ( c == ';' ) {
			newline();
		}
	}
	void start_comment( void ) {
		append( "/*" );
		_scopes.push( STATE::COMMENT );
	}
	void end_comment( void ) {
		commit();
		_scopes.pop();
		append( "*/" );
		newline();
	}
	void do_identifier( yaal::hcore::HString const& word_ ) {
		if ( state() == STATE::STICKY ) {
			if ( ( word_ != "else" ) && ( word_ != "break" ) && ( word_ != "catch" ) ) {
				newline();
			}
			_scopes.pop();
		}
		if ( state() != STATE::COMMENT ) {
			if ( word_ == "case" ) {
				_scopes.push( STATE::CASE );
			} else if ( word_ == "if" ) {
				_scopes.push( STATE::IF );
			} else if ( word_ == "try" ) {
				_scopes.push( STATE::TRY );
			}
		}
		append( word_ );
	}
	void do_string_literal( yaal::hcore::HString const& literal_ ) {
		append( "\""_ys.append( literal_ ).append( '"' ) );
		if ( state() == STATE::STICKY ) {
			newline();
			_scopes.pop();
		}
	}
	void do_character_literal( yaal::hcore::HString const& literal_ ) {
		append( "'"_ys.append( literal_ ).append( "'" ) );
		if ( state() == STATE::STICKY ) {
			newline();
			_scopes.pop();
		}
	}
	void do_white( yaal::hcore::HString const& white_ ) {
		if ( state() == STATE::COMMENT ) {
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
	static code_point_t matching( STATE state_ ) {
		switch ( state_ ) {
			case ( STATE::IF ):          return ( ')'_ycp );
			case ( STATE::CASE ):        return ( ')'_ycp );
			case ( STATE::TRY ):         return ( '}'_ycp );
			case ( STATE::IF_BODY ):     return ( '}'_ycp );
			case ( STATE::CASE_BODY ):   return ( '}'_ycp );
			case ( STATE::TRY_BODY ):    return ( '}'_ycp );
			case ( STATE::SCOPE ):       return ( '}'_ycp );
			case ( STATE::LIST ):        return ( ']'_ycp );
			case ( STATE::PARENTHESES ): return ( ')'_ycp );
			default: {
				M_ASSERT( !"Bad code path!"[0] );
			}
		}
		return ( '\0'_ycp );
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

