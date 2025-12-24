#ifndef TREE_SITTER_HTML_TAG_H_
#define TREE_SITTER_HTML_TAG_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TREE_SITTER_HTML_MAX_CUSTOM_TAG_NAME_LENGTH 255

#define TREE_SITTER_HTML_VOID_TAG_LIST \
  TAG(AREA) \
  TAG(BASE) \
  TAG(BASEFONT) \
  TAG(BGSOUND) \
  TAG(BR) \
  TAG(COL) \
  TAG(COMMAND) \
  TAG(EMBED) \
  TAG(FRAME) \
  TAG(HR) \
  TAG(IMAGE) \
  TAG(IMG) \
  TAG(INPUT) \
  TAG(ISINDEX) \
  TAG(KEYGEN) \
  TAG(LINK) \
  TAG(MENUITEM) \
  TAG(META) \
  TAG(NEXTID) \
  TAG(PARAM) \
  TAG(SOURCE) \
  TAG(TRACK) \
  TAG(WBR)

#define TREE_SITTER_HTML_NON_VOID_TAG_LIST \
  TAG(A) \
  TAG(ABBR) \
  TAG(ADDRESS) \
  TAG(ARTICLE) \
  TAG(ASIDE) \
  TAG(AUDIO) \
  TAG(B) \
  TAG(BDI) \
  TAG(BDO) \
  TAG(BLOCKQUOTE) \
  TAG(BODY) \
  TAG(BUTTON) \
  TAG(CANVAS) \
  TAG(CAPTION) \
  TAG(CITE) \
  TAG(CODE) \
  TAG(COLGROUP) \
  TAG(DATA) \
  TAG(DATALIST) \
  TAG(DD) \
  TAG(DEL) \
  TAG(DETAILS) \
  TAG(DFN) \
  TAG(DIALOG) \
  TAG(DIV) \
  TAG(DL) \
  TAG(DT) \
  TAG(EM) \
  TAG(FIELDSET) \
  TAG(FIGCAPTION) \
  TAG(FIGURE) \
  TAG(FOOTER) \
  TAG(FORM) \
  TAG(H1) \
  TAG(H2) \
  TAG(H3) \
  TAG(H4) \
  TAG(H5) \
  TAG(H6) \
  TAG(HEAD) \
  TAG(HEADER) \
  TAG(HGROUP) \
  TAG(HTML) \
  TAG(I) \
  TAG(IFRAME) \
  TAG(INS) \
  TAG(KBD) \
  TAG(LABEL) \
  TAG(LEGEND) \
  TAG(LI) \
  TAG(MAIN) \
  TAG(MAP) \
  TAG(MARK) \
  TAG(MATH) \
  TAG(MENU) \
  TAG(METER) \
  TAG(NAV) \
  TAG(NOSCRIPT) \
  TAG(OBJECT) \
  TAG(OL) \
  TAG(OPTGROUP) \
  TAG(OPTION) \
  TAG(OUTPUT) \
  TAG(P) \
  TAG(PICTURE) \
  TAG(PRE) \
  TAG(PROGRESS) \
  TAG(Q) \
  TAG(RB) \
  TAG(RP) \
  TAG(RT) \
  TAG(RTC) \
  TAG(RUBY) \
  TAG(S) \
  TAG(SAMP) \
  TAG(SCRIPT) \
  TAG(SECTION) \
  TAG(SELECT) \
  TAG(SLOT) \
  TAG(SMALL) \
  TAG(SPAN) \
  TAG(STRONG) \
  TAG(STYLE) \
  TAG(SUB) \
  TAG(SUMMARY) \
  TAG(SUP) \
  TAG(SVG) \
  TAG(TABLE) \
  TAG(TBODY) \
  TAG(TD) \
  TAG(TEMPLATE) \
  TAG(TEXTAREA) \
  TAG(TFOOT) \
  TAG(TH) \
  TAG(THEAD) \
  TAG(TIME) \
  TAG(TITLE) \
  TAG(TR) \
  TAG(U) \
  TAG(UL) \
  TAG(VAR) \
  TAG(VIDEO)

typedef enum {
#define TAG(name) name,
  TREE_SITTER_HTML_VOID_TAG_LIST
#undef TAG
  END_OF_VOID_TAGS,
#define TAG(name) name,
  TREE_SITTER_HTML_NON_VOID_TAG_LIST
#undef TAG
  CUSTOM,
} TagType;

typedef struct {
  const char *name;
  TagType type;
} TagNameEntry;

typedef struct {
  TagType type;
  uint8_t custom_tag_name_length;
  char custom_tag_name[TREE_SITTER_HTML_MAX_CUSTOM_TAG_NAME_LENGTH + 1];
} Tag;

