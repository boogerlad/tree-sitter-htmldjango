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
} Scanner;

static inline void advance(TSLexer *lexer) { lexer->advance(lexer, false); }

static inline void skip(TSLexer *lexer) { lexer->advance(lexer, true); }
static void pop_tag(Scanner *scanner);

static unsigned serialize(Scanner *scanner, char *buffer) {
    uint16_t tag_count = scanner->tags.size > UINT16_MAX ? UINT16_MAX : scanner->tags.size;
    uint16_t serialized_tag_count = 0;

    unsigned size = sizeof(tag_count);
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

    memcpy(&buffer[0], &serialized_tag_count, sizeof(serialized_tag_count));
    return size;
}

static void deserialize(Scanner *scanner, const char *buffer, unsigned length) {
    for (unsigned i = 0; i < scanner->tags.size; i++) {
        tag_free(&scanner->tags.contents[i]);
    }
    array_clear(&scanner->tags);

    if (length > 0) {
        unsigned size = 0;
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
    while (lexer->lookahead) {
        if (towupper(lexer->lookahead) == end_delimiter[delimiter_index]) {
            delimiter_index++;
            if (delimiter_index == strlen(end_delimiter)) {
                break;
            }
            advance(lexer);
        } else {
            delimiter_index = 0;
            advance(lexer);
            lexer->mark_end(lexer);
        }
    }

    lexer->result_symbol = RAW_TEXT;
    return true;
}

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
    while (lexer->lookahead) {
        if (towupper(lexer->lookahead) == end_delimiter[delimiter_index]) {
            delimiter_index++;
            if (delimiter_index == strlen(end_delimiter)) {
                break;
            }
            advance(lexer);
        } else {
            delimiter_index = 0;
            advance(lexer);
            lexer->mark_end(lexer);
        }
    }

    lexer->result_symbol = RCDATA_TEXT;
    return true;
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
        lexer->result_symbol = ERRONEOUS_END_TAG_NAME;
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

void *tree_sitter_html_external_scanner_create() {
    Scanner *scanner = (Scanner *)ts_calloc(1, sizeof(Scanner));
    return scanner;
}

bool tree_sitter_html_external_scanner_scan(void *payload, TSLexer *lexer, const bool *valid_symbols) {
    Scanner *scanner = (Scanner *)payload;
    return scan(scanner, lexer, valid_symbols);
}

unsigned tree_sitter_html_external_scanner_serialize(void *payload, char *buffer) {
    Scanner *scanner = (Scanner *)payload;
    return serialize(scanner, buffer);
}

void tree_sitter_html_external_scanner_deserialize(void *payload, const char *buffer, unsigned length) {
    Scanner *scanner = (Scanner *)payload;
    deserialize(scanner, buffer, length);
}

void tree_sitter_html_external_scanner_destroy(void *payload) {
    Scanner *scanner = (Scanner *)payload;
    for (unsigned i = 0; i < scanner->tags.size; i++) {
        tag_free(&scanner->tags.contents[i]);
    }
    array_delete(&scanner->tags);
    ts_free(scanner);
}
