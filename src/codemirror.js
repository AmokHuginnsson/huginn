// CodeMirror, copyright (c) by Marijn Haverbeke and others
// Distributed under an MIT license: http://codemirror.net/LICENSE

(function(mod) {
	if (typeof exports == "object" && typeof module == "object") // CommonJS
		mod(require("../../lib/codemirror"));
	else if (typeof define == "function" && define.amd) // AMD
		define(["../../lib/codemirror"], mod);
	else // Plain browser env
		mod( CodeMirror );
})(function( CodeMirror ) {
	"use strict";

	function words( str ) {
		let obj = {}, words = str.split( " " );
		for (let i = 0; i < words.length; ++i) obj[words[i]] = true;
		return obj;
	}
	function contains(words, word) {
		if ( typeof words === "function" ) {
			return words( word );
		} else {
			return words.propertyIsEnumerable( word );
		}
	}

	CodeMirror.defineMode( "huginn", function( config, parserConfig ) {
		let strKeywords = " assert break case catch class constructor continue default destructor else enum for if return super switch this throw try while";
		let strTypes = " blob boolean character deque dict integer list lookup number order real set string tuple";
		let strBuiltin = " copy observe size type use √ ∑ ∏";
		let strMagic = "doc reset source imports version";

		let indentUnit = config.indentUnit,
		statementIndentUnit = parserConfig.statementIndentUnit || indentUnit,
		dontAlignCalls = parserConfig.dontAlignCalls,
		keywords = words( strKeywords ),
		types = words( strTypes ),
		builtin = words( strBuiltin ),
		magic = words( strMagic ),
		atoms = words( "none true false" ),
		imports = words( "import as from" ),
		hooks = parserConfig.hooks || {},
		multiLineStrings = parserConfig.multiLineStrings,
		indentStatements = parserConfig.indentStatements !== false,
		indentSwitch = parserConfig.indentSwitch !== false,
		namespaceSeparator = parserConfig.namespaceSeparator,
		isPunctuationChar = parserConfig.isPunctuationChar || /[\[\]{}\(\),;\:\.]/,
		isNumberChar = parserConfig.isNumberChar || /\d/,
		isOperatorChar = parserConfig.isOperatorChar || /[+\-*&%=<>!?|\/⋀⋁⊕¬≠≤≥∈∉]/,
		endStatement = parserConfig.endStatement || /^[;:,]$/;

		let curPunc;

		function tokenBase(stream, state) {
			let ch = stream.next();
			if (hooks[ch]) {
				let result = hooks[ch](stream, state);
				if (result !== false) return result;
			}
			if (ch == '"' || ch == "'") {
				state.tokenize = tokenString(ch);
				return state.tokenize(stream, state);
			}
			if (isPunctuationChar.test(ch)) {
				curPunc = ch;
				return null;
			}
			if ( ch == '$' ) {
				stream.match( /[\d\.]+([eE][+-]?\d+)?/ );
				return "number";
			} else if ( ( ch == '0' ) && stream.match( /([bB][01]+|[oO][0-7]+|[xX][0-9a-fA-F]+)/ ) ) {
				return "number";
			} else if ( isNumberChar.test( ch ) ) {
				stream.match( /[\d\.]*([eE][+-]?\d+)?/ );
				return "number";
			}
			if ( ch == "/" ) {
				if ( stream.eat( "*" ) ) {
					state.tokenize = tokenComment;
					return tokenComment( stream, state );
				}
				if ( stream.eat( "/" ) ) {
					stream.skipToEnd();
					return "comment";
				}
			}
			if ( isOperatorChar.test( ch ) ) {
				stream.eatWhile( isOperatorChar );
				return "operator";
			}
			stream.eatWhile(/[\w\$_\xa1-\uffff]/);
			if ( namespaceSeparator ) {
				while ( stream.match( namespaceSeparator ) ) {
					stream.eatWhile(/[\w\$_\xa1-\uffff]/);
				}
			}

			let cur = stream.current();
			if ( contains( keywords, cur ) ) {
				return "keyword";
			}
			if ( contains( types, cur ) ) {
				return "type";
			}
			if ( contains( builtin, cur ) ) {
				return "builtin";
			}
			if ( contains( atoms, cur ) ) {
				return "atom";
			}
			if ( contains( imports, cur ) ) {
				return "import";
			}
			if ( contains( magic, cur ) ) {
				return "magic";
			}
			if ( /\b[A-Z][A-Z0-9_]*[A-Z0-9]\b/.test( cur ) ) {
				return "constant";
			}
			if ( /\b[A-Z][a-zA-Z]*\b/.test( cur ) ) {
				return "class";
			}
			if ( /\b_[a-zA-Z0-9]+\b/.test( cur ) ) {
				return "field";
			}
			if ( /\b[a-zA-Z][a-zA-Z0-9]*_\b/.test( cur ) ) {
				return "argument";
			}
			return "variable";
		}

		function tokenString(quote) {
			return function(stream, state) {
				let escaped = false, next, end = false;
				while ((next = stream.next()) != null) {
					if (next == quote && !escaped) {end = true; break;}
					escaped = !escaped && next == "\\";
				}
				if (end || !(escaped || multiLineStrings))
					state.tokenize = null;
				return "string";
			};
		}

		function tokenComment(stream, state) {
			let maybeEnd = false, ch;
			while (ch = stream.next()) {
				if (ch == "/" && maybeEnd) {
					state.tokenize = null;
					break;
				}
				maybeEnd = (ch == "*");
			}
			return "comment";
		}

		function Context(indented, column, type, align, prev) {
			this.indented = indented;
			this.column = column;
			this.type = type;
			this.align = align;
			this.prev = prev;
		}
		function isStatement(type) {
			return type == "statement" || type == "switchstatement" || type == "namespace";
		}
		function pushContext(state, col, type) {
			let indent = state.indented;
			if (state.context && isStatement(state.context.type) && !isStatement(type))
				indent = state.context.indented;
			return state.context = new Context(indent, col, type, null, state.context);
		}
		function popContext(state) {
			let t = state.context.type;
			if (t == ")" || t == "]" || t == "}")
				state.indented = state.context.indented;
			return state.context = state.context.prev;
		}

		function typeBefore(stream, state) {
			if (state.prevToken == "variable" || state.prevToken == "type") return true;
			if (/\S(?:[^- ]>|[*\]])\s*$|\*$/.test(stream.string.slice(0, stream.start))) return true;
		}

		function isTopScope(context) {
			for (;;) {
				if (!context || context.type == "top") return true;
				if (context.type == "}" && context.prev.type != "namespace") return false;
				context = context.prev;
			}
		}

		// Interface

		return {
			startState: function(basecolumn) {
				return {
					tokenize: null,
					context: new Context((basecolumn || 0) - indentUnit, 0, "top", false),
					indented: 0,
					startOfLine: true,
					prevToken: null
				};
			},

			token: function(stream, state) {
				let ctx = state.context;
				if (stream.sol()) {
					if (ctx.align == null) ctx.align = false;
					state.indented = stream.indentation();
					state.startOfLine = true;
				}
				if (stream.eatSpace()) return null;
				let style = (state.tokenize || tokenBase)(stream, state);
				if (style == "comment" || style == "meta") return style;
				if (ctx.align == null) ctx.align = true;

				if (endStatement.test(curPunc)) while (isStatement(state.context.type)) popContext(state);
				else if (curPunc == "{") pushContext(state, stream.column(), "}");
				else if (curPunc == "[") pushContext(state, stream.column(), "]");
				else if (curPunc == "(") pushContext(state, stream.column(), ")");
				else if (curPunc == "}") {
					while (isStatement(ctx.type)) ctx = popContext(state);
					if (ctx.type == "}") ctx = popContext(state);
					while (isStatement(ctx.type)) ctx = popContext(state);
				}
				else if (curPunc == ctx.type) popContext(state);
				else if (indentStatements &&
					(((ctx.type == "}" || ctx.type == "top") && curPunc != ";") ||
					 (isStatement(ctx.type) && curPunc == "newstatement"))) {
					let type = "statement";
					if (curPunc == "newstatement" && indentSwitch && stream.current() == "switch")
						type = "switchstatement";
					pushContext(state, stream.column(), type);
				}

				if (hooks.token) {
					let result = hooks.token(stream, state, style);
					if (result !== undefined) style = result;
				}

				state.startOfLine = false;
				state.prevToken = style || curPunc;
				return style;
			},

			indent: function(state, textAfter) {
				if (state.tokenize != tokenBase && state.tokenize != null) return CodeMirror.Pass;
				let ctx = state.context, firstChar = textAfter && textAfter.charAt(0);
				if (isStatement(ctx.type) && firstChar == "}") ctx = ctx.prev;
				if (hooks.indent) {
					let hook = hooks.indent(state, ctx, textAfter);
					if (typeof hook == "number") return hook
				}
				let closing = firstChar == ctx.type;
				let switchBlock = ctx.prev && ctx.prev.type == "switchstatement";
				if (isStatement(ctx.type))
					return ctx.indented + (firstChar == "{" ? 0 : statementIndentUnit);
				if (ctx.align && (!dontAlignCalls || ctx.type != ")"))
					return ctx.column + (closing ? 0 : 1);
				if (ctx.type == ")" && !closing)
					return ctx.indented + statementIndentUnit;

				return ctx.indented + (closing ? 0 : indentUnit) +
					(!closing && switchBlock && !/^(?:case|default)\b/.test(textAfter) ? indentUnit : 0);
			},

			electricInput: indentSwitch ? /^\s*(?:case .*?:|default:|\{\}?|\})$/ : /^\s*[{}]$/,
			blockCommentStart: "/*",
			blockCommentEnd: "*/",
			lineComment: "//",
			fold: "brace"
		};
	});

	CodeMirror.defineMIME( "text/x-huginn", "huginn" );

});
