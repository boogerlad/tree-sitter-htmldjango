#include "tag.h"
#include "tree_sitter/parser.h"

#include <wctype.h>

enum TokenType {
    HTML_START_TAG_NAME,
    VOID_START_TAG_NAME,
    FOREIGN_START_TAG_NAME,
    SCRIPT_START_TAG_NAME,
    STYLE_START_TAG_NAME,
    TITLE_START_TAG_NAME,
    TEXTAREA_START_TAG_NAME,
    PLAINTEXT_START_TAG_NAME,
    END_TAG_NAME,
    ERRONEOUS_END_TAG_NAME,
    SELF_CLOSING_TAG_DELIMITER,
    IMPLICIT_END_TAG,
    RAW_TEXT,
    RCDATA_TEXT,
    PLAINTEXT_TEXT,
    COMMENT,
    // Django externals
    DJANGO_COMMENT_CONTENT,
    VERBATIM_START,
    VERBATIM_BLOCK_CONTENT,
    VALIDATE_GENERIC_BLOCK,
    VALIDATE_GENERIC_SIMPLE,
    FILTER_COLON,
};

typedef enum {
    HTML_COMMENT_START,
    HTML_COMMENT_START_DASH,
    HTML_COMMENT,
    HTML_COMMENT_LT,
    HTML_COMMENT_LT_BANG,
    HTML_COMMENT_LT_BANG_DASH,
    HTML_COMMENT_LT_BANG_DASH_DASH,
    HTML_COMMENT_END_DASH,
    HTML_COMMENT_END,
    HTML_COMMENT_END_BANG,
} HtmlCommentState;

typedef struct {
    Array(Tag) tags;
    // Verbatim suffix storage
    char *verbatim_suffix;
    uint32_t verbatim_length;
    uint32_t verbatim_capacity;
} Scanner;

static inline void advance(TSLexer *lexer) { lexer->advance(lexer, false); }

static inline void skip(TSLexer *lexer) { lexer->advance(lexer, true); }
static void pop_tag(Scanner *scanner);

// Verbatim suffix helpers
static inline bool is_horizontal_space(int32_t c) {
    return c == ' ' || c == '\t' || c == '\r';
}

static inline void skip_horizontal_space(TSLexer *lexer) {
    while (is_horizontal_space(lexer->lookahead)) advance(lexer);
}

static bool ensure_verbatim_capacity(Scanner *scanner, uint32_t size) {
    if (scanner->verbatim_capacity >= size) return true;
    uint32_t new_cap = scanner->verbatim_capacity ? scanner->verbatim_capacity : 64;
    while (new_cap < size) new_cap *= 2;
    char *new_buf = (char *)realloc(scanner->verbatim_suffix, new_cap);
    if (!new_buf) return false;
    scanner->verbatim_suffix = new_buf;
    scanner->verbatim_capacity = new_cap;
    return true;
}

static void clear_verbatim_suffix(Scanner *scanner) {
    scanner->verbatim_length = 0;
}

// Scan verbatim start: captures suffix after "verbatim" keyword until %}
// Strict Django DTL - no whitespace trim markers supported
static bool scan_verbatim_start(Scanner *scanner, TSLexer *lexer) {
    lexer->mark_end(lexer);
    uint32_t length = 0;
    uint32_t last_non_space = 0;

    for (;;) {
        if (lexer->lookahead == 0 || lexer->lookahead == '\n') return false;

        if (lexer->lookahead == '%') {
            advance(lexer);
            if (lexer->lookahead == '}') {
                // Trim trailing horizontal whitespace from suffix
                length = last_non_space;
                scanner->verbatim_length = length;
                advance(lexer); // consume '}'
                lexer->mark_end(lexer);
                lexer->result_symbol = VERBATIM_START;
                return true;
            }
            // Not a tag end, treat '%' as content
            if (!ensure_verbatim_capacity(scanner, length + 1)) return false;
            scanner->verbatim_suffix[length++] = '%';
            last_non_space = length;
            continue;
        }

        if (!ensure_verbatim_capacity(scanner, length + 1)) return false;
        scanner->verbatim_suffix[length] = (char)lexer->lookahead;
        if (!is_horizontal_space(lexer->lookahead)) {
            last_non_space = length + 1;
        }
        length++;
        advance(lexer);
    }
}

