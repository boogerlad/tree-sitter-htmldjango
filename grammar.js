/**
 * @file HTML+Django template grammar for tree-sitter
 * @author Based on tree-sitter-html by Max Brunsfeld and tree-sitter-django
 * @license MIT
 */

/// <reference types="tree-sitter-cli/dsl" />
// @ts-check

module.exports = grammar({
  name: 'htmldjango',

  word: $ => $.identifier,

  extras: $ => [
    $.comment,
    /\s+/,
  ],

  externals: $ => [
    $._html_start_tag_name,
    $._void_start_tag_name,
    $._foreign_start_tag_name,
    $._script_start_tag_name,
    $._style_start_tag_name,
    $._title_start_tag_name,
    $._textarea_start_tag_name,
    $._plaintext_start_tag_name,
    $._end_tag_name,
    $.erroneous_end_tag_name,
    '/>',
    $._implicit_end_tag,
    $.raw_text,
    $.rcdata_text,
    $.plaintext_text,
    $.comment,
    // Django externals
    $._django_comment_content,
    $._verbatim_start,
    $._verbatim_block_content,
    $._validate_generic_block,
    $._validate_generic_simple,
    $._filter_colon,
  ],

  conflicts: $ => [
    // Django conflicts for unbalanced HTML in conditionals
    [$.django_elif_branch],
    [$.django_else_branch],
    [$.django_empty_branch],
    // With legacy syntax: could continue with 'and' or end
    [$.with_legacy],
    // With assignments: ambiguity in repeat with optional whitespace
    [$.with_assignments],
    // Cycle tag: named reference vs filter expression
    [$.django_cycle_tag, $.lookup],
    // Cycle value vs literal
    [$.cycle_value, $.literal],
    // Unpaired tags vs normal elements
    [$.normal_element, $._start_tag_only],
    // Tag argument can be filter_expression standalone or as part of as_alias
    [$.tag_argument, $.as_alias],
  ],

  supertypes: $ => [
    $.comparison_operator,
    $.element,
    $.django_statement,
    $.literal,
  ],

  rules: {
    document: $ => repeat($._node),

    // ==========================================================================
    // Node types
    // ==========================================================================

    _node: $ => choice(
      $._html_node,
      $._django_node,
    ),

    _html_node: $ => choice(
      $.doctype,
      $.text,
      $.element,
    ),

    _django_node: $ => choice(
      $.django_interpolation,
      $.django_line_comment,
      $.django_block_comment,
      $.django_statement,
    ),

    // ==========================================================================
    // HTML: Doctype
    // ==========================================================================

    doctype: $ => seq(
      '<!',
      alias($._doctype, 'doctype'),
      /[^>]+/,
      '>',
    ),

    _doctype: _ => /[Dd][Oo][Cc][Tt][Yy][Pp][Ee]/,

    // ==========================================================================
    // HTML: Elements
    // ==========================================================================

    element: $ => choice(
      $.void_element,
      $.normal_element,
      $.script_element,
      $.style_element,
      $.rcdata_element,
      $.plaintext_element,
      $.foreign_element,
      $.erroneous_end_tag,
    ),

    void_element: $ => seq(
      '<',
      alias($._void_start_tag_name, $.tag_name),
      repeat($._attribute_node),
      choice('>', '/>'),
    ),

    normal_element: $ => seq(
      '<',
      alias($._html_start_tag_name, $.tag_name),
      repeat($._attribute_node),
      choice('>', '/>'),
      repeat($._node),
      choice($.end_tag, $._implicit_end_tag),
    ),

    // For unbalanced HTML tags inside Django conditionals
    _start_tag_only: $ => seq(
      '<',
      alias($._html_start_tag_name, $.tag_name),
      repeat($._attribute_node),
      choice('>', '/>'),
    ),

    script_element: $ => seq(
      alias($.script_start_tag, $.start_tag),
      optional(alias($._raw_text_with_django, $.raw_text)),
      $.end_tag,
    ),

    _raw_text_with_django: $ => repeat1(choice(
      $.raw_text,
      $.django_interpolation,
      $.django_line_comment,
      $.django_statement,
    )),

    style_element: $ => seq(
      alias($.style_start_tag, $.start_tag),
      optional(alias($._raw_text_with_django, $.raw_text)),
      $.end_tag,
    ),

    rcdata_element: $ => choice(
      seq(
        alias($.title_start_tag, $.start_tag),
        optional($._rcdata_with_django),
        $.end_tag,
      ),
      seq(
        alias($.textarea_start_tag, $.start_tag),
        optional($._rcdata_with_django),
        $.end_tag,
      ),
    ),

    _rcdata_with_django: $ => repeat1(choice(
      $.rcdata_text,
      $.django_interpolation,
      $.django_line_comment,
      $.django_statement,
    )),

    plaintext_element: $ => seq(
      alias($.plaintext_start_tag, $.start_tag),
      optional($.plaintext_text),
    ),

    foreign_element: $ => seq(
      '<',
      alias($._foreign_start_tag_name, $.tag_name),
      repeat($._attribute_node),
      choice(
        seq('>', repeat($._node), choice($.end_tag, $._implicit_end_tag)),
        '/>',
      ),
    ),

    // ==========================================================================
    // HTML: Start/End Tags
    // ==========================================================================

    start_tag: $ => seq(
      '<',
      alias(
        choice(
          $._html_start_tag_name,
          $._void_start_tag_name,
          $._foreign_start_tag_name,
          $._script_start_tag_name,
          $._style_start_tag_name,
          $._title_start_tag_name,
          $._textarea_start_tag_name,
          $._plaintext_start_tag_name,
        ),
        $.tag_name,
      ),
      repeat($._attribute_node),
      choice('>', '/>'),
    ),

    script_start_tag: $ => seq(
      '<',
      alias($._script_start_tag_name, $.tag_name),
      repeat($._attribute_node),
      choice('>', '/>'),
    ),

    style_start_tag: $ => seq(
      '<',
      alias($._style_start_tag_name, $.tag_name),
      repeat($._attribute_node),
      choice('>', '/>'),
    ),

    title_start_tag: $ => seq(
      '<',
      alias($._title_start_tag_name, $.tag_name),
      repeat($._attribute_node),
      choice('>', '/>'),
    ),

    textarea_start_tag: $ => seq(
      '<',
      alias($._textarea_start_tag_name, $.tag_name),
      repeat($._attribute_node),
      choice('>', '/>'),
    ),

    plaintext_start_tag: $ => seq(
      '<',
      alias($._plaintext_start_tag_name, $.tag_name),
      repeat($._attribute_node),
      choice('>', '/>'),
    ),

    end_tag: $ => seq(
      '</',
      alias($._end_tag_name, $.tag_name),
      '>',
    ),

    erroneous_end_tag: $ => seq(
      '</',
      $.erroneous_end_tag_name,
      '>',
    ),

    // ==========================================================================
    // HTML: Attributes
    // ==========================================================================

    _attribute_node: $ => choice(
      $.attribute,
      $.django_interpolation,
      $.django_statement,
      $.django_line_comment,
    ),

    attribute: $ => seq(
      $.attribute_name,
      optional(seq(
        '=',
        choice(
          $.attribute_value,
          $.quoted_attribute_value,
        ),
      )),
    ),

    attribute_name: _ => /[^<>"'/=\s\{%]+/,

    attribute_value: $ => prec.left(repeat1(choice(
      $.entity,
      /[^<>"'=\s\{%]+/,
      $.django_interpolation,
    ))),

    quoted_attribute_value: $ => choice(
      seq(
        "'",
        repeat(choice(
          $.entity,
          alias(/[^'&\{]+/, $.attribute_value),
          // { not starting Django: optionally followed by non-Django, non-quote char and more text
          // The optional group allows lone { at end of attribute (before closing quote)
          alias(token(prec(-1, /\{([^{%#'][^'&\{]*)?/)), $.attribute_value),
          $.django_interpolation,
          $.django_statement,
        )),
        "'",
      ),
      seq(
        '"',
        repeat(choice(
          $.entity,
          alias(/[^"&\{]+/, $.attribute_value),
          // { not starting Django: optionally followed by non-Django, non-quote char and more text
          // The optional group allows lone { at end of attribute (before closing quote)
          alias(token(prec(-1, /\{([^{%#"][^"&\{]*)?/)), $.attribute_value),
          $.django_interpolation,
          $.django_statement,
        )),
        '"',
      ),
    ),

    // ==========================================================================
    // HTML: Text and Entities
    // ==========================================================================

    entity: _ => /&(#x[0-9A-Fa-f]{1,6}|#[0-9]{1,7}|[A-Za-z][A-Za-z0-9]{1,31});/,

    text: $ => prec.right(repeat1(choice(
      $.entity,
      // Single brace that's not start of Django (lookahead simulation)
      token(prec(-1, /\{/)),
      // Greater-than sign can appear in text (after comments, etc.)
      token(prec(-1, />/)),
      // Regular text excluding HTML special chars and braces
      token(/[^<>&\s\{][^<>&\{]*/),
    ))),

    // ==========================================================================
    // Django: Interpolation ({{ expression }})
    // ==========================================================================

    django_interpolation: $ => seq(
      '{{',
      optional($._django_inner_ws),
      optional($.filter_expression),
      optional($._django_inner_ws),
      '}}',
    ),

    // ==========================================================================
    // Django: Comments
    // ==========================================================================

    django_line_comment: _ => token(seq(
      '{#',
      /[^#]*#*([^#}][^#]*#*)*/,
      '#}',
    )),

    django_block_comment: $ => seq(
      $._django_tag_open,
      optional($._django_inner_ws),
      'comment',
      optional(seq($._django_inner_ws, alias(/[^%]+/, $.comment_text))),
      optional($._django_inner_ws),
      $._django_tag_close,
      optional(alias($._django_comment_content, $.comment_content)),
      $._django_tag_open,
      optional($._django_inner_ws),
      'endcomment',
      optional($._django_inner_ws),
      $._django_tag_close,
    ),

    // ==========================================================================
    // Django: Statements
    // ==========================================================================

    django_statement: $ => choice(
      $.django_if_block,
      $.django_for_block,
      $.django_with_block,
      $.django_block_block,
      $.django_extends_tag,
      $.django_include_tag,
      $.django_load_tag,
      $.django_url_tag,
      $.django_csrf_token_tag,
      $.django_autoescape_block,
      $.django_filter_block,
      $.django_spaceless_block,
      $.django_verbatim_block,
      $.django_cycle_tag,
      $.django_firstof_tag,
      $.django_now_tag,
      $.django_regroup_tag,
      $.django_ifchanged_block,
      $.django_widthratio_tag,
      $.django_templatetag_tag,
      $.django_debug_tag,
      $.django_lorem_tag,
      $.django_resetcycle_tag,
      $.django_querystring_tag,
      $.django_partialdef_block,
      $.django_partial_tag,
      $.django_generic_block,
      $.django_generic_tag,
    ),

    // ==========================================================================
    // Django: If Block
    // ==========================================================================

    django_if_block: $ => seq(
      $.django_if_open,
      repeat(choice($._node, alias($._start_tag_only, $.unpaired_start_tag), alias($.end_tag, $.unpaired_end_tag))),
      repeat($.django_elif_branch),
      optional($.django_else_branch),
      $.django_endif,
    ),

    django_if_open: $ => seq(
      $._django_tag_open,
      optional($._django_inner_ws),
      'if',
      $._django_inner_ws,
      $.test_expression,
      optional($._django_inner_ws),
      $._django_tag_close,
    ),

    django_elif_branch: $ => seq(
      $.django_elif,
      repeat(choice($._node, alias($._start_tag_only, $.unpaired_start_tag), alias($.end_tag, $.unpaired_end_tag))),
    ),

    django_elif: $ => seq(
      $._django_tag_open,
      optional($._django_inner_ws),
      'elif',
      $._django_inner_ws,
      $.test_expression,
      optional($._django_inner_ws),
      $._django_tag_close,
    ),

    django_else_branch: $ => seq(
      $.django_else,
      repeat(choice($._node, alias($._start_tag_only, $.unpaired_start_tag), alias($.end_tag, $.unpaired_end_tag))),
    ),

    django_else: $ => seq(
      $._django_tag_open,
      optional($._django_inner_ws),
      'else',
      $._django_tag_close,
    ),

    django_endif: $ => seq(
      $._django_tag_open,
      optional($._django_inner_ws),
      'endif',
      $._django_tag_close,
    ),

    // ==========================================================================
    // Django: For Block
    // ==========================================================================

    django_for_block: $ => seq(
      $.django_for_open,
      repeat(choice($._node, alias($._start_tag_only, $.unpaired_start_tag), alias($.end_tag, $.unpaired_end_tag))),
      optional($.django_empty_branch),
      $.django_endfor,
    ),

    django_for_open: $ => seq(
      $._django_tag_open,
      optional($._django_inner_ws),
      'for',
      $._django_inner_ws,
      $.loop_variables,
      $._django_inner_ws,
      'in',
      $._django_inner_ws,
      $.filter_expression,
      optional(seq($._django_inner_ws, alias('reversed', $.reversed))),
      optional($._django_inner_ws),
      $._django_tag_close,
    ),

    django_empty_branch: $ => seq(
      $.django_empty,
      repeat(choice($._node, alias($._start_tag_only, $.unpaired_start_tag), alias($.end_tag, $.unpaired_end_tag))),
    ),

    django_empty: $ => seq(
      $._django_tag_open,
      optional($._django_inner_ws),
      'empty',
      $._django_tag_close,
    ),

    django_endfor: $ => seq(
      $._django_tag_open,
      optional($._django_inner_ws),
      'endfor',
      $._django_tag_close,
    ),

    loop_variables: $ => prec.left(seq(
      alias($.identifier, $.variable_name),
      repeat(seq(optional($._django_inner_ws), ',', optional($._django_inner_ws), alias($.identifier, $.variable_name))),
    )),

    // ==========================================================================
    // Django: With Block
    // ==========================================================================

    django_with_block: $ => seq(
      $.django_with_open,
      repeat($._node),
      $.django_endwith,
    ),

    django_with_open: $ => seq(
      $._django_tag_open,
      optional($._django_inner_ws),
      'with',
      $._django_inner_ws,
      choice(
        $.with_assignments,
        $.with_legacy,
      ),
      optional($._django_inner_ws),
      $._django_tag_close,
    ),

    with_assignments: $ => repeat1($._spaced_assignment),

    // Assignment with optional leading whitespace - the whitespace is consumed
    // as part of this rule rather than relying on extras
    _spaced_assignment: $ => seq(
      optional($._django_inner_ws),
      $.assignment,
    ),

    with_legacy: $ => seq(
      $.filter_expression,
      optional($._django_inner_ws),
      'as',
      optional($._django_inner_ws),
      alias($.identifier, $.variable_name),
      repeat(seq(
        optional($._django_inner_ws),
        'and',
        optional($._django_inner_ws),
        $.filter_expression,
        optional($._django_inner_ws),
        'as',
        optional($._django_inner_ws),
        alias($.identifier, $.variable_name),
      )),
    ),

    django_endwith: $ => seq(
      $._django_tag_open,
      optional($._django_inner_ws),
      'endwith',
      $._django_tag_close,
    ),

    // ==========================================================================
    // Django: Block Block (template inheritance)
    // ==========================================================================

    django_block_block: $ => seq(
      $.django_block_open,
      repeat($._node),
      $.django_endblock,
    ),

    django_block_open: $ => seq(
      $._django_tag_open,
      optional($._django_inner_ws),
      'block',
      $._django_inner_ws,
      alias($.identifier, $.block_name),
      optional($._django_inner_ws),
      $._django_tag_close,
    ),

    django_endblock: $ => seq(
      $._django_tag_open,
      optional($._django_inner_ws),
      'endblock',
      optional(seq($._django_inner_ws, alias($.identifier, $.block_name))),
      optional($._django_inner_ws),
      $._django_tag_close,
    ),

    // ==========================================================================
    // Django: Extends, Include, Load
    // ==========================================================================

    django_extends_tag: $ => seq(
      $._django_tag_open,
      optional($._django_inner_ws),
      'extends',
      $._django_inner_ws,
      $.filter_expression,
      optional($._django_inner_ws),
      $._django_tag_close,
    ),

    django_include_tag: $ => seq(
      $._django_tag_open,
      optional($._django_inner_ws),
      'include',
      $._django_inner_ws,
      $.filter_expression,
      optional(choice(
        // with ... only
        seq(
          $._django_inner_ws,
          'with',
          repeat1($._spaced_assignment),
          optional(seq(optional($._django_inner_ws), alias('only', $.only))),
        ),
        // only with ... (alternative order)
        seq(
          $._django_inner_ws,
          alias('only', $.only),
          $._django_inner_ws,
          'with',
          repeat1($._spaced_assignment),
        ),
        // only (standalone)
        seq($._django_inner_ws, alias('only', $.only)),
      )),
      optional($._django_inner_ws),
      $._django_tag_close,
    ),

    django_load_tag: $ => seq(
      $._django_tag_open,
      optional($._django_inner_ws),
      'load',
      $._django_inner_ws,
      choice(
        seq(
          repeat1(seq($.library_name, optional($._django_inner_ws))),
          'from',
          $._django_inner_ws,
          $.library_name,
        ),
        repeat1(seq($.library_name, optional($._django_inner_ws))),
      ),
      $._django_tag_close,
    ),

    library_name: _ => /[A-Za-z_][\w.]*/,

    // ==========================================================================
    // Django: URL
    // ==========================================================================

    django_url_tag: $ => seq(
      $._django_tag_open,
      optional($._django_inner_ws),
      'url',
      $._django_inner_ws,
      $.filter_expression,
      repeat(seq(
        $._django_inner_ws,
        choice($.named_argument, $.filter_expression),
      )),
      optional(seq($._django_inner_ws, 'as', $._django_inner_ws, alias($.identifier, $.variable_name))),
      optional($._django_inner_ws),
      $._django_tag_close,
    ),

    named_argument: $ => prec.dynamic(1, seq(
      alias($.identifier, $.argument_name),
      '=',
      $.filter_expression,
    )),

    // ==========================================================================
    // Django: CSRF Token
    // ==========================================================================

    django_csrf_token_tag: $ => seq(
      $._django_tag_open,
      optional($._django_inner_ws),
      'csrf_token',
      optional($._django_inner_ws),
      $._django_tag_close,
    ),

    // ==========================================================================
    // Django: Autoescape Block
    // ==========================================================================

    django_autoescape_block: $ => seq(
      $._django_tag_open,
      optional($._django_inner_ws),
      'autoescape',
      $._django_inner_ws,
      choice('on', 'off'),
      optional($._django_inner_ws),
      $._django_tag_close,
      repeat($._node),
      $._django_tag_open,
      optional($._django_inner_ws),
      'endautoescape',
      optional($._django_inner_ws),
      $._django_tag_close,
    ),

    // ==========================================================================
    // Django: Filter Block
    // ==========================================================================

    django_filter_block: $ => seq(
      $._django_tag_open,
      optional($._django_inner_ws),
      'filter',
      $._django_inner_ws,
      $.filter_chain,
      optional($._django_inner_ws),
      $._django_tag_close,
      repeat($._node),
      $._django_tag_open,
      optional($._django_inner_ws),
      'endfilter',
      optional($._django_inner_ws),
      $._django_tag_close,
    ),

    filter_chain: $ => prec.left(seq(
      $.filter_call,
      repeat(seq(alias($._filter_pipe, '|'), $.filter_call)),
    )),

    // ==========================================================================
    // Django: Spaceless Block
    // ==========================================================================

    django_spaceless_block: $ => seq(
      $._django_tag_open,
      optional($._django_inner_ws),
      'spaceless',
      optional($._django_inner_ws),
      $._django_tag_close,
      repeat($._node),
      $._django_tag_open,
      optional($._django_inner_ws),
      'endspaceless',
      optional($._django_inner_ws),
      $._django_tag_close,
    ),

    // ==========================================================================
    // Django: Verbatim Block (HTML parsed, Django not parsed)
    // ==========================================================================

    // Verbatim block using external scanner for exact suffix matching.
    // The external scanner captures the suffix after "verbatim" and ensures
    // {% endverbatim<suffix> %} matches exactly.
    django_verbatim_block: $ => seq(
      $._django_tag_open,
      optional($._django_inner_ws),
      'verbatim',
      $._verbatim_start,  // External: captures suffix, consumes up to and including %}
      optional($._verbatim_block_content),  // External: scans until matching endverbatim
    ),

    // ==========================================================================
    // Django: Cycle Tag
    // ==========================================================================

    django_cycle_tag: $ => seq(
      $._django_tag_open,
      optional($._django_inner_ws),
      'cycle',
      $._django_inner_ws,
      choice(
        alias($.identifier, $.cycle_name),
        seq(
          $.cycle_value,
          repeat(seq($._django_inner_ws, $.cycle_value)),
          optional(seq($._django_inner_ws, 'as', $._django_inner_ws, alias($.identifier, $.variable_name), optional(seq($._django_inner_ws, alias('silent', $.silent))))),
        ),
      ),
      optional($._django_inner_ws),
      $._django_tag_close,
    ),

    cycle_value: $ => choice($.string, $.filter_expression),

    // ==========================================================================
    // Django: Firstof Tag
    // ==========================================================================

    django_firstof_tag: $ => seq(
      $._django_tag_open,
      optional($._django_inner_ws),
      'firstof',
      repeat1(seq($._django_inner_ws, $.filter_expression)),
      optional(seq($._django_inner_ws, 'as', $._django_inner_ws, alias($.identifier, $.variable_name))),
      optional($._django_inner_ws),
      $._django_tag_close,
    ),

    // ==========================================================================
    // Django: Now Tag
    // ==========================================================================

    django_now_tag: $ => seq(
      $._django_tag_open,
      optional($._django_inner_ws),
      'now',
      $._django_inner_ws,
      $.string,
      optional(seq($._django_inner_ws, 'as', $._django_inner_ws, alias($.identifier, $.variable_name))),
      optional($._django_inner_ws),
      $._django_tag_close,
    ),

    // ==========================================================================
    // Django: Regroup Tag
    // ==========================================================================

    django_regroup_tag: $ => seq(
      $._django_tag_open,
      optional($._django_inner_ws),
      'regroup',
      $._django_inner_ws,
      $.filter_expression,
      $._django_inner_ws,
      'by',
      $._django_inner_ws,
      $.lookup,
      $._django_inner_ws,
      'as',
      $._django_inner_ws,
      alias($.identifier, $.variable_name),
      optional($._django_inner_ws),
      $._django_tag_close,
    ),

    // ==========================================================================
    // Django: Ifchanged Block
    // ==========================================================================

    django_ifchanged_block: $ => seq(
      $._django_tag_open,
      optional($._django_inner_ws),
      'ifchanged',
      optional(repeat1(seq($._django_inner_ws, $.filter_expression))),
      optional($._django_inner_ws),
      $._django_tag_close,
      repeat($._node),
      optional(seq(
        $._django_tag_open,
        optional($._django_inner_ws),
        'else',
        optional($._django_inner_ws),
        $._django_tag_close,
        repeat($._node),
      )),
      $._django_tag_open,
      optional($._django_inner_ws),
      'endifchanged',
      optional($._django_inner_ws),
      $._django_tag_close,
    ),

    // ==========================================================================
    // Django: Widthratio Tag
    // ==========================================================================

    django_widthratio_tag: $ => seq(
      $._django_tag_open,
      optional($._django_inner_ws),
      'widthratio',
      $._django_inner_ws,
      $.filter_expression,
      $._django_inner_ws,
      $.filter_expression,
      $._django_inner_ws,
      $.filter_expression,
      optional(seq($._django_inner_ws, 'as', $._django_inner_ws, alias($.identifier, $.variable_name))),
      optional($._django_inner_ws),
      $._django_tag_close,
    ),

    // ==========================================================================
    // Django: Templatetag Tag
    // ==========================================================================

    django_templatetag_tag: $ => seq(
      $._django_tag_open,
      optional($._django_inner_ws),
      'templatetag',
      $._django_inner_ws,
      choice(
        'openblock',
        'closeblock',
        'openvariable',
        'closevariable',
        'openbrace',
        'closebrace',
        'opencomment',
        'closecomment',
      ),
      optional($._django_inner_ws),
      $._django_tag_close,
    ),

    // ==========================================================================
    // Django: Debug Tag
    // ==========================================================================

    django_debug_tag: $ => seq(
      $._django_tag_open,
      optional($._django_inner_ws),
      'debug',
      optional($._django_inner_ws),
      $._django_tag_close,
    ),

    // ==========================================================================
    // Django: Lorem Tag
    // ==========================================================================

    django_lorem_tag: $ => seq(
      $._django_tag_open,
      optional($._django_inner_ws),
      'lorem',
      optional(seq($._django_inner_ws, $.filter_expression)),
      optional(seq($._django_inner_ws, alias(choice('w', 'p', 'b'), $.method))),
      optional(seq($._django_inner_ws, alias('random', $.random))),
      optional($._django_inner_ws),
      $._django_tag_close,
    ),

    // ==========================================================================
    // Django: Resetcycle Tag
    // ==========================================================================

    django_resetcycle_tag: $ => seq(
      $._django_tag_open,
      optional($._django_inner_ws),
      'resetcycle',
      optional(seq($._django_inner_ws, alias($.identifier, $.cycle_name))),
      optional($._django_inner_ws),
      $._django_tag_close,
    ),

    // ==========================================================================
    // Django: Querystring Tag
    // ==========================================================================

    django_querystring_tag: $ => seq(
      $._django_tag_open,
      optional($._django_inner_ws),
      'querystring',
      repeat(seq($._django_inner_ws, choice($.named_argument, $.filter_expression))),
      optional($._django_inner_ws),
      $._django_tag_close,
    ),

    // ==========================================================================
    // Django: Partialdef Block
    // ==========================================================================

    django_partialdef_block: $ => seq(
      $._django_tag_open,
      optional($._django_inner_ws),
      'partialdef',
      $._django_inner_ws,
      alias($.identifier, $.partial_name),
      optional(seq($._django_inner_ws, alias('inline', $.inline))),
      optional($._django_inner_ws),
      $._django_tag_close,
      repeat($._node),
      $._django_tag_open,
      optional($._django_inner_ws),
      'endpartialdef',
      optional(seq($._django_inner_ws, alias($.identifier, $.partial_name))),
      optional($._django_inner_ws),
      $._django_tag_close,
    ),

    // ==========================================================================
    // Django: Partial Tag
    // ==========================================================================

    django_partial_tag: $ => seq(
      $._django_tag_open,
      optional($._django_inner_ws),
      'partial',
      $._django_inner_ws,
      alias($.identifier, $.partial_name),
      optional($._django_inner_ws),
      $._django_tag_close,
    ),

    // ==========================================================================
    // Django: Generic Block and Tag (fallback for unknown tags)
    // ==========================================================================

    django_generic_block: $ => prec.dynamic(-1, seq(
      $._django_tag_open,
      optional($._django_inner_ws),
      $._validate_generic_block,
      field('name', alias($.identifier, $.generic_tag_name)),
      repeat(seq($._django_inner_ws, $.tag_argument)),
      optional($._django_inner_ws),
      $._django_tag_close,
      repeat($._node),
      $._django_tag_open,
      optional($._django_inner_ws),
      field('end_name', alias(token(seq('end', /[a-zA-Z_][a-zA-Z0-9_]*/)), $.end_tag_name)),
      optional($._django_inner_ws),
      $._django_tag_close,
    )),

    django_generic_tag: $ => prec(-1, seq(
      $._django_tag_open,
      optional($._django_inner_ws),
      $._validate_generic_simple,
      alias($.identifier, $.generic_tag_name),
      repeat(seq($._django_inner_ws, $.tag_argument)),
      optional($._django_inner_ws),
      $._django_tag_close,
    )),

    tag_argument: $ => choice(
      $.assignment,
      $.as_alias,
      $.filter_expression,
    ),

    as_alias: $ => seq(
      $.filter_expression,
      $._django_inner_ws,
      'as',
      $._django_inner_ws,
      alias($.identifier, $.variable_name),
    ),

    // ==========================================================================
    // Django: Expressions
    // ==========================================================================

    filter_expression: $ => seq(
      $.primary_expression,
      repeat(seq(alias($._filter_pipe, '|'), $.filter_call)),
    ),

    _filter_pipe: _ => token(prec(1, /[ \t\r\n]*\|/)),

    primary_expression: $ => choice(
      $.literal,
      $.lookup,
    ),

    literal: $ => choice(
      $.string,
      $.number,
      $.i18n_string,
    ),

    lookup: $ => seq(
      $.identifier,
      repeat(seq('.', choice($.identifier, $.numeric_index))),
    ),

    numeric_index: _ => /\d+/,

    filter_call: $ => prec.left(seq(
      alias($.identifier, $.filter_name),
      optional(seq(
        alias($._filter_colon, ':'),
        $.filter_argument,
      )),
    )),

    filter_argument: $ => choice(
      $.literal,
      $.lookup,
    ),

    assignment: $ => seq(
      alias($.identifier, $.variable_name),
      '=',
      $.filter_expression,
    ),

    // ==========================================================================
    // Django: Test Expressions (for {% if %})
    // ==========================================================================

    test_expression: $ => $.or_expression,

    or_expression: $ => prec.left(1, seq(
      $.and_expression,
      repeat(seq($.or_keyword, $.and_expression)),
    )),

    or_keyword: _ => token(prec(10, 'or')),

    and_expression: $ => prec.left(2, seq(
      $.not_expression,
      repeat(seq($.and_keyword, $.not_expression)),
    )),

    and_keyword: _ => token(prec(10, 'and')),

    not_expression: $ => choice(
      prec(3, seq(token(prec(10, 'not')), $.not_expression)),
      $.comparison_expression,
    ),

    comparison_expression: $ => prec.left(4, seq(
      $.filter_expression,
      repeat(seq(
        $.comparison_operator,
        $.filter_expression,
      )),
    )),

    // Supertype for comparison operators - allows queries like (comparison_operator)
    comparison_operator: $ => choice(
      $.op_not_in,
      $.op_is_not,
      $.op_in,
      $.op_is,
      $.op_eq,
      $.op_ne,
      $.op_gte,
      $.op_gt,
      $.op_lte,
      $.op_lt,
    ),

    // Individual comparison operators as named nodes
    op_not_in: _ => token(prec(10, seq('not', /[ \t\r\n]+/, 'in'))),
    op_is_not: _ => token(prec(10, seq('is', /[ \t\r\n]+/, 'not'))),
    op_in: _ => token(prec(10, 'in')),
    op_is: _ => token(prec(10, 'is')),
    op_eq: _ => token(prec(10, '==')),
    op_ne: _ => token(prec(10, '!=')),
    op_gte: _ => token(prec(10, '>=')),
    op_gt: _ => token(prec(10, '>')),
    op_lte: _ => token(prec(10, '<=')),
    op_lt: _ => token(prec(10, '<')),

    // ==========================================================================
    // Django: Tokens
    // ==========================================================================

    // Identifier that excludes Django keywords (and, or, not, in, is, as)
    identifier: _ => token(prec(-1, /[a-zA-Z_][a-zA-Z0-9_]*/)),

    number: _ => token(prec(1, seq(
      optional(choice('+', '-')),
      choice(
        seq(/\d+\.\d+/, optional(seq(/[eE]/, optional(choice('+', '-')), /\d+/))),
        seq(/\d+\./, /[eE]/, optional(choice('+', '-')), /\d+/),
        seq(/\.\d+/, optional(seq(/[eE]/, optional(choice('+', '-')), /\d+/))),
        seq(/\d+/, optional(seq(/[eE]/, optional(choice('+', '-')), /\d+/))),
      ),
    ))),

    string: _ => choice(
      seq("'", repeat(choice(/[^'\\]/, /\\./)), "'"),
      seq('"', repeat(choice(/[^"\\]/, /\\./)), '"'),
    ),

    i18n_string: _ => token(seq(
      '_(',
      choice(
        seq("'", repeat(choice(/[^'\\]/, /\\./)), "'"),
        seq('"', repeat(choice(/[^"\\]/, /\\./)), '"'),
      ),
      ')',
    )),

    // ==========================================================================
    // Django: Tag delimiters (with optional whitespace trimming)
    // ==========================================================================

    // Django tag open: {% (strict Django DTL - no whitespace trim markers)
    _django_tag_open: _ => '{%',

    // Django tag close: %} (strict Django DTL - no whitespace trim markers)
    _django_tag_close: _ => '%}',

    // ==========================================================================
    // Django: Whitespace handling
    // ==========================================================================

    _django_inner_ws: _ => /[ \t\r\n]+/,
  },
});
