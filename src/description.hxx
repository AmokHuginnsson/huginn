/*
---           `huginn' 0.0.0 (c) 1978 by Marcin 'Amok' Konarski            ---

  description.hxx - this file is integral part of `huginn' project.

  i.  You may not make any changes in Copyright information.
  ii. You must attach Copyright information to any part of every copy
      of this software.

Copyright:

 You can use this software free of charge and you can redistribute its binary
 package freely but:
  1. You are not allowed to use any part of sources of this software.
  2. You are not allowed to redistribute any part of sources of this software.
  3. You are not allowed to reverse engineer this software.
  4. If you want to distribute a binary package of this software you cannot
     demand any fees for it. You cannot even demand
     a return of cost of the media or distribution (CD for example).
  5. You cannot involve this software in any commercial activity (for example
     as a free add-on to paid software or newspaper).
 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. Use it at your own risk.
*/

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
	typedef yaal::hcore::HHashMap<yaal::hcore::HString, words_t> method_map_t;
	typedef yaal::hcore::HHashMap<yaal::hcore::HString, yaal::hcore::HString> symbol_map_t;
private:
	words_t _symbols;
	words_t _classes;
	words_t _functions;
	symbol_map_t _symbolMap;
	method_map_t _methodMap;
	symbol_map_t _docs;
	yaal::tools::HStringStream _streamCache;
public:
	HDescription( void );
	void prepare( yaal::tools::HHuginn const& );
	void clear( void );
	words_t const& methods( yaal::hcore::HString const& );
	words_t const& dependent_symbols( yaal::hcore::HString const& );
	words_t const& symbols( void ) const;
	words_t const& classes( void ) const;
	words_t const& functions( void ) const;
	yaal::hcore::HString doc( yaal::hcore::HString const&, yaal::hcore::HString const& = yaal::hcore::HString() ) const;
};

}

#endif /* #ifndef DESCRIPTION_HXX_INCLUDED */