static inline void tag_reset(Tag *tag) {
  if (!tag) return;
  tag->type = END_OF_VOID_TAGS;
  tag->custom_tag_name_length = 0;
  tag->custom_tag_name[0] = '\0';
}

static const TagNameEntry TAG_NAME_ENTRIES[] = {
#define TAG(name) {#name, name},
  TREE_SITTER_HTML_VOID_TAG_LIST
  TREE_SITTER_HTML_NON_VOID_TAG_LIST
#undef TAG
};

static const size_t TAG_NAME_ENTRY_COUNT = sizeof(TAG_NAME_ENTRIES) / sizeof(TAG_NAME_ENTRIES[0]);

static const TagType TAG_TYPES_NOT_ALLOWED_IN_PARAGRAPHS[] = {
  ADDRESS,
  ARTICLE,
  ASIDE,
  BLOCKQUOTE,
  DETAILS,
  DIV,
  DL,
  FIELDSET,
  FIGCAPTION,
  FIGURE,
  FOOTER,
  FORM,
  H1,
  H2,
  H3,
  H4,
  H5,
  H6,
  HEADER,
  HR,
  MAIN,
  NAV,
  OL,
  P,
  PRE,
  SECTION,
};

static const size_t TAG_TYPES_NOT_ALLOWED_IN_PARAGRAPHS_COUNT = sizeof(TAG_TYPES_NOT_ALLOWED_IN_PARAGRAPHS) / sizeof(TagType);

static inline bool tag_is_void(const Tag *tag) {
  return tag && tag->type < END_OF_VOID_TAGS;
}

static inline bool tag_names_equal(const Tag *left, const Tag *right) {
  if (!left || !right) return false;
  if (left->custom_tag_name_length != right->custom_tag_name_length) return false;
  return memcmp(left->custom_tag_name, right->custom_tag_name, left->custom_tag_name_length) == 0;
}

static inline bool tag_equals(const Tag *left, const Tag *right) {
  if (!left || !right) return false;
  if (left->type != right->type) return false;
  if (left->type != CUSTOM) return true;
  return tag_names_equal(left, right);
}

static inline bool tag_type_allowed_in_paragraph(TagType type) {
  for (size_t i = 0; i < TAG_TYPES_NOT_ALLOWED_IN_PARAGRAPHS_COUNT; i++) {
    if (TAG_TYPES_NOT_ALLOWED_IN_PARAGRAPHS[i] == type) {
      return false;
    }
  }
  return true;
}

static inline bool tag_can_contain(const Tag *parent, const Tag *child) {
  if (!parent || !child) return false;
  TagType parent_type = parent->type;
  TagType child_type = child->type;

  switch (parent_type) {
    case LI:
      return child_type != LI;

    case DT:
    case DD:
      return child_type != DT && child_type != DD;

    case P:
      return tag_type_allowed_in_paragraph(child_type);

    case COLGROUP:
      return child_type == COL;

    case RB:
    case RT:
    case RP:
      return child_type != RB && child_type != RT && child_type != RP;

    case OPTGROUP:
      return child_type != OPTGROUP;

    case TR:
      return child_type != TR;

    case TD:
    case TH:
      return child_type != TD && child_type != TH && child_type != TR;

    default:
      return true;
  }
}

static inline void tag_set_custom_name(Tag *tag, const char *name, size_t length) {
  if (!tag) return;
  if (!name) {
    tag_reset(tag);
    return;
  }

  if (length > TREE_SITTER_HTML_MAX_CUSTOM_TAG_NAME_LENGTH) {
    length = TREE_SITTER_HTML_MAX_CUSTOM_TAG_NAME_LENGTH;
  }
  memcpy(tag->custom_tag_name, name, length);
  tag->custom_tag_name[length] = '\0';
  tag->custom_tag_name_length = (uint8_t)length;
}

static inline Tag tag_make(TagType type) {
  Tag tag;
  tag_reset(&tag);
  tag.type = type;
  return tag;
}

static inline Tag tag_for_name(const char *name) {
  if (!name) {
    return tag_make(END_OF_VOID_TAGS);
  }

  for (size_t i = 0; i < TAG_NAME_ENTRY_COUNT; i++) {
    if (strcmp(name, TAG_NAME_ENTRIES[i].name) == 0) {
      return tag_make(TAG_NAME_ENTRIES[i].type);
    }
  }

  Tag tag = tag_make(CUSTOM);
  tag_set_custom_name(&tag, name, strlen(name));
  return tag;
}

#ifdef __cplusplus
}
#endif

#undef TREE_SITTER_HTML_VOID_TAG_LIST
#undef TREE_SITTER_HTML_NON_VOID_TAG_LIST

#endif