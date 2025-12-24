#include <tree_sitter/parser.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wctype.h>
#include "tag.h"

enum TokenType {
  TEXT_FRAGMENT,
  INTERPOLATION_TEXT,
  START_TAG_NAME,
  TEMPLATE_START_TAG_NAME,
  SCRIPT_START_TAG_NAME,
  STYLE_START_TAG_NAME,
  END_TAG_NAME,
  ERRONEOUS_END_TAG_NAME,
  SELF_CLOSING_TAG_DELIMITER,
  IMPLICIT_END_TAG,
  RAW_TEXT,
  COMMENT
};

typedef struct {
  Tag *data;
  size_t size;
  size_t capacity;
} TagStack;

typedef struct Scanner {
  TagStack tags;
} Scanner;

static void tag_stack_init(TagStack *stack) {
  if (!stack) return;
  stack->data = NULL;
  stack->size = 0;
  stack->capacity = 0;
}

static void tag_stack_free(TagStack *stack) {
  if (!stack) return;
  free(stack->data);
  stack->data = NULL;
  stack->size = 0;
  stack->capacity = 0;
}

static bool tag_stack_reserve(TagStack *stack, size_t capacity) {
  if (!stack) return false;
  if (capacity <= stack->capacity) return true;

  size_t new_capacity = stack->capacity ? stack->capacity : 8;
  while (new_capacity < capacity) {
    new_capacity *= 2;
  }

  Tag *data = (Tag *)realloc(stack->data, new_capacity * sizeof(Tag));
  if (!data) {
    return false;
  }

  stack->data = data;
  stack->capacity = new_capacity;
  return true;
}

static bool tag_stack_resize(TagStack *stack, size_t size) {
  if (!tag_stack_reserve(stack, size)) {
    return false;
  }

  if (size > stack->size) {
    for (size_t i = stack->size; i < size; i++) {
      tag_reset(&stack->data[i]);
    }
  }

  stack->size = size;
  return true;
}

static bool tag_stack_push(TagStack *stack, const Tag *tag) {
  if (!stack || !tag) return false;
  if (!tag_stack_reserve(stack, stack->size + 1)) {
    return false;
  }
  stack->data[stack->size] = *tag;
  stack->size += 1;
  return true;
}

static void tag_stack_pop(TagStack *stack) {
  if (!stack || stack->size == 0) return;
  stack->size -= 1;
}

static Tag *tag_stack_top(TagStack *stack) {
  if (!stack || stack->size == 0) return NULL;
  return &stack->data[stack->size - 1];
}

static bool tag_stack_empty(const TagStack *stack) {
  return !stack || stack->size == 0;
}

static bool tag_stack_contains(const TagStack *stack, const Tag *tag) {
  if (!stack || !tag) return false;
  for (size_t i = 0; i < stack->size; i++) {
    if (tag_equals(&stack->data[i], tag)) {
      return true;
    }
  }
  return false;
}

static Scanner *scanner_new(void) {
  Scanner *scanner = (Scanner *)calloc(1, sizeof(Scanner));
  if (!scanner) {
    return NULL;
  }
  tag_stack_init(&scanner->tags);
  return scanner;
}

static void scanner_free(Scanner *scanner) {
  if (!scanner) return;
  tag_stack_free(&scanner->tags);
  free(scanner);
}

static unsigned scanner_serialize(Scanner *scanner, char *buffer) {
  if (!scanner || !buffer) return 0;

  size_t tag_count_value = scanner->tags.size;
  if (tag_count_value > UINT16_MAX) {
    tag_count_value = UINT16_MAX;
  }

  uint16_t tag_count = (uint16_t)tag_count_value;
  uint16_t serialized_tag_count = 0;

  unsigned offset = sizeof(tag_count);
  memcpy(&buffer[offset], &tag_count, sizeof(tag_count));
  offset += sizeof(tag_count);

  for (; serialized_tag_count < tag_count; serialized_tag_count++) {
    Tag *tag = &scanner->tags.data[serialized_tag_count];
    if (tag->type == CUSTOM) {
      unsigned name_length = tag->custom_tag_name_length;
      if (name_length > UINT8_MAX) {
        name_length = UINT8_MAX;
      }
      if (offset + 2 + name_length >= TREE_SITTER_SERIALIZATION_BUFFER_SIZE) {
        break;
      }
      buffer[offset++] = (char)tag->type;
      buffer[offset++] = (char)name_length;
      memcpy(&buffer[offset], tag->custom_tag_name, name_length);
      offset += name_length;
    } else {
      if (offset + 1 >= TREE_SITTER_SERIALIZATION_BUFFER_SIZE) {
        break;
      }
      buffer[offset++] = (char)tag->type;
    }
  }

  memcpy(&buffer[0], &serialized_tag_count, sizeof(serialized_tag_count));
  return offset;
}

