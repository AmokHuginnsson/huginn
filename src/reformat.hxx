/* Read huginn/LICENSE.md file for copyright and licensing information. */

/*! \file reformat.hxx
 * \brief Declaration of reformat() function.
 */

#ifndef REFORMAT_HXX_INCLUDED
#define REFORMAT_HXX_INCLUDED 1

#include <yaal/hcore/hresource.hxx>
#include <yaal/tools/filesystem.hxx>

namespace huginn {

class HFormatter {
private:
	class HFormatterImpl;
	typedef yaal::hcore::HResource<HFormatterImpl> impl_t;
	impl_t _impl;
public:
	HFormatter( void );
	virtual ~HFormatter( void );
	bool reformat_file( char const* );
	bool reformat_string( yaal::hcore::HString const&, yaal::hcore::HString& );
	yaal::hcore::HString const& error_message( void ) const;
private:
	HFormatter( HFormatter const& ) = delete;
	HFormatter& operator = ( HFormatter const& ) = delete;
};

}

#endif /* #ifndef REFORMAT_HXX_INCLUDED */

