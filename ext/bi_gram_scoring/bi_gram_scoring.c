#include "assert.h"
#include "stdint.h"
#include "math.h"
#include "ruby.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))

/*
 * An entry, each has a key (Ruby String) used to identify it, and a value (Ruby String) used to generate the similarity score with another entry.
 */
typedef struct entry {
  VALUE key;
  VALUE val;
  struct entry *next;
} entry_t;

#define ENTRY_KEY_PTR(entry) (RSTRING(entry->key)->ptr)
#define ENTRY_KEY_LEN(entry) (RSTRING(entry->key)->len)

#define ENTRY_VAL_PTR(entry) (RSTRING(entry->val)->ptr)
#define ENTRY_VAL_LEN(entry) (RSTRING(entry->val)->len)

static entry_t *entry_init_(VALUE key, VALUE val) {
  entry_t *entry = malloc(sizeof(*entry));
  entry->key = key;
  entry->val = val;
  entry->next = NULL;
  return entry;
}

static void entry_mark_(entry_t *entry) {
  rb_gc_mark(entry->key);
  rb_gc_mark(entry->val);
}

static void entry_free_(entry_t *entry) {
  free(entry);
}

#define KEY_HASH_SIZE (1U)

static uint8_t key_hash_(VALUE string) {
  int i;
  uint8_t rval = 0x00;

  for (i = 0; i < RSTRING(string)->len; i++) {
    rval ^= *((uint8_t *)RSTRING(string)->ptr + i);
  }
  return rval;
}

/*
 * Bi-grams are two-character (actually, two-octet) combinations.
 */
typedef uint16_t bi_gram_t;
#define BI_GRAM_VALUE_COUNT (1U << (8*sizeof(bi_gram_t) - 1))
#define BI_GRAM_VALUE(ptr, idx) ((*((uint8_t *)ptr + idx) << 8) + *((uint8_t *)ptr + idx + 1))

/*
 * BiGramScoring instance data, maintaining a list of entries, in the order added, and a hash of these entries, by key.
 * Adding an entry with the same key as an existing entry replaces the value of the existing entry.
 * Adding an entry after the scoring instance has reached its configured capacity ejects the first entry.
 */
typedef struct bi_gram_scoring {
  unsigned int capacity;
  unsigned int entries_count;
  entry_t **entries;
  entry_t *entries_hash[1U << (KEY_HASH_SIZE << 3)];
  unsigned int bi_gram_counts[BI_GRAM_VALUE_COUNT];
  float bi_gram_weights[BI_GRAM_VALUE_COUNT];
} bi_gram_scoring_t;

static VALUE bi_gram_scoring;

static void bi_gram_scoring_mark_(bi_gram_scoring_t *instance) {
  int i;

  for (i = 0; i < instance->entries_count; i++) {
    entry_mark_(instance->entries[i]);
  }
}

static void bi_gram_scoring_free_(bi_gram_scoring_t *instance) {
  int i;

  for (i = 0; i < instance->entries_count; i++) {
    entry_free_(instance->entries[i]);
    instance->entries[i] = NULL;
  }
  free(instance->entries);
  free(instance);
}

/*
 * Public singleton methos: new(capacity)
 */
static VALUE rb_bi_gram_scoring_new_(VALUE rb_class, VALUE rb_capacity) {
  int capacity = NUM2INT(rb_capacity);

  if (0 >= capacity) {
    rb_raise(rb_eArgError, "capacity must be a positive number");
    return Qnil;
  }
  bi_gram_scoring_t *instance = malloc(sizeof(bi_gram_scoring_t));
  instance->capacity = capacity;
  instance->entries_count = 0;
  instance->entries = malloc(capacity*sizeof(entry_t *));
  memset(instance->entries_hash, 0, sizeof(instance->entries_hash));
  return Data_Wrap_Struct(rb_class, bi_gram_scoring_mark_, bi_gram_scoring_free_, instance);
}

#define BIT_SET_SIZE(count) (((count) + 7)/8)
#define GET_BIT(set, i) (set[(i)/8] & (1U << ((i) % 8)))
#define SET_BIT(set, i) do { set[(i)/8] |= (1U << ((i) % 8)); } while (0);

/*
 * Adjust bigram document-frequency counts when an entry is added (increment == +1) or removed (increment == -1).
 */
static void increment_bi_gram_counts_(bi_gram_scoring_t *instance, const entry_t *entry, unsigned int increment) {
  const int count = ENTRY_VAL_LEN(entry) - 1;
  int i;
  unsigned char flags[BIT_SET_SIZE(BI_GRAM_VALUE_COUNT)];
  memset(flags, 0, sizeof(flags));
  for (i = 0; i < count; i++) {
    const bi_gram_t bi_gram = BI_GRAM_VALUE(ENTRY_VAL_PTR(entry), i);
    const size_t flag_posn = bi_gram/8;
    const unsigned char flag_mask = 1U << (bi_gram % 8);

    if (!GET_BIT(flags, bi_gram)) {
      assert(0 <= (instance->bi_gram_counts[bi_gram] + increment));
      instance->bi_gram_counts[bi_gram] += increment;
      SET_BIT(flags, bi_gram);
    }
  }
}

/*
 * Weights applied to each bi-gram in the scoring algorithm, based on Inverse Document Frequency.
 */