// Scan verbatim content until {% endverbatim<suffix> %}
// This scanner consumes everything INCLUDING the {% endverbatim<suffix> %} tag
// because the suffix is dynamic and matched at runtime by the scanner.
// Strict Django DTL - no whitespace trim markers supported
static bool scan_verbatim_content(Scanner *scanner, TSLexer *lexer) {
    for (;;) {
        if (lexer->lookahead == 0) return false;

        lexer->mark_end(lexer);

        if (lexer->lookahead == '{') {
            advance(lexer);
            if (lexer->lookahead == '%') {
                advance(lexer);
                skip_horizontal_space(lexer);

                // Check for "endverbatim"
                const char *kw = "endverbatim";
                const char *p = kw;
                while (*p && lexer->lookahead == *p) {
                    advance(lexer);
                    p++;
                }
                if (*p == '\0') {
                    // Now match the suffix
                    uint32_t i = 0;
                    while (i < scanner->verbatim_length && lexer->lookahead == scanner->verbatim_suffix[i]) {
                        advance(lexer);
                        i++;
                    }
                    if (i == scanner->verbatim_length) {
                        skip_horizontal_space(lexer);
                        if (lexer->lookahead == '%') {
                            advance(lexer);
                            if (lexer->lookahead == '}') {
                                advance(lexer);
                                lexer->mark_end(lexer);
                                lexer->result_symbol = VERBATIM_BLOCK_CONTENT;
                                clear_verbatim_suffix(scanner);
                                return true;
                            }
                        }
                    }
                }
            }
        }

        advance(lexer);
    }
}

// List of built-in Django tag names that have their own grammar rules
// These should NOT be matched by the generic block scanner
static const char *BUILTIN_DJANGO_TAGS[] = {
    "if", "elif", "else", "endif",
    "for", "empty", "endfor",
    "with", "endwith",
    "block", "endblock",
    "extends",
    "include",
    "load",
    "url",
    "csrf_token",
    "autoescape", "endautoescape",
    "filter", "endfilter",
    "spaceless", "endspaceless",
    "verbatim", "endverbatim",
    "cycle",
    "firstof",
    "now",
    "regroup",
    "ifchanged", "endifchanged",
    "widthratio",
    "templatetag",
    "debug",
    "lorem",
    "resetcycle",
    "querystring",
    "partialdef", "endpartialdef",
    "partial",
    "comment", "endcomment",
    NULL
};

static bool is_builtin_django_tag(const char *tag_name) {
    for (const char **p = BUILTIN_DJANGO_TAGS; *p != NULL; p++) {
        if (strcmp(tag_name, *p) == 0) {
            return true;
        }
    }
    return false;
}

// Zero-width validation scanner for generic tags.
// This produces a zero-width token that validates whether a generic tag is valid.
// For blocks, it looks ahead to verify a matching end tag exists.
// The actual tag name is parsed by the grammar's identifier rule after this validates.
static bool scan_validate_generic_tag(TSLexer *lexer, const bool *valid_symbols) {
    // Mark end immediately - this creates a zero-width token
    // Tree-sitter will reset the lexer to this position after we return
    lexer->mark_end(lexer);

    // Check if we're at a valid tag name start (identifier start char)
    if (!iswalpha(lexer->lookahead) && lexer->lookahead != '_') {
        return false;
    }

    // Scan the tag name (lookahead only - doesn't affect token position)
    char tag_name[256];
    int tag_len = 0;

    while ((iswalnum(lexer->lookahead) || lexer->lookahead == '_') && tag_len < 255) {
        tag_name[tag_len++] = (char)lexer->lookahead;
        advance(lexer);
    }
    tag_name[tag_len] = '\0';

    if (tag_len == 0) {
        return false;
    }

    // Check if this is a built-in Django tag - if so, let the grammar handle it
    if (is_builtin_django_tag(tag_name)) {
        return false;
    }

    // Reject tag names starting with "end" - these are closing tags for block structures
    // and should be handled by the grammar's end tag rules, not as generic tags
    if (tag_len >= 3 && tag_name[0] == 'e' && tag_name[1] == 'n' && tag_name[2] == 'd') {
        return false;
    }

    // If block validation is requested, look for matching end tag
    if (valid_symbols[VALIDATE_GENERIC_BLOCK]) {
        // Build the end tag to look for: "end" + tagname
        char end_tag[264];
        strcpy(end_tag, "end");
        strncat(end_tag, tag_name, sizeof(end_tag) - 4);

        // Scan ahead looking for {% end<tagname> %}
        while (lexer->lookahead != 0) {
            if (lexer->lookahead == '{') {
                advance(lexer);
                if (lexer->lookahead == '%') {
                    advance(lexer);
                    // Skip whitespace
                    while (lexer->lookahead == ' ' || lexer->lookahead == '\t' ||
                           lexer->lookahead == '\r' || lexer->lookahead == '\n') {
                        advance(lexer);
                    }
                    // Try to match end tag
                    const char *p = end_tag;
                    while (*p && lexer->lookahead == *p) {
                        advance(lexer);
                        p++;
                    }
                    if (*p == '\0') {
                        // Matched "end<tagname>", check it's followed by whitespace or %}
                        if (lexer->lookahead == ' ' || lexer->lookahead == '\t' ||
                            lexer->lookahead == '\r' || lexer->lookahead == '\n' ||
                            lexer->lookahead == '%') {
                            // Found the end tag - this is a valid block
                            lexer->result_symbol = VALIDATE_GENERIC_BLOCK;
                            return true;
                        }
                    }
                }
            } else {
                advance(lexer);
            }
        }
    }

    // No end tag found (or block validation not requested)
    // If simple tag validation is valid, return that
    if (valid_symbols[VALIDATE_GENERIC_SIMPLE]) {
        lexer->result_symbol = VALIDATE_GENERIC_SIMPLE;
        return true;
    }

    return false;
}

