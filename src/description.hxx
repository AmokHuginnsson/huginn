/* Read huginn/LICENSE.md file for copyright and licensing information. */

/*! \file description.hxx
 * \brief Declaration of HDescription class.
 */

#ifndef DESCRIPTION_HXX_INCLUDED
#define DESCRIPTION_HXX_INCLUDED 1

#include <yaal/tools/hhuginn.hxx>

namespace huginn {

class HDescription {
public:
	typedef HDescription this_type;
	typedef yaal::hcore::HArray<yaal::hcore::HString> words_t;
	typedef yaal::hcore::HHashMap<yaal::hcore::HString, words_t> member_map_t;
	typedef yaal::hcore::HHashMap<yaal::hcore::HString, yaal::hcore::HString> symbol_map_t;
	enum class SYMBOL_KIND {
		CLASS,
		FUNCTION,
		METHOD,
		PACKAGE,
		ALIAS,
		LITERAL,
		UNKNOWN
	};
private:
	words_t _symbols;
	words_t _classes;
	words_t _functions;
	symbol_map_t _packages;
	member_map_t _memberMap;
	symbol_map_t _docs;
	words_t _docSymbols;
	yaal::tools::HStringStream _streamCache;
public:
	HDescription( void );
	void prepare( yaal::tools::HHuginn const& );
	void note_locals( yaal::tools::HIntrospecteeInterface::variable_views_t const& );
	void clear( void );
	words_t const& members( yaal::hcore::HString const& );
	words_t const& symbols( bool ) const;
	words_t const& classes( void ) const;
	words_t const& functions( void ) const;
	yaal::hcore::HString const& package_alias( yaal::hcore::HString const& ) const;
	yaal::hcore::HString doc( yaal::hcore::HString const&, yaal::hcore::HString const& = yaal::hcore::HString() ) const;
	SYMBOL_KIND symbol_kind( yaal::hcore::HString const& ) const;
};

void dump_call_stack( yaal::tools::HHuginn::call_stack_t const&, yaal::hcore::HStreamInterface& );

}

#endif /* #ifndef DESCRIPTION_HXX_INCLUDED */

