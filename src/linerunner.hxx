/* Read huginn/LICENSE.md file for copyright and licensing information. */

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
	symbol_map_t _symbolToTypeCache;
	yaal::hcore::HString _tag;
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
	yaal::hcore::HString symbol_type( yaal::hcore::HString const& );
	HDescription::SYMBOL_KIND symbol_kind( yaal::hcore::HString const& ) const;
	yaal::hcore::HString doc( yaal::hcore::HString const& );
	bool use_result( void ) const;
	void reset( void );
	void undo( void );
	void load_session( void );
	void save_session( void );
	yaal::tools::HHuginn const* huginn( void ) const;
	yaal::tools::HHuginn* huginn( void );
protected:
	virtual void do_introspect( yaal::tools::HIntrospecteeInterface& ) override;
private:
	void prepare_source( void );
};

}

#endif /* #ifndef LINERUNNER_HXX_INCLUDED */