static unsigned serialize(Scanner *scanner, char *buffer) {
    uint16_t tag_count = scanner->tags.size > UINT16_MAX ? UINT16_MAX : scanner->tags.size;
    uint16_t serialized_tag_count = 0;

    // First byte: verbatim suffix length (0-255)
    uint8_t verbatim_len = scanner->verbatim_length > 255 ? 255 : (uint8_t)scanner->verbatim_length;
    buffer[0] = (char)verbatim_len;
    unsigned size = 1;

    // Copy verbatim suffix
    if (verbatim_len > 0) {
        memcpy(&buffer[size], scanner->verbatim_suffix, verbatim_len);
        size += verbatim_len;
    }

    // Then tag count header
    unsigned tag_header_pos = size;
    size += sizeof(tag_count);
    memcpy(&buffer[size], &tag_count, sizeof(tag_count));
    size += sizeof(tag_count);

    for (; serialized_tag_count < tag_count; serialized_tag_count++) {
        Tag tag = scanner->tags.contents[serialized_tag_count];
        if (tag.type == CUSTOM) {
            unsigned name_length = tag.custom_tag_name.size;
            if (name_length > UINT8_MAX) {
                name_length = UINT8_MAX;
            }
            if (size + 2 + name_length >= TREE_SITTER_SERIALIZATION_BUFFER_SIZE) {
                break;
            }
            buffer[size++] = (char)tag.type;
            buffer[size++] = (char)name_length;
            strncpy(&buffer[size], tag.custom_tag_name.contents, name_length);
            size += name_length;
        } else {
            if (size + 1 >= TREE_SITTER_SERIALIZATION_BUFFER_SIZE) {
                break;
            }
            buffer[size++] = (char)tag.type;
        }
    }

    memcpy(&buffer[tag_header_pos], &serialized_tag_count, sizeof(serialized_tag_count));
    return size;
}

static void deserialize(Scanner *scanner, const char *buffer, unsigned length) {
    for (unsigned i = 0; i < scanner->tags.size; i++) {
        tag_free(&scanner->tags.contents[i]);
    }
    array_clear(&scanner->tags);
    clear_verbatim_suffix(scanner);

    if (length > 0) {
        unsigned size = 0;

        // First byte: verbatim suffix length
        uint8_t verbatim_len = (uint8_t)buffer[size++];
        if (verbatim_len > 0 && size + verbatim_len <= length) {
            if (ensure_verbatim_capacity(scanner, verbatim_len)) {
                memcpy(scanner->verbatim_suffix, &buffer[size], verbatim_len);
                scanner->verbatim_length = verbatim_len;
            }
            size += verbatim_len;
        }

        // Then tag count header
        if (size + sizeof(uint16_t) * 2 > length) return;

        uint16_t tag_count = 0;
        uint16_t serialized_tag_count = 0;

        memcpy(&serialized_tag_count, &buffer[size], sizeof(serialized_tag_count));
        size += sizeof(serialized_tag_count);

        memcpy(&tag_count, &buffer[size], sizeof(tag_count));
        size += sizeof(tag_count);

        array_reserve(&scanner->tags, tag_count);
        if (tag_count > 0) {
            unsigned iter = 0;
            for (iter = 0; iter < serialized_tag_count; iter++) {
                Tag tag = tag_new();
                tag.type = (TagType)buffer[size++];
                if (tag.type == CUSTOM) {
                    uint16_t name_length = (uint8_t)buffer[size++];
                    array_reserve(&tag.custom_tag_name, name_length);
                    tag.custom_tag_name.size = name_length;
                    memcpy(tag.custom_tag_name.contents, &buffer[size], name_length);
                    size += name_length;
                }
                array_push(&scanner->tags, tag);
            }
            // add zero tags if we didn't read enough, this is because the
            // buffer had no more room but we held more tags.
            for (; iter < tag_count; iter++) {
                array_push(&scanner->tags, tag_new());
            }
        }
    }
}