static void scanner_deserialize(Scanner *scanner, const char *buffer, unsigned length) {
  if (!scanner) return;
  scanner->tags.size = 0;
  if (!buffer || length == 0) {
    return;
  }

  unsigned offset = 0;
  uint16_t serialized_tag_count = 0;
  uint16_t tag_count = 0;

  if (length < sizeof(serialized_tag_count) + sizeof(tag_count)) {
    return;
  }

  memcpy(&serialized_tag_count, &buffer[offset], sizeof(serialized_tag_count));
  offset += sizeof(serialized_tag_count);

  memcpy(&tag_count, &buffer[offset], sizeof(tag_count));
  offset += sizeof(tag_count);

  if (!tag_stack_resize(&scanner->tags, tag_count)) {
    scanner->tags.size = 0;
    return;
  }

  for (unsigned j = 0; j < serialized_tag_count && j < scanner->tags.size; j++) {
    if (offset >= length) {
      break;
    }
    Tag *tag = &scanner->tags.data[j];
    tag->type = (TagType)buffer[offset++];
    if (tag->type == CUSTOM) {
      if (offset >= length) {
        tag_reset(tag);
        break;
      }
      uint16_t name_length = (uint8_t)buffer[offset++];
      if (name_length > TREE_SITTER_HTML_MAX_CUSTOM_TAG_NAME_LENGTH) {
        name_length = TREE_SITTER_HTML_MAX_CUSTOM_TAG_NAME_LENGTH;
      }
      if ((unsigned)name_length > length - offset) {
        name_length = (uint16_t)(length - offset);
      }
      memcpy(tag->custom_tag_name, &buffer[offset], name_length);
      tag->custom_tag_name[name_length] = '\0';
      tag->custom_tag_name_length = (uint8_t)name_length;
      offset += name_length;
    } else {
      tag->custom_tag_name_length = 0;
      tag->custom_tag_name[0] = '\0';
    }
  }

  for (size_t j = serialized_tag_count; j < scanner->tags.size; j++) {
    tag_reset(&scanner->tags.data[j]);
  }
}

static bool scan_tag_name(TSLexer *lexer, char *buffer, uint8_t *length) {
  if (buffer) {
    buffer[0] = '\0';
  }
  if (length) {
    *length = 0;
  }

  size_t stored = 0;
  bool has_char = false;

  while (iswalnum(lexer->lookahead) || lexer->lookahead == '-' || lexer->lookahead == ':') {
    has_char = true;
    if (stored < TREE_SITTER_HTML_MAX_CUSTOM_TAG_NAME_LENGTH) {
      buffer[stored++] = (char)towupper(lexer->lookahead);
    }
    lexer->advance(lexer, false);
  }

  if (!has_char) {
    return false;
  }

  size_t capped_length = stored;
  if (capped_length > TREE_SITTER_HTML_MAX_CUSTOM_TAG_NAME_LENGTH) {
    capped_length = TREE_SITTER_HTML_MAX_CUSTOM_TAG_NAME_LENGTH;
  }

  buffer[capped_length] = '\0';
  if (length) {
    *length = (uint8_t)capped_length;
  }
  return true;
}

static bool scan_comment(TSLexer *lexer) {
  if (lexer->lookahead != '-') return false;
  lexer->advance(lexer, false);
  if (lexer->lookahead != '-') return false;
  lexer->advance(lexer, false);

  unsigned dashes = 0;
  while (lexer->lookahead) {
    switch (lexer->lookahead) {
      case '-':
        dashes++;
        break;
      case '>':
        if (dashes >= 2) {
          lexer->result_symbol = COMMENT;
          lexer->advance(lexer, false);
          lexer->mark_end(lexer);
          return true;
        }
        /* fallthrough */
      default:
        dashes = 0;
        break;
    }
    lexer->advance(lexer, false);
  }
  return false;
}

static bool scan_raw_text(Scanner *scanner, TSLexer *lexer) {
  if (!scanner || tag_stack_empty(&scanner->tags)) return false;

  lexer->mark_end(lexer);

  Tag *top = tag_stack_top(&scanner->tags);
  const char *end_delimiter = (top->type == SCRIPT) ? "</SCRIPT" : "</STYLE";
  size_t delimiter_length = strlen(end_delimiter);
  size_t delimiter_index = 0;

  while (lexer->lookahead) {
    if ((char)towupper(lexer->lookahead) == end_delimiter[delimiter_index]) {
      delimiter_index++;
      if (delimiter_index == delimiter_length) {
        break;
      }
      lexer->advance(lexer, false);
    } else {
      delimiter_index = 0;
      lexer->advance(lexer, false);
      lexer->mark_end(lexer);
    }
  }

  lexer->result_symbol = RAW_TEXT;
  return true;
}

