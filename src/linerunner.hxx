/* Read huginn/LICENSE.md file for copyright and licensing information. */

/*! \file linerunner.hxx
 * \brief Declaration of HLineRunner class.
 */

#ifndef LINERUNNER_HXX_INCLUDED
#define LINERUNNER_HXX_INCLUDED 1

#include <yaal/hcore/duration.hxx>
#include <yaal/tools/hstringstream.hxx>
#include <yaal/tools/filesystem.hxx>
#include <yaal/tools/huginn/helper.hxx>

#include "description.hxx"
#include "reformat.hxx"

namespace huginn {

class HLineRunner : public yaal::tools::HIntrospectorInterface {
public:
	typedef HLineRunner this_type;
	typedef HDescription::words_t words_t;
	class HEntry {
		yaal::hcore::HString _data;
		bool _persist;
	public:
		HEntry( yaal::hcore::HString const& data_, bool persist_ )
			: _data( data_ )
			, _persist( persist_ ) {
		}
		yaal::hcore::HString data( void ) const {
			return ( _data );
		}
		bool persist( void ) const {
			return ( _persist );
		}
		bool operator != ( yaal::hcore::HString const& data_ ) const {
			return ( _data != data_ );
		}
	};
	class HTimeItResult {
		int _count;
		yaal::hcore::time::duration_t _total;
		yaal::hcore::time::duration_t _iteration;
	public:
		HTimeItResult( int, yaal::hcore::time::duration_t );
		int count( void ) const {
			return ( _count );
		}
		yaal::hcore::time::duration_t total( void ) const {
			return ( _total );
		}
		yaal::hcore::time::duration_t iteration( void ) const {
			return ( _iteration );
		}
	};
	typedef yaal::hcore::HArray<HEntry> entries_t;
	typedef yaal::hcore::HHashMap<yaal::hcore::HString, yaal::tools::huginn::HClass const*> symbol_types_t;
	enum class LINE_TYPE {
		NONE,
		CODE,
		DEFINITION,
		IMPORT,
		TRIMMED_CODE
	};
private:
	entries_t _lines;
	entries_t _imports;
	entries_t _definitions;
	int _definitionsLineCount;
	LINE_TYPE _lastLineType;
	yaal::hcore::HString _lastLine;
	bool _interrupted;
	yaal::tools::HHuginn::ptr_t _huginn;
	yaal::tools::HStringStream _streamCache;
	HDescription _description;
	yaal::hcore::HString _source;
	yaal::tools::HIntrospecteeInterface::variable_views_t _locals;
	yaal::tools::huginn::classes_t _localsTypes;
	symbol_types_t _symbolToTypeCache;
	entries_t _sessionFiles;
	yaal::hcore::HString _tag;
	bool _ignoreIntrospection;
	bool _executing;
	yaal::hcore::HString _errorMessage;
	int _errorLine;
	int _errorColumn;
	HFormatter _formatter;
	mutable yaal::hcore::HMutex _mutex;
public:
	HLineRunner( yaal::hcore::HString const& );
	bool add_line( yaal::hcore::HString const&, bool );
	yaal::tools::HHuginn::value_t execute( void );
	HTimeItResult timeit( int );
	yaal::hcore::HString err( void ) const;
	words_t const& words( bool );
	yaal::hcore::HString const& source( void );
	words_t const& members( yaal::hcore::HString const&, bool );
	words_t const& dependent_symbols( yaal::hcore::HString const&, bool );
	entries_t const& imports( void ) const;
	yaal::hcore::HString symbol_type_name( yaal::hcore::HString const& );
	HDescription::SYMBOL_KIND symbol_kind( yaal::hcore::HString const& ) const;
	yaal::hcore::HString doc( yaal::hcore::HString const&, bool );
	bool use_result( void ) const;
	void reset( void );
	void reload( void );
	void undo( void );
	yaal::tools::HHuginn::value_t call( yaal::hcore::HString const&, yaal::tools::HHuginn::values_t const&, yaal::hcore::HStreamInterface* = nullptr, bool = true );
	void load_session( yaal::tools::filesystem::path_t const&, bool, bool = true );
	void save_session( yaal::tools::filesystem::path_t const& );
	yaal::tools::HHuginn const* huginn( void ) const;
	yaal::tools::HHuginn* huginn( void );
	void stop( void );
	yaal::tools::HIntrospecteeInterface::variable_views_t const& locals( void ) const;
	entries_t const& definitions( void ) const;
	void mend_interrupt( void );
	bool is_executing( void ) const;
protected:
	virtual void do_introspect( yaal::tools::HIntrospecteeInterface& ) override;
private:
	yaal::tools::HHuginn::value_t do_execute( bool );
	yaal::tools::HHuginn::value_t finalize_execute( bool, bool, yaal::tools::HIntrospecteeInterface::variable_views_t const&, yaal::tools::huginn::classes_t const&, int, int );
	yaal::tools::huginn::HClass const* symbol_type_id( yaal::hcore::HString const& );
	void mend( void );
	bool amend(  yaal::hcore::HString const& );
	int handle_interrupt( int );
	void prepare_source( void );
	void load_session_impl( yaal::tools::filesystem::path_t const&, bool, bool );
	void reset_session( bool );
	yaal::tools::huginn::HClass const* symbol_type_id( yaal::tools::HHuginn::value_t const& );
	void save_error_info( void );
};

}

#endif /* #ifndef LINERUNNER_HXX_INCLUDED */