static bool in_foreign_content(Scanner *scanner) {
    for (unsigned i = 0; i < scanner->tags.size; i++) {
        TagType type = scanner->tags.contents[i].type;
        if (type == SVG || type == MATH) {
            return true;
        }
    }
    return false;
}

static String scan_tag_name(TSLexer *lexer, bool uppercase) {
    String tag_name = array_new();
    while (iswalnum(lexer->lookahead) || lexer->lookahead == '-' || lexer->lookahead == ':') {
        array_push(&tag_name, uppercase ? towupper(lexer->lookahead) : lexer->lookahead);
        advance(lexer);
    }
    return tag_name;
}

static bool scan_comment(TSLexer *lexer) {
    // Called immediately after "<!"
    if (lexer->lookahead != '-') return false;
    advance(lexer);
    if (lexer->lookahead != '-') return false;
    advance(lexer);

    HtmlCommentState state = HTML_COMMENT_START;

    for (;;) {
        int32_t c = lexer->lookahead;

        if (c == 0) {
            lexer->result_symbol = COMMENT;
            lexer->mark_end(lexer);
            return true;
        }

        switch (state) {
            case HTML_COMMENT_START:
                if (c == '-') {
                    state = HTML_COMMENT_START_DASH;
                    advance(lexer);
                } else if (c == '>') {
                    advance(lexer);
                    lexer->result_symbol = COMMENT;
                    lexer->mark_end(lexer);
                    return true;
                } else {
                    state = HTML_COMMENT;
                    advance(lexer);
                }
                break;

            case HTML_COMMENT_START_DASH:
                if (c == '-') {
                    state = HTML_COMMENT_END;
                    advance(lexer);
                } else if (c == '>') {
                    advance(lexer);
                    lexer->result_symbol = COMMENT;
                    lexer->mark_end(lexer);
                    return true;
                } else {
                    state = HTML_COMMENT;
                    advance(lexer);
                }
                break;

            case HTML_COMMENT:
                if (c == '<') {
                    state = HTML_COMMENT_LT;
                    advance(lexer);
                } else if (c == '-') {
                    state = HTML_COMMENT_END_DASH;
                    advance(lexer);
                } else {
                    advance(lexer);
                }
                break;

            case HTML_COMMENT_LT:
                if (c == '!') {
                    state = HTML_COMMENT_LT_BANG;
                    advance(lexer);
                } else if (c == '<') {
                    state = HTML_COMMENT;
                    advance(lexer);
                } else {
                    state = HTML_COMMENT;
                }
                break;

            case HTML_COMMENT_LT_BANG:
                if (c == '-') {
                    state = HTML_COMMENT_LT_BANG_DASH;
                    advance(lexer);
                } else {
                    state = HTML_COMMENT;
                }
                break;

            case HTML_COMMENT_LT_BANG_DASH:
                if (c == '-') {
                    state = HTML_COMMENT_LT_BANG_DASH_DASH;
                    advance(lexer);
                } else {
                    state = HTML_COMMENT_END_DASH;
                }
                break;

            case HTML_COMMENT_LT_BANG_DASH_DASH:
                state = HTML_COMMENT_END;
                break;

            case HTML_COMMENT_END_DASH:
                if (c == '-') {
                    state = HTML_COMMENT_END;
                    advance(lexer);
                } else {
                    state = HTML_COMMENT;
                    advance(lexer);
                }
                break;

            case HTML_COMMENT_END:
                if (c == '>') {
                    advance(lexer);
                    lexer->result_symbol = COMMENT;
                    lexer->mark_end(lexer);
                    return true;
                } else if (c == '!') {
                    state = HTML_COMMENT_END_BANG;
                    advance(lexer);
                } else if (c == '-') {
                    advance(lexer);
                } else {
                    state = HTML_COMMENT;
                    advance(lexer);
                }
                break;

            case HTML_COMMENT_END_BANG:
                if (c == '-') {
                    state = HTML_COMMENT_END_DASH;
                    advance(lexer);
                } else if (c == '>') {
                    advance(lexer);
                    lexer->result_symbol = COMMENT;
                    lexer->mark_end(lexer);
                    return true;
                } else {
                    state = HTML_COMMENT;
                    advance(lexer);
                }
                break;
        }
    }
}