static bool scan_implicit_end_tag(Scanner *scanner, TSLexer *lexer) {
  Tag *parent = tag_stack_top(&scanner->tags);

  bool is_closing_tag = false;
  if (lexer->lookahead == '/') {
    is_closing_tag = true;
    lexer->advance(lexer, false);
  } else if (parent && tag_is_void(parent)) {
    tag_stack_pop(&scanner->tags);
    lexer->result_symbol = IMPLICIT_END_TAG;
    return true;
  }

  char tag_name_buffer[TREE_SITTER_HTML_MAX_CUSTOM_TAG_NAME_LENGTH + 1];
  if (!scan_tag_name(lexer, tag_name_buffer, NULL)) {
    return false;
  }

  Tag next_tag = tag_for_name(tag_name_buffer);

  if (is_closing_tag) {
    if (!tag_stack_empty(&scanner->tags) && tag_equals(tag_stack_top(&scanner->tags), &next_tag)) {
      return false;
    }

    if (tag_stack_contains(&scanner->tags, &next_tag)) {
      tag_stack_pop(&scanner->tags);
      lexer->result_symbol = IMPLICIT_END_TAG;
      return true;
    }
  } else if (parent && !tag_can_contain(parent, &next_tag)) {
    tag_stack_pop(&scanner->tags);
    lexer->result_symbol = IMPLICIT_END_TAG;
    return true;
  }

  return false;
}

static bool scan_start_tag_name(Scanner *scanner, TSLexer *lexer) {
  char tag_name_buffer[TREE_SITTER_HTML_MAX_CUSTOM_TAG_NAME_LENGTH + 1];
  if (!scan_tag_name(lexer, tag_name_buffer, NULL)) {
    return false;
  }

  Tag tag = tag_for_name(tag_name_buffer);
  if (!tag_stack_push(&scanner->tags, &tag)) {
    return false;
  }

  switch (tag.type) {
    case TEMPLATE:
      lexer->result_symbol = TEMPLATE_START_TAG_NAME;
      break;
    case SCRIPT:
      lexer->result_symbol = SCRIPT_START_TAG_NAME;
      break;
    case STYLE:
      lexer->result_symbol = STYLE_START_TAG_NAME;
      break;
    default:
      lexer->result_symbol = START_TAG_NAME;
      break;
  }

  return true;
}

static bool scan_end_tag_name(Scanner *scanner, TSLexer *lexer) {
  char tag_name_buffer[TREE_SITTER_HTML_MAX_CUSTOM_TAG_NAME_LENGTH + 1];
  if (!scan_tag_name(lexer, tag_name_buffer, NULL)) {
    return false;
  }

  Tag tag = tag_for_name(tag_name_buffer);
  Tag *top = tag_stack_top(&scanner->tags);
  if (top && tag_equals(top, &tag)) {
    tag_stack_pop(&scanner->tags);
    lexer->result_symbol = END_TAG_NAME;
  } else {
    lexer->result_symbol = ERRONEOUS_END_TAG_NAME;
  }

  return true;
}

static bool scan_self_closing_tag_delimiter(Scanner *scanner, TSLexer *lexer) {
  lexer->advance(lexer, false);
  if (lexer->lookahead == '>') {
    lexer->advance(lexer, false);
    if (!tag_stack_empty(&scanner->tags)) {
      tag_stack_pop(&scanner->tags);
      lexer->result_symbol = SELF_CLOSING_TAG_DELIMITER;
    }
    return true;
  }
  return false;
}

static bool scanner_scan(Scanner *scanner, TSLexer *lexer, const bool *valid_symbols) {
  if (!scanner) return false;

  while (iswspace(lexer->lookahead)) {
    lexer->advance(lexer, true);
  }

  if (valid_symbols[RAW_TEXT] && !valid_symbols[START_TAG_NAME] && !valid_symbols[END_TAG_NAME]) {
    return scan_raw_text(scanner, lexer);
  }

  switch (lexer->lookahead) {
    case '<':
      lexer->mark_end(lexer);
      lexer->advance(lexer, false);

      if (lexer->lookahead == '!') {
        lexer->advance(lexer, false);
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
      if ((valid_symbols[START_TAG_NAME] || valid_symbols[END_TAG_NAME]) && !valid_symbols[RAW_TEXT]) {
        if (valid_symbols[START_TAG_NAME]) {
          return scan_start_tag_name(scanner, lexer);
        }
        return scan_end_tag_name(scanner, lexer);
      }
      break;
  }

  return false;
}

#ifdef __cplusplus
extern "C" {
#endif

void *tree_sitter_html_external_scanner_create() {
  return scanner_new();
}

bool tree_sitter_html_external_scanner_scan(void *payload, TSLexer *lexer, const bool *valid_symbols) {
  return scanner_scan((Scanner *)payload, lexer, valid_symbols);
}

unsigned tree_sitter_html_external_scanner_serialize(void *payload, char *buffer) {
  return scanner_serialize((Scanner *)payload, buffer);
}

void tree_sitter_html_external_scanner_deserialize(void *payload, const char *buffer, unsigned length) {
  scanner_deserialize((Scanner *)payload, buffer, length);
}

void tree_sitter_html_external_scanner_destroy(void *payload) {
  scanner_free((Scanner *)payload);
}

#ifdef __cplusplus
}
#endif