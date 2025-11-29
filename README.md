# tree-sitter-htmldjango

HTML + Django template grammar for [tree-sitter](https://github.com/tree-sitter/tree-sitter).

## Overview

This grammar provides full parsing support for HTML documents containing Django template syntax, including:

- **Django template tags**: `{% if %}`, `{% for %}`, `{% block %}`, `{% include %}`, `{% extends %}`, etc.
- **Django template variables**: `{{ variable }}`, `{{ object.attribute }}`
- **Django filters**: `{{ value|lower }}`, `{{ value|default:"fallback" }}`
- **Django comments**: `{# line comment #}`, `{% comment %}block comment{% endcomment %}`
- **Complete HTML5 support**: All standard HTML elements, attributes, and entities
- **Mixed content**: Django syntax within HTML attributes, scripts, and styles

## Features

- Parses both HTML and Django template syntax in a single tree
- Handles unbalanced HTML tags within Django conditionals
- Supports Django's `{% verbatim %}` blocks
- Comprehensive syntax highlighting queries
- JavaScript/CSS injection support for `<script>` and `<style>` elements

## Installation

### npm

```bash
npm install tree-sitter-htmldjango
```

### Cargo

```toml
[dependencies]
tree-sitter-htmldjango = "0.1"
```

### pip

```bash
pip install tree-sitter-htmldjango
```

## Usage

### Node.js

```javascript
const Parser = require('tree-sitter');
const HTMLDjango = require('tree-sitter-htmldjango');

const parser = new Parser();
parser.setLanguage(HTMLDjango);

const tree = parser.parse(`
{% if user.is_authenticated %}
  <p>Hello, {{ user.name|title }}!</p>
{% endif %}
`);
```

### Rust

```rust
let code = r#"
{% if user.is_authenticated %}
  <p>Hello, {{ user.name|title }}!</p>
{% endif %}
"#;

let mut parser = tree_sitter::Parser::new();
parser.set_language(&tree_sitter_htmldjango::LANGUAGE.into())?;
let tree = parser.parse(code, None).unwrap();
```

### Python

```python
import tree_sitter_htmldjango as ts_htmldjango
from tree_sitter import Language, Parser

parser = Parser(Language(ts_htmldjango.language()))
tree = parser.parse(b"""
{% if user.is_authenticated %}
  <p>Hello, {{ user.name|title }}!</p>
{% endif %}
""")
```

## Supported Django Tags

### Built-in Tags
- Control flow: `if`, `elif`, `else`, `endif`, `for`, `empty`, `endfor`
- Template inheritance: `block`, `endblock`, `extends`, `include`
- Variable scoping: `with`, `endwith`
- Output control: `autoescape`, `endautoescape`, `filter`, `endfilter`, `spaceless`, `endspaceless`
- Comments: `comment`, `endcomment`
- Utilities: `load`, `url`, `csrf_token`, `cycle`, `firstof`, `now`, `debug`, `lorem`, `verbatim`
- And more...

### Generic Tags
Unknown tags are parsed as `django_generic_tag` or `django_generic_block`, allowing the grammar to handle custom template tags from third-party libraries.

## Querying

The grammar contains several [supertypes](https://tree-sitter.github.io/tree-sitter/using-parsers#static-node-types),
which group multiple node types under a single name.

Supertype names do not appear as wrapper nodes in parse trees, but they can be used in queries in special ways:

- As an alias, matching any of their subtypes
- As a prefix for one of their subtypes using the `supertype/subtype` syntax

### Example

For the template `{% if a == b %}<div>hello</div>{% endif %}`, the parse tree is:

```
(document
  (django_if_block
    (django_if_open
      (test_expression
        (or_expression
          (and_expression
            (not_expression
              (comparison_expression
                (filter_expression
                  (primary_expression
                    (lookup (identifier))))
                (op_eq)
                (filter_expression
                  (primary_expression
                    (lookup (identifier))))))))))
    (normal_element
      (tag_name)
      (text)
      (end_tag (tag_name)))
    (django_endif)))
```

The query `(element)` matches `normal_element` (and would match `void_element`, `script_element`, etc. if present).

The query `(django_statement)` matches `django_if_block` (and would match `django_for_block`, `django_with_block`, etc.).

The query `(comparison_operator)` matches `op_eq` (and would match `op_ne`, `op_gt`, `op_in`, etc.).

### Supertypes

- **`element`**

  Any HTML element type. Subtypes include:
  - `normal_element` - Standard elements like `<div>`, `<span>`, `<p>`
  - `void_element` - Self-closing elements like `<br>`, `<img>`, `<input>`
  - `script_element` - `<script>` elements with raw content
  - `style_element` - `<style>` elements with raw content
  - `rcdata_element` - `<title>`, `<textarea>` with RCDATA content
  - `foreign_element` - SVG and MathML elements
  - `plaintext_element` - Deprecated `<plaintext>` element
  - `erroneous_end_tag` - Unmatched closing tags

- **`django_statement`**

  Any Django template tag or block. Subtypes include:
  - Control flow: `django_if_block`, `django_for_block`
  - Template inheritance: `django_block_block`, `django_extends_tag`, `django_include_tag`
  - Variable scoping: `django_with_block`
  - Output control: `django_autoescape_block`, `django_filter_block`, `django_spaceless_block`
  - Comments: `django_block_comment`
  - Utilities: `django_load_tag`, `django_url_tag`, `django_csrf_token_tag`, `django_cycle_tag`
  - Generic: `django_generic_tag`, `django_generic_block` (for custom/unknown tags)
  - And many more...

- **`literal`**

  Any literal value in Django expressions. Subtypes:
  - `string` - String literals like `"hello"` or `'world'`
  - `number` - Numeric literals like `42` or `3.14`
  - `i18n_string` - Translated strings like `_("Hello")`

- **`comparison_operator`**

  Any comparison operator in Django expressions. Subtypes:
  - `op_eq` - `==`
  - `op_ne` - `!=`
  - `op_lt` - `<`
  - `op_lte` - `<=`
  - `op_gt` - `>`
  - `op_gte` - `>=`
  - `op_in` - `in`
  - `op_not_in` - `not in`
  - `op_is` - `is`
  - `op_is_not` - `is not`

### Query Examples

```scheme
; Match any HTML element
(element) @element

; Match only div elements specifically
(element/normal_element
  (tag_name) @tag
  (#eq? @tag "div")) @div

; Match any Django control structure
(django_statement) @statement

; Match all comparison operators for highlighting
(comparison_operator) @operator

; Match all literals in Django expressions
(literal) @literal

; Match string literals specifically
(literal/string) @string
```

## References

- [Django Template Language](https://docs.djangoproject.com/en/stable/ref/templates/language/)
- [Django Built-in Tags](https://docs.djangoproject.com/en/stable/ref/templates/builtins/)
- [HTML5 Specification](https://html.spec.whatwg.org/)
- [tree-sitter Documentation](https://tree-sitter.github.io/)

## License

MIT