// Modified to break on Django delimiters
static bool scan_raw_text(Scanner *scanner, TSLexer *lexer) {
    if (scanner->tags.size == 0) {
        return false;
    }

    Tag *tag = array_back(&scanner->tags);
    if (tag->type != SCRIPT && tag->type != STYLE) {
        return false;
    }

    lexer->mark_end(lexer);

    const char *end_delimiter = array_back(&scanner->tags)->type == SCRIPT ? "</SCRIPT" : "</STYLE";

    unsigned delimiter_index = 0;
    bool has_content = false;

    while (lexer->lookahead) {
        // Check for HTML end tag
        if (towupper(lexer->lookahead) == end_delimiter[delimiter_index]) {
            delimiter_index++;
            if (delimiter_index == strlen(end_delimiter)) {
                break;
            }
            advance(lexer);
        }
        // Check for Django delimiters: {{ or {% or {#
        else if (lexer->lookahead == '{') {
            lexer->mark_end(lexer);
            advance(lexer);
            if (lexer->lookahead == '{' || lexer->lookahead == '%' || lexer->lookahead == '#') {
                // Stop here, let grammar handle Django
                break;
            }
            // Single brace, continue as content
            delimiter_index = 0;
            has_content = true;
            advance(lexer);
            lexer->mark_end(lexer);
        }
        else {
            delimiter_index = 0;
            advance(lexer);
            has_content = true;
            lexer->mark_end(lexer);
        }
    }

    if (has_content) {
        lexer->result_symbol = RAW_TEXT;
        return true;
    }
    return false;
}

// Modified to break on Django delimiters
static bool scan_rcdata_text(Scanner *scanner, TSLexer *lexer) {
    if (scanner->tags.size == 0) {
        return false;
    }

    Tag *tag = array_back(&scanner->tags);
    const char *end_delimiter = NULL;
    switch (tag->type) {
        case TITLE:
            end_delimiter = "</TITLE";
            break;
        case TEXTAREA:
            end_delimiter = "</TEXTAREA";
            break;
        default:
            return false;
    }

    lexer->mark_end(lexer);
    unsigned delimiter_index = 0;
    bool has_content = false;

    while (lexer->lookahead) {
        // Check for HTML end tag
        if (towupper(lexer->lookahead) == end_delimiter[delimiter_index]) {
            delimiter_index++;
            if (delimiter_index == strlen(end_delimiter)) {
                break;
            }
            advance(lexer);
        }
        // Check for Django delimiters: {{ or {% or {#
        else if (lexer->lookahead == '{') {
            lexer->mark_end(lexer);
            advance(lexer);
            if (lexer->lookahead == '{' || lexer->lookahead == '%' || lexer->lookahead == '#') {
                // Stop here, let grammar handle Django
                break;
            }
            // Single brace, continue as content
            delimiter_index = 0;
            has_content = true;
            advance(lexer);
            lexer->mark_end(lexer);
        }
        else {
            delimiter_index = 0;
            advance(lexer);
            has_content = true;
            lexer->mark_end(lexer);
        }
    }

    if (has_content) {
        lexer->result_symbol = RCDATA_TEXT;
        return true;
    }
    return false;
}

static bool scan_plaintext_text(Scanner *scanner, TSLexer *lexer) {
    if (scanner->tags.size == 0 || array_back(&scanner->tags)->type != PLAINTEXT) {
        return false;
    }

    lexer->mark_end(lexer);
    while (lexer->lookahead) {
        advance(lexer);
        lexer->mark_end(lexer);
    }

    pop_tag(scanner);
    lexer->result_symbol = PLAINTEXT_TEXT;
    return true;
}

