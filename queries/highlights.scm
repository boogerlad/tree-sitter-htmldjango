; =============================================================================
; HTML Highlighting
; =============================================================================

(tag_name) @tag
(erroneous_end_tag_name) @tag.error
(doctype) @constant
(attribute_name) @attribute
(attribute_value) @string
(comment) @comment

[
  "<"
  ">"
  "</"
  "/>"
] @punctuation.bracket

"=" @punctuation.delimiter

(entity) @string.special

; =============================================================================
; Django Delimiters
; =============================================================================

[
  "{{"
  "}}"
] @punctuation.special

[
  "{%"
  "%}"
] @tag.delimiter

; =============================================================================
; Django Comments
; =============================================================================

(django_line_comment) @comment
(django_block_comment) @comment
(comment_content) @comment
(comment_text) @comment

; =============================================================================
; Django Keywords - Control Flow
; =============================================================================

[
  "if"
  "elif"
  "else"
  "endif"
  "for"
  "endfor"
  "empty"
] @keyword.conditional

[
  "in"
] @keyword

; Named optional keyword nodes
[
  (reversed)
  (only)
  (silent)
  (random)
  (inline)
] @keyword

; =============================================================================
; Django Keywords - Block Tags
; =============================================================================

[
  "block"
  "endblock"
  "extends"
  "include"
  "with"
  "endwith"
  "autoescape"
  "endautoescape"
  "filter"
  "endfilter"
  "spaceless"
  "endspaceless"
  "verbatim"
  "comment"
  "endcomment"
  "ifchanged"
  "endifchanged"
  "partialdef"
  "endpartialdef"
] @keyword

; =============================================================================
; Django Keywords - Simple Tags
; =============================================================================

[
  "load"
  "url"
  "csrf_token"
  "cycle"
  "firstof"
  "now"
  "regroup"
  "widthratio"
  "templatetag"
  "debug"
  "lorem"
  "resetcycle"
  "querystring"
  "partial"
] @keyword

; =============================================================================
; Django Keywords - Modifiers
; =============================================================================

[
  "as"
  "from"
  "by"
  "on"
  "off"
  "and"
] @keyword

; =============================================================================
; Django Keywords - Templatetag Arguments
; =============================================================================

[
  "openblock"
  "closeblock"
  "openvariable"
  "closevariable"
  "openbrace"
  "closebrace"
  "opencomment"
  "closecomment"
] @string.special

; =============================================================================
; Django Keywords - Lorem Methods
; =============================================================================

(method) @string.special

; =============================================================================
; Django Boolean Operators
; =============================================================================

(and_keyword) @keyword.operator
(or_keyword) @keyword.operator

[
  "not"
] @keyword.operator

; =============================================================================
; Django Comparison Operators
; =============================================================================

(comparison_operator) @operator

; =============================================================================
; Django Identifiers and Variables
; =============================================================================

; Block names
(block_name) @variable.parameter

; Loop variables
(variable_name) @variable.parameter

; Generic identifiers (variables)
(lookup
  (identifier) @variable)

(lookup
  (numeric_index) @number)

; Named arguments
(argument_name) @variable.parameter

; Cycle names
(cycle_name) @variable.parameter

; Partial names
(partial_name) @variable.parameter

; =============================================================================
; Django Filters
; =============================================================================

(filter_call
  (filter_name) @function)

(filter_chain
  (filter_call
    (filter_name) @function))

; =============================================================================
; Django Library Names (load tag)
; =============================================================================

(library_name) @module

; =============================================================================
; Django Generic Tag Names
; =============================================================================

(generic_tag_name) @function.macro

(end_tag_name) @function.macro

; =============================================================================
; Django Literals
; =============================================================================

(string) @string
(number) @number
(i18n_string) @string.special

; =============================================================================
; Django Punctuation
; =============================================================================

"|" @punctuation.delimiter
":" @punctuation.delimiter
"." @punctuation.delimiter
"," @punctuation.delimiter
