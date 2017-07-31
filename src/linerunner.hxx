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

#include <yaal/tools/hstringstream.hxx>

#include "description.hxx"

namespace huginn {

class HLineRunner : public yaal::tools::HIntrospectorInterface {
public:
	typedef HLineRunner this_type;
	typedef HDescription::words_t words_t;
	typedef yaal::hcore::HArray<yaal::hcore::HString> lines_t;
	typedef HDescription::symbol_map_t symbol_map_t;
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
	HDescription _description;
	yaal::hcore::HString _source;
	yaal::tools::HIntrospecteeInterface::variable_views_t _locals;
	yaal::hcore::HString _session;
public:
	HLineRunner( yaal::hcore::HString const& );
	bool add_line( yaal::hcore::HString const& );
	yaal::tools::HHuginn::value_t execute( void );
	yaal::hcore::HString err( void ) const;
	int handle_interrupt( int );
	words_t const& words( void );
	yaal::hcore::HString const& source( void );
	words_t const& methods( yaal::hcore::HString const& );
	words_t const& dependent_symbols( yaal::hcore::HString const& );
	lines_t const& imports( void ) const;
	yaal::hcore::HString doc( yaal::hcore::HString const& );
	bool use_result( void ) const;
	void reset( void );
	void undo( void );
	yaal::tools::HHuginn const* huginn( void ) const;
	yaal::tools::HHuginn* huginn( void );
protected:
	virtual void do_introspect( yaal::tools::HIntrospecteeInterface& ) override;
private:
	void prepare_source( void );
};

}

#endif /* #ifndef LINERUNNER_HXX_INCLUDED */