// Scan Django block comment content until {% endcomment %}
// The content excludes the {% endcomment %} tag - we stop right before it
// so the grammar can match the closing tag explicitly
// Strict Django DTL - no whitespace trim markers supported
static bool scan_django_comment_content(TSLexer *lexer) {
    lexer->mark_end(lexer);

    for (;;) {
        if (lexer->lookahead == 0) {
            return false;
        }

        if (lexer->lookahead == '{') {
            // Mark end BEFORE the '{' - this will be the end of content
            // if we find {% endcomment %}
            lexer->mark_end(lexer);
            advance(lexer);
            if (lexer->lookahead == '%') {
                advance(lexer);
                // Skip whitespace
                while (lexer->lookahead == ' ' || lexer->lookahead == '\t' ||
                       lexer->lookahead == '\r' || lexer->lookahead == '\n') {
                    advance(lexer);
                }
                // Check for "endcomment"
                const char *keyword = "endcomment";
                const char *p = keyword;
                while (*p && lexer->lookahead == *p) {
                    advance(lexer);
                    p++;
                }
                if (*p == '\0') {
                    // Peek ahead to verify this is actually {% endcomment %}
                    // Skip whitespace
                    while (lexer->lookahead == ' ' || lexer->lookahead == '\t' ||
                           lexer->lookahead == '\r' || lexer->lookahead == '\n') {
                        advance(lexer);
                    }
                    if (lexer->lookahead == '%') {
                        advance(lexer);
                        if (lexer->lookahead == '}') {
                            // Found {% endcomment %} - don't consume it!
                            // The mark_end before '{' already set the correct end position
                            // Return the content without the closing tag
                            lexer->result_symbol = DJANGO_COMMENT_CONTENT;
                            return true;
                        }
                    }
                }
            }
        } else {
            advance(lexer);
        }
    }
}

static void pop_tag(Scanner *scanner) {
    Tag popped_tag = array_pop(&scanner->tags);
    tag_free(&popped_tag);
}

static bool scan_implicit_end_tag(Scanner *scanner, TSLexer *lexer) {
    bool foreign = in_foreign_content(scanner);
    Tag *parent = scanner->tags.size == 0 ? NULL : array_back(&scanner->tags);

    if (!foreign && parent && lexer->eof(lexer)) {
        pop_tag(scanner);
        lexer->result_symbol = IMPLICIT_END_TAG;
        return true;
    }

    bool is_closing_tag = false;
    if (lexer->lookahead == '/') {
        is_closing_tag = true;
        advance(lexer);
    } else {
        if (parent && tag_is_void(parent)) {
            pop_tag(scanner);
            lexer->result_symbol = IMPLICIT_END_TAG;
            return true;
        }
    }

    bool uppercase = !foreign || (parent && parent->type != CUSTOM);
    String tag_name = scan_tag_name(lexer, uppercase);
    if (tag_name.size == 0 && !lexer->eof(lexer)) {
        array_delete(&tag_name);
        return false;
    }

    Tag next_tag = tag_for_name(tag_name);

    if (is_closing_tag) {
        // The tag correctly closes the topmost element on the stack
        if (scanner->tags.size > 0 && tag_eq(array_back(&scanner->tags), &next_tag)) {
            tag_free(&next_tag);
            return false;
        }

        // Otherwise, dig deeper and queue implicit end tags (to be nice in
        // the case of malformed HTML)
        for (unsigned i = scanner->tags.size; i > 0; i--) {
            if (tag_eq(&scanner->tags.contents[i - 1], &next_tag)) {
                pop_tag(scanner);
                lexer->result_symbol = IMPLICIT_END_TAG;
                tag_free(&next_tag);
                return true;
            }
        }
    } else if (
        parent &&
        !foreign &&
        (
            !tag_can_contain(parent, &next_tag) ||
            ((parent->type == HTML || parent->type == HEAD || parent->type == BODY) && lexer->eof(lexer))
        )
    ) {
        pop_tag(scanner);
        lexer->result_symbol = IMPLICIT_END_TAG;
        tag_free(&next_tag);
        return true;
    }

    tag_free(&next_tag);
    return false;
}

