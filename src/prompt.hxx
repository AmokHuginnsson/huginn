/* Read huginn/LICENSE.md file for copyright and licensing information. */

/*! \file prompt.hxx
 * \brief Declaration of HPrompt class.
 */

#ifndef HUGINN_PROMPT_HXX_INCLUDED
#define HUGINN_PROMPT_HXX_INCLUDED 1

#include <yaal/hcore/hclock.hxx>

#include "repl.hxx"

namespace huginn {

class HSystemShell;

class HPromptRenderer {
protected:
	yaal::hcore::HString _template;
	int _lineNo;
	yaal::hcore::HClock _clock;
	yaal::hcore::HString _buffer;
	yaal::hcore::HUTF8String _utf8ConversionCache;
public:
	HPromptRenderer( yaal::hcore::HString const& = yaal::hcore::HString() );
	virtual ~HPromptRenderer( void );
	void make_prompt( yaal::hcore::HString const*, HSystemShell* );
	yaal::hcore::HString const& rendered_prompt( void ) const;
private:
	HPromptRenderer( HPromptRenderer const& ) = delete;
	HPromptRenderer& operator = ( HPromptRenderer const& ) = delete;
};

class HPrompt : public HPromptRenderer {
private:
	HRepl _repl;
public:
	HPrompt( yaal::hcore::HString const& = yaal::hcore::HString() );
	virtual ~HPrompt( void );
	bool input( yaal::hcore::HString&, yaal::hcore::HString const* = nullptr );
	HRepl& repl( void );
private:
	HPrompt( HPrompt const& ) = delete;
	HPrompt& operator = ( HPrompt const& ) = delete;
};

}

#endif /* #ifndef HUGINN_PROMPT_HXX_INCLUDED */

