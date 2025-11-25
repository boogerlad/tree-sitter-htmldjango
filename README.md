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

## References

- [Django Template Language](https://docs.djangoproject.com/en/stable/ref/templates/language/)
- [Django Built-in Tags](https://docs.djangoproject.com/en/stable/ref/templates/builtins/)
- [HTML5 Specification](https://html.spec.whatwg.org/)
- [tree-sitter Documentation](https://tree-sitter.github.io/)

## License

MIT
