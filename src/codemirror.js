(function() {
	function words( str ) {
		var obj = {}, words = str.split( " " );
		for ( var i = 0; i < words.length; ++i ) {
			obj[ words[i] ] = true;
		}
		return obj;
	}
	var keywords = "case else for if switch while class break continue assert default super this constructor destructor";
	var builtin = " integer number string character boolean real list deque dict order lookup set size type copy observe use";
	var mime = "text/x-huginn";
	var modeName = "clike";
	var mode = {
		name: modeName,
		keywords: words( builtin ),
		blockKeywords: words( keywords ),
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
		mode.helperType = mime;
		CodeMirror.registerHelper( "hintWords", mime, w );
	}
	CodeMirror.defineMIME( mime, mode );
	CodeMirror.modeInfo.push( {
		name: "Huginn",
		mime: mime,
		mode: modeName
	} );
})();
