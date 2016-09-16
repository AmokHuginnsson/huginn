/*
---           `huginn' 0.0.0 (c) 1978 by Marcin 'Amok' Konarski            ---

  linerunner.hxx - this file is integral part of `huginn' project.

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

/*! \file linerunner.hxx
 * \brief Declaration of HLineRunner class.
 */

#ifndef LINERUNNER_HXX_INCLUDED
#define LINERUNNER_HXX_INCLUDED 1

#include <yaal/tools/hhuginn.hxx>
#include <yaal/tools/hstringstream.hxx>

namespace huginn {

class HLineRunner {
public:
	typedef HLineRunner this_type;
	typedef yaal::hcore::HArray<yaal::hcore::HString> lines_t;
	typedef yaal::hcore::HArray<yaal::hcore::HString> words_t;
	typedef yaal::hcore::HHashMap<yaal::hcore::HString, words_t> method_map_t;
	typedef yaal::hcore::HHashMap<yaal::hcore::HString, yaal::hcore::HString> symbol_map_t;
	enum class LINE_TYPE {
		NONE,
		CODE,
		DEFINITION,
		IMPORT
	};
private:
	lines_t _lines;
	lines_t _imports;
	lines_t _definitions;
	int _definitionsLineCount;
	LINE_TYPE _lastLineType;
	yaal::hcore::HString _lastLine;
	bool _interrupted;
	yaal::tools::HHuginn::ptr_t _huginn;
	yaal::tools::HStringStream _streamCache;
	words_t _wordCache;
	symbol_map_t _symbolMap;
	method_map_t _methodMap;
	yaal::hcore::HString _source;
	yaal::hcore::HString _session;
public:
	HLineRunner( yaal::hcore::HString const& );
	bool add_line( yaal::hcore::HString const& );
	yaal::tools::HHuginn::value_t execute( void );
	yaal::hcore::HString err( void ) const;
	int handle_interrupt( int );
	void fill_words( void );
	words_t const& words( void );
	words_t const& methods( yaal::hcore::HString const& );
	yaal::hcore::HString const& source( void ) const;
	lines_t const& imports( void ) const;
	bool use_result( void ) const;
	yaal::tools::HHuginn const* huginn( void ) const;
};

}

#endif /* #ifndef LINERUNNER_HXX_INCLUDED */