static bool scan_start_tag_name(Scanner *scanner, TSLexer *lexer) {
    bool foreign_context = in_foreign_content(scanner);
    String tag_name = scan_tag_name(lexer, !foreign_context);
    if (tag_name.size == 0) {
        array_delete(&tag_name);
        return false;
    }

    if (foreign_context) {
        Tag tag = tag_new();
        tag.type = CUSTOM;
        tag.custom_tag_name = tag_name;
        array_push(&scanner->tags, tag);
        lexer->result_symbol = FOREIGN_START_TAG_NAME;
        return true;
    }

    Tag tag = tag_for_name(tag_name);

    if (tag_is_void(&tag)) {
        lexer->result_symbol = VOID_START_TAG_NAME;
        tag_free(&tag);
        return true;
    }

    array_push(&scanner->tags, tag);
    switch (tag.type) {
        case SCRIPT:
            lexer->result_symbol = SCRIPT_START_TAG_NAME;
            break;
        case STYLE:
            lexer->result_symbol = STYLE_START_TAG_NAME;
            break;
        case TITLE:
            lexer->result_symbol = TITLE_START_TAG_NAME;
            break;
        case TEXTAREA:
            lexer->result_symbol = TEXTAREA_START_TAG_NAME;
            break;
        case PLAINTEXT:
            lexer->result_symbol = PLAINTEXT_START_TAG_NAME;
            break;
        case SVG:
        case MATH:
            lexer->result_symbol = FOREIGN_START_TAG_NAME;
            break;
        default:
            lexer->result_symbol = HTML_START_TAG_NAME;
            break;
    }
    return true;
}

static bool scan_end_tag_name(Scanner *scanner, TSLexer *lexer) {
    bool foreign_context = in_foreign_content(scanner);
    Tag *top = scanner->tags.size > 0 ? array_back(&scanner->tags) : NULL;
    bool uppercase = !foreign_context || (top && (top->type == SVG || top->type == MATH));

    String tag_name = scan_tag_name(lexer, uppercase);

    if (tag_name.size == 0) {
        array_delete(&tag_name);
        return false;
    }

    Tag tag;
    if (foreign_context && !uppercase) {
        tag = tag_new();
        tag.type = CUSTOM;
        tag.custom_tag_name = tag_name;
    } else {
        tag = tag_for_name(tag_name);
    }

    if (scanner->tags.size > 0 && tag_eq(array_back(&scanner->tags), &tag)) {
        pop_tag(scanner);
        lexer->result_symbol = END_TAG_NAME;
    } else {
        // The end tag doesn't match the top of stack.
        // Check if it matches something deeper in the stack.
        // If found, return END_TAG_NAME but DON'T pop - this allows the
        // grammar to parse it as unpaired_end_tag while preserving the
        // stack for Django conditional branches (where both if/else
        // branches may have unbalanced tags).
        bool found_match = false;
        for (unsigned i = scanner->tags.size; i > 0; i--) {
            if (tag_eq(&scanner->tags.contents[i - 1], &tag)) {
                lexer->result_symbol = END_TAG_NAME;
                found_match = true;
                break;
            }
        }
        if (!found_match) {
            lexer->result_symbol = ERRONEOUS_END_TAG_NAME;
        }
    }

    tag_free(&tag);
    return true;
}

static bool scan_self_closing_tag_delimiter(Scanner *scanner, TSLexer *lexer) {
    advance(lexer);
    if (lexer->lookahead == '>') {
        advance(lexer);
        if (in_foreign_content(scanner) && scanner->tags.size > 0) {
            pop_tag(scanner);
        }
        lexer->result_symbol = SELF_CLOSING_TAG_DELIMITER;
        return true;
    }
    return false;
}

