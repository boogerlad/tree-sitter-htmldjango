/**
 * @file HTML grammar for tree-sitter
 * @author Max Brunsfeld <maxbrunsfeld@gmail.com>
 * @author Amaan Qureshi <amaanq12@gmail.com>
 * @license MIT
 */

/// <reference types="tree-sitter-cli/dsl" />
// @ts-check

module.exports = grammar({
  name: 'html',

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
  ],

  rules: {
    document: $ => repeat($._node),

    doctype: $ => seq(
      '<!',
      alias($._doctype, 'doctype'),
      /[^>]+/,
      '>',
    ),

    _doctype: _ => /[Dd][Oo][Cc][Tt][Yy][Pp][Ee]/,

    _node: $ => choice(
      $.doctype,
      $.text,
      $.element,
    ),

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
      repeat($.attribute),
      choice('>', '/>'),
    ),

    normal_element: $ => seq(
      '<',
      alias($._html_start_tag_name, $.tag_name),
      repeat($.attribute),
      choice('>', '/>'),
      repeat($._node),
      choice($.end_tag, $._implicit_end_tag),
    ),

    script_element: $ => seq(
      alias($.script_start_tag, $.start_tag),
      optional($.raw_text),
      $.end_tag,
    ),

    style_element: $ => seq(
      alias($.style_start_tag, $.start_tag),
      optional($.raw_text),
      $.end_tag,
    ),

    rcdata_element: $ => choice(
      seq(
        alias($.title_start_tag, $.start_tag),
        optional($.rcdata_text),
        $.end_tag,
      ),
      seq(
        alias($.textarea_start_tag, $.start_tag),
        optional($.rcdata_text),
        $.end_tag,
      ),
    ),

    plaintext_element: $ => seq(
      alias($.plaintext_start_tag, $.start_tag),
      optional($.plaintext_text),
    ),

    foreign_element: $ => seq(
      '<',
      alias($._foreign_start_tag_name, $.tag_name),
      repeat($.attribute),
      choice(
        seq('>', repeat($._node), choice($.end_tag, $._implicit_end_tag)),
        '/>',
      ),
    ),

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
      repeat($.attribute),
      choice('>', '/>'),
    ),

    script_start_tag: $ => seq(
      '<',
      alias($._script_start_tag_name, $.tag_name),
      repeat($.attribute),
      choice('>', '/>'),
    ),

    style_start_tag: $ => seq(
      '<',
      alias($._style_start_tag_name, $.tag_name),
      repeat($.attribute),
      choice('>', '/>'),
    ),

    title_start_tag: $ => seq(
      '<',
      alias($._title_start_tag_name, $.tag_name),
      repeat($.attribute),
      choice('>', '/>'),
    ),

    textarea_start_tag: $ => seq(
      '<',
      alias($._textarea_start_tag_name, $.tag_name),
      repeat($.attribute),
      choice('>', '/>'),
    ),

    plaintext_start_tag: $ => seq(
      '<',
      alias($._plaintext_start_tag_name, $.tag_name),
      repeat($.attribute),
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

    attribute_name: _ => /[^<>"'/=\s]+/,

    attribute_value: _ => /[^<>"'=\s]+/,

    // 13.1.4 Character references – authoring syntax
    // &name;  or  &#123;  or  &#x1F600;
    entity: _ => /&(#x[0-9A-Fa-f]{1,6}|#[0-9]{1,7}|[A-Za-z][A-Za-z0-9]{1,31});/,

    text: $ => prec.right(repeat1(choice(
      $.entity,
      token(/[^<&\s]([^<&]*[^<&\s])?/),
    ))),

    attribute_value: $ => repeat1(choice(
      $.entity,
      token(/[^<>"'=&\s]+/),
    )),

    quoted_attribute_value: $ => choice(
      seq('\'', repeat(choice($.entity, alias(/[^&']+/, $.attribute_value))), '\''),
      seq('"', repeat(choice($.entity, alias(/[^&"]+/, $.attribute_value))), '"'),
    ),
  },
});