static void initialize_bi_gram_weights_(bi_gram_scoring_t *instance) {
  int i;
  for (i = 0; i < BI_GRAM_VALUE_COUNT; i++) {
    const unsigned int count = instance->bi_gram_counts[i];
    instance->bi_gram_weights[i] = 0 < count ? MAX(0.0, logf((instance->entries_count - count + 0.5)/(count + 0.5))) : 0.0;
  }
}

/*
 * Similarity score of two entries.
 */
static double score_(const bi_gram_scoring_t *instance, const entry_t *entry_1, const entry_t *entry_2) {
  double total = 0.0;
  double joint = 0.0;
  const int count_1 = ENTRY_VAL_LEN(entry_1) - 1;
  const int count_2 = ENTRY_VAL_LEN(entry_2) - 1;
  int i, j;
  char *flags;
  double score;

  for (i = 0; i < count_1; i++) {
    total += instance->bi_gram_weights[BI_GRAM_VALUE(ENTRY_VAL_PTR(entry_1), i)];
  }
  for (i = 0; i < count_2; i++) {
    total += instance->bi_gram_weights[BI_GRAM_VALUE(ENTRY_VAL_PTR(entry_2), i)];
  }
  flags = calloc(BIT_SET_SIZE(count_2), 1);
  for (i = 0; i < count_1; i++) {
    const bi_gram_t bi_gram = BI_GRAM_VALUE(ENTRY_VAL_PTR(entry_1), i);
    for (j = 0; j < count_2; j++) {
      if (!GET_BIT(flags, j) && bi_gram == BI_GRAM_VALUE(ENTRY_VAL_PTR(entry_2), j)) {
        joint += instance->bi_gram_weights[bi_gram];
        SET_BIT(flags, j);
        break;
      }
    }
  }
  free(flags);
  return 0.0 < total ? 2.0*joint/total : 0.0;
}

static entry_t *hash_find_entry_(bi_gram_scoring_t *instance, VALUE string) {
  entry_t *entry;
  void *string_ptr = RSTRING(string)->ptr;
  size_t string_len = RSTRING(string)->len;

  for (entry = instance->entries_hash[key_hash_(string)]; entry; entry = entry->next) {
    if (string_len == ENTRY_KEY_LEN(entry) && !memcmp(string_ptr, ENTRY_KEY_PTR(entry), string_len)) {
      break;
    }
  }
  return entry;
}

static void hash_link_entry_(bi_gram_scoring_t *instance, entry_t *entry) {
  size_t hash = key_hash_(entry->key);

  entry->next = instance->entries_hash[hash];
  instance->entries_hash[hash] = entry;
}

static void hash_drop_entry_(bi_gram_scoring_t *instance, entry_t *entry) {
  entry_t **ptr;

  for (ptr = &instance->entries_hash[key_hash_(entry->key)]; *ptr; ptr = &(*ptr)->next) {
    if (entry == *ptr) break;
  }
  assert(entry == *ptr);
  *ptr = entry->next;
}

static void remove_head_entry_(bi_gram_scoring_t *instance) {
  entry_t *entry;
  int i;

  assert(0 < instance->entries_count);
  entry = instance->entries[0];
  for (i = 1; i < instance->entries_count; i++) {
    instance->entries[i - 1] = instance->entries[i];
  }
  instance->entries_count -= 1;
  hash_drop_entry_(instance, entry);
  increment_bi_gram_counts_(instance, entry, -1);
  entry_free_(entry);
  
}

/*
 * Public instance method: add_entry(key, val)
 */
static VALUE rb_bi_gram_scoring_add_entry_(VALUE rb_self, VALUE rb_key, VALUE rb_val) {
  struct bi_gram_scoring *instance;
  entry_t *entry;
  double max_score = -1.0;
  VALUE max_score_key = Qnil;
  int i;
  VALUE retval;

  if (T_STRING != TYPE(rb_key)) {
    rb_raise(rb_eArgError, "key must be a string");
    return Qnil;
  }
  if (T_STRING != TYPE(rb_val)) {
    rb_raise(rb_eArgError, "val must be a string");
    return Qnil;
  }
  Data_Get_Struct(rb_self, bi_gram_scoring_t, instance);

  if ((entry = hash_find_entry_(instance, rb_key))) {
    increment_bi_gram_counts_(instance, entry, -1);
    entry->val = rb_val;
    increment_bi_gram_counts_(instance, entry, +1);
  } else {
    while (instance->entries_count >= instance->capacity) {
      remove_head_entry_(instance);
    }
    instance->entries[instance->entries_count++] = entry = entry_init_(rb_key, rb_val);
    hash_link_entry_(instance, entry);
    increment_bi_gram_counts_(instance, entry, +1);
  }
  initialize_bi_gram_weights_(instance);

  for (i = 0; i < instance->entries_count && entry != instance->entries[i]; i++) {
    double score = score_(instance, entry, instance->entries[i]);
    if (max_score < score) {
      max_score = score;
      max_score_key = instance->entries[i]->key;
    }
  }
  retval = rb_ary_new();
  rb_ary_push(retval, rb_float_new(max_score));
  rb_ary_push(retval, max_score_key);
  return retval;
}

void Init_bi_gram_scoring() {
  bi_gram_scoring = rb_define_class("BiGramScoring", rb_cObject);
  rb_define_singleton_method(bi_gram_scoring, "new", rb_bi_gram_scoring_new_, 1);
  rb_define_method(bi_gram_scoring, "add_entry", rb_bi_gram_scoring_add_entry_, 2);
}
