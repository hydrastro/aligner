#ifndef DS_PERSISTENT_TRIE_H
#define DS_PERSISTENT_TRIE_H

#include "common.h"

#include "context.h"
#include "status.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ds_ptrie_node;
typedef struct ds_ptrie ds_ptrie_t;
typedef struct ds_ptrie_version ds_ptrie_version_t;

typedef void *(*ds_ptrie_clone_value_func_t)(void *value, void *user);
typedef void (*ds_ptrie_destroy_value_func_t)(void *value, void *user);
typedef bool (*ds_ptrie_visit_func_t)(const unsigned char *key, size_t len,
                                      void *value, void *user);

typedef struct ds_ptrie_config {
  ds_ptrie_clone_value_func_t clone_value;
  ds_ptrie_destroy_value_func_t destroy_value;
  void *user;
  ds_context_t *context;
} ds_ptrie_config_t;

void ds_ptrie_config_init(ds_ptrie_config_t *config);
ds_ptrie_t *FUNC(ds_ptrie_create)(const ds_ptrie_config_t *config);
void FUNC(ds_ptrie_destroy)(ds_ptrie_t *trie);
ds_ptrie_version_t *FUNC(ds_ptrie_root)(ds_ptrie_t *trie);
ds_ptrie_version_t *FUNC(ds_ptrie_insert)(ds_ptrie_t *trie,
                                           ds_ptrie_version_t *base,
                                           const unsigned char *key,
                                           size_t len, void *value);
ds_ptrie_version_t *FUNC(ds_ptrie_remove)(ds_ptrie_t *trie,
                                           ds_ptrie_version_t *base,
                                           const unsigned char *key,
                                           size_t len);
ds_ptrie_version_t *FUNC(ds_ptrie_remove_prefix)(ds_ptrie_t *trie,
                                                  ds_ptrie_version_t *base,
                                                  const unsigned char *prefix,
                                                  size_t prefix_len,
                                                  size_t *out_removed);
ds_status_t FUNC(ds_ptrie_get)(ds_ptrie_version_t *version,
                                const unsigned char *key, size_t len,
                                void **out_value);
ds_status_t FUNC(ds_ptrie_contains_prefix)(ds_ptrie_version_t *version,
                                            const unsigned char *prefix,
                                            size_t prefix_len);
size_t FUNC(ds_ptrie_count_prefix)(ds_ptrie_version_t *version,
                                    const unsigned char *prefix,
                                    size_t prefix_len);
ds_ptrie_version_t *FUNC(ds_ptrie_copy_prefix)(
    ds_ptrie_t *trie, ds_ptrie_version_t *destination_base,
    ds_ptrie_version_t *source_version, const unsigned char *source_prefix,
    size_t source_prefix_len, const unsigned char *destination_prefix,
    size_t destination_prefix_len, size_t *out_copied);
ds_ptrie_version_t *FUNC(ds_ptrie_move_prefix)(
    ds_ptrie_t *trie, ds_ptrie_version_t *base, const unsigned char *source_prefix,
    size_t source_prefix_len, const unsigned char *destination_prefix,
    size_t destination_prefix_len, size_t *out_moved);
ds_status_t FUNC(ds_ptrie_longest_prefix)(ds_ptrie_version_t *version,
                                           const unsigned char *key, size_t len,
                                           size_t *out_len, void **out_value);
ds_status_t FUNC(ds_ptrie_visit_prefix)(ds_ptrie_version_t *version,
                                         const unsigned char *prefix,
                                         size_t prefix_len,
                                         ds_ptrie_visit_func_t visit,
                                         void *user);
ds_status_t FUNC(ds_ptrie_visit_range)(ds_ptrie_version_t *version,
                                        const unsigned char *lower,
                                        size_t lower_len,
                                        const unsigned char *upper,
                                        size_t upper_len,
                                        ds_ptrie_visit_func_t visit,
                                        void *user);
void FUNC(ds_ptrie_version_retain)(ds_ptrie_version_t *version);
void FUNC(ds_ptrie_version_release)(ds_ptrie_t *trie,
                                    ds_ptrie_version_t *version);

#ifdef __cplusplus
}
#endif

#endif /* DS_PERSISTENT_TRIE_H */
