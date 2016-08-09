(function() {
	function words( str ) {
		var obj = {}, words = str.split( " " );
		for ( var i = 0; i < words.length; ++i ) {
			obj[ words[i] ] = true;
		}
		return obj;
	}
	var keywords = "case else for if switch while class break continue assert default super this constructor destructor";
	var types = " integer number string character boolean real list deque dict order lookup set";
	var builtin = " size type copy observe use";
	var mode = {
		name: "clike",
		keywords: words( keywords + builtin ),
		blockKeywords: words( keywords ),
		builtin: words( types + builtin ),
		types: words( types ),
		atoms: words( "none true false" ),
		modeProps: { fold: ["brace"] }
	};
	var w = [];
	function add(obj) {
		if (obj) {
			for (var prop in obj) {
				if ( obj.hasOwnProperty(prop) ) {
					w.push(prop);
				}
			}
		}
	}
	add( mode.keywords );
	add( mode.builtin );
	add( mode.atoms );
	if ( words.length ) {
		mode.helperType = "text/x-huginn";
		CodeMirror.registerHelper( "hintWords", "text/x-huginn", w );
	}
	CodeMirror.defineMIME( "text/x-huginn", mode );
	CodeMirror.modeInfo.push( {
		name: "Huginn",
		mime: "text/x-huginn",
		mode: "clike"
	} );
})();