static bool scan(Scanner *scanner, TSLexer *lexer, const bool *valid_symbols) {
    // Handle Django block comment content first
    if (valid_symbols[DJANGO_COMMENT_CONTENT]) {
        return scan_django_comment_content(lexer);
    }

    // Handle verbatim start (after "verbatim" keyword)
    if (valid_symbols[VERBATIM_START]) {
        return scan_verbatim_start(scanner, lexer);
    }

    // Handle verbatim block content
    if (valid_symbols[VERBATIM_BLOCK_CONTENT]) {
        return scan_verbatim_content(scanner, lexer);
    }

    // Handle generic tag validation (zero-width lookahead tokens)
    // These validate whether a generic tag/block is appropriate before the grammar parses the tag name
    if (valid_symbols[VALIDATE_GENERIC_BLOCK] || valid_symbols[VALIDATE_GENERIC_SIMPLE]) {
        return scan_validate_generic_tag(lexer, valid_symbols);
    }

    // Handle filter colon - only match ':' if immediately followed by valid argument char
    // Django does not allow whitespace after the colon in filter arguments
    if (valid_symbols[FILTER_COLON] && lexer->lookahead == ':') {
        lexer->mark_end(lexer);
        advance(lexer);
        // Check if next char is valid start of filter argument:
        // - Quote chars for strings: " '
        // - Digits for numbers: 0-9
        // - Sign for numbers: + -
        // - Decimal point for numbers: .
        // - Letters/underscore for identifiers and i18n strings: a-z A-Z _
        int32_t next = lexer->lookahead;
        if (next == '"' || next == '\'' ||
            (next >= '0' && next <= '9') ||
            next == '+' || next == '-' || next == '.' ||
            (next >= 'a' && next <= 'z') ||
            (next >= 'A' && next <= 'Z') ||
            next == '_') {
            lexer->mark_end(lexer);
            lexer->result_symbol = FILTER_COLON;
            return true;
        }
        // Not followed by valid argument char - don't match
        return false;
    }

    bool valid_start_tag =
        valid_symbols[HTML_START_TAG_NAME] ||
        valid_symbols[VOID_START_TAG_NAME] ||
        valid_symbols[FOREIGN_START_TAG_NAME] ||
        valid_symbols[SCRIPT_START_TAG_NAME] ||
        valid_symbols[STYLE_START_TAG_NAME] ||
        valid_symbols[TITLE_START_TAG_NAME] ||
        valid_symbols[TEXTAREA_START_TAG_NAME] ||
        valid_symbols[PLAINTEXT_START_TAG_NAME];

    bool valid_end_tag = valid_symbols[END_TAG_NAME] || valid_symbols[ERRONEOUS_END_TAG_NAME];

    if (valid_symbols[RAW_TEXT] && !valid_end_tag && !valid_start_tag) {
        return scan_raw_text(scanner, lexer);
    }

    if (valid_symbols[RCDATA_TEXT] && !valid_end_tag && !valid_start_tag) {
        return scan_rcdata_text(scanner, lexer);
    }

    if (valid_symbols[PLAINTEXT_TEXT]) {
        return scan_plaintext_text(scanner, lexer);
    }

    while (iswspace(lexer->lookahead)) {
        skip(lexer);
    }

    switch (lexer->lookahead) {
        case '<':
            lexer->mark_end(lexer);
            advance(lexer);

            if (lexer->lookahead == '!') {
                advance(lexer);
                return scan_comment(lexer);
            }

            if (valid_symbols[IMPLICIT_END_TAG]) {
                return scan_implicit_end_tag(scanner, lexer);
            }
            break;

        case '\0':
            if (valid_symbols[IMPLICIT_END_TAG]) {
                return scan_implicit_end_tag(scanner, lexer);
            }
            break;

        case '/':
            if (valid_symbols[SELF_CLOSING_TAG_DELIMITER]) {
                return scan_self_closing_tag_delimiter(scanner, lexer);
            }
            break;

        default:
            if ((valid_start_tag || valid_end_tag) && !valid_symbols[RAW_TEXT]) {
                return valid_end_tag ? scan_end_tag_name(scanner, lexer)
                                     : scan_start_tag_name(scanner, lexer);
            }
    }

    return false;
}

void *tree_sitter_htmldjango_external_scanner_create() {
    Scanner *scanner = (Scanner *)ts_calloc(1, sizeof(Scanner));
    return scanner;
}

bool tree_sitter_htmldjango_external_scanner_scan(void *payload, TSLexer *lexer, const bool *valid_symbols) {
    Scanner *scanner = (Scanner *)payload;
    return scan(scanner, lexer, valid_symbols);
}

unsigned tree_sitter_htmldjango_external_scanner_serialize(void *payload, char *buffer) {
    Scanner *scanner = (Scanner *)payload;
    return serialize(scanner, buffer);
}

void tree_sitter_htmldjango_external_scanner_deserialize(void *payload, const char *buffer, unsigned length) {
    Scanner *scanner = (Scanner *)payload;
    deserialize(scanner, buffer, length);
}

void tree_sitter_htmldjango_external_scanner_destroy(void *payload) {
    Scanner *scanner = (Scanner *)payload;
    for (unsigned i = 0; i < scanner->tags.size; i++) {
        tag_free(&scanner->tags.contents[i]);
    }
    array_delete(&scanner->tags);
    free(scanner->verbatim_suffix);
    ts_free(scanner);
}
