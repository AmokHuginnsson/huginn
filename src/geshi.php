<?php
/*************************************************************************************
 * huginn.php
 * ----------
 * Author: Marcin Konarski (amok (at) codestation.org)
 * Copyright: (c) 2018 Marcin Konarski (https://codestation.org)
 * Release Version: 1.0.9.0
 * Date Started: 2018/03/07
 *
 * Huginn language file for GeSHi.
 * Huginn programming language website: https://huginn.org/
 * Huginn programming reference:
 * https://huginn.org/?h-action=bar-huginn&huginn=huginn-reference&menu=submenu-project&page=&project=huginn
 *
 * CHANGES
 * -------
 * 2018/03/07 (1.0.0)
 *  -  First Release
 *
 *************************************************************************************
 *
 *     This file is part of GeSHi.
 *
 *   GeSHi is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   GeSHi is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with GeSHi; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 ************************************************************************************/

$language_data = array (
    'LANG_NAME' => 'Huginn',
    'COMMENT_SINGLE' => array(1 => '//'),
    'COMMENT_MULTI' => array('/*' => '*/'),
    'CASE_KEYWORDS' => GESHI_CAPS_NO_CHANGE,
    //Longest quotemarks ALWAYS first
    'QUOTEMARKS' => array('"""', "'''", '"', "'"),
    'ESCAPE_CHAR' => '\\',
    'NUMBERS' =>
        GESHI_NUMBER_INT_BASIC | GESHI_NUMBER_BIN_PREFIX_0B |
        GESHI_NUMBER_OCT_PREFIX_0O | GESHI_NUMBER_HEX_PREFIX |
        GESHI_NUMBER_FLT_NONSCI | GESHI_NUMBER_FLT_NONSCI_F |
        GESHI_NUMBER_FLT_SCI_SHORT | GESHI_NUMBER_FLT_SCI_ZERO,
    'KEYWORDS' => array(

        /*
        ** Set 1: reserved words
        */
        1 => array(
            'assert', 'break', 'case', 'catch', 'class', 'constructor', 'continue', 'default', 'destructor',
            'else', 'for', 'if', 'return', 'super', 'switch', 'this', 'throw', 'try', 'while'
            ),
        2 => array(
            'none', 'true', 'false'
            ),

        /*
        ** Set 2: builtins
        */
        3 => array(
            'blob', 'boolean', 'character', 'copy', 'deque', 'dict', 'integer', 'list', 'lookup', 'number',
            'observe', 'order', 'real', 'set', 'size', 'string', 'tuple', 'type', 'use'
            ),

        /*
        ** Set 3: import
        */
        4 => array(
            'import', 'as'
            ),

        /*
        ** Set 4: special methods
        */
        5 => array(
            'get_size', 'clone', 'hash', 'iterator', 'is_valid', 'next', 'value',
            'to_string', 'to_integer', 'to_number', 'to_character', 'to_boolean', 'to_real',
            'equals','less','less_or_equal','greater','greater_or_equal',
            'add','subtract','multiply','divide','modulo','power','negate',
            'modulus'
            )
        ),
    'SYMBOLS' => array(
        '+=', '+', '-=', '-', '*=', '*', '/=', '/', '%=', '%', '^=', '^',
        '||', '⋁', '|', '&&', '⋀', '∈',
        '<=', '>=', '==', '!=', '≠', '≤', '≥', '<', '>', '=', '!', '¬',
        '@', ':', ';', ','
        ),
    'CASE_SENSITIVE' => array(
        GESHI_COMMENTS => false,
        1 => true,
        2 => true,
        3 => true,
        4 => true,
        5 => true
        ),
    'STYLES' => array(
        'KEYWORDS' => array(
            1 => 'color: #990;font-weight:bold;',
            2 => 'color: #e0e;',
            3 => 'color: #080;font-weight:bold;',
            4 => 'color: #00f;',
            5 => 'color: #440;'
            ),
        'COMMENTS' => array(
            1 => 'color: #088; font-style: italic;',
            'MULTI' => 'color: #088; font-style: italic;'
            ),
        'ESCAPE_CHAR' => array(
            0 => 'color: #a00; font-weight: bold;'
            ),
        'BRACKETS' => array(
            0 => 'color: #666;'
            ),
        'STRINGS' => array(
            0 => 'color: #e0e;'
            ),
        'NUMBERS' => array(
            0 => 'color: #e0e;'
            ),
        'METHODS' => array(
            1 => 'color: black;'
            ),
        'SYMBOLS' => array(
            0 => 'color: #666;'
            ),
        'REGEXPS' => array(
            0 => 'color: #0cc;',
            1 => 'color: #620;',
            2 => 'color: #00f;',
            3 => 'color: #0b0;'
            ),
        'SCRIPT' => array(
            )
        ),
    'URLS' => array(
        1 => '',
        2 => '',
        3 => '',
        4 => ''
        ),
    'OOLANG' => true,
    'OBJECT_SPLITTERS' => array(
        1 => '.'
        ),
    'REGEXPS' => array(
        0 => "\\b[A-Z][A-Z0-9_]*[A-Z0-9]\\b",
        1 => "\\b[A-Z][a-zA-Z0-9_]*[a-z][a-zA-Z0-9_]*\\b",
        2 => "\\b_[a-zA-Z][a-zA-Z0-9_]*\\b",
        3 => "\\b[a-zA-Z][a-zA-Z0-9_]*_\\b"
        ),
    'STRICT_MODE_APPLIES' => GESHI_NEVER,
    'SCRIPT_DELIMITERS' => array(
        ),
    'HIGHLIGHT_STRICT_BLOCK' => array(
        ),
    'TAB_WIDTH' => 2
);
