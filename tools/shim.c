/* --- ds shim: minimal stand-ins for the symbols the aligner uses ---------
 * Used only in the single-file amalgam build. The full project uses the
 * real ds library; this shim provides bug-for-bug-compatible drop-ins for
 * the 27 symbols we touch, so the amalgam compiles with libc only.
 */

typedef enum {
    DS_OK            =  0,
    DS_NOT_FOUND     =  1,
    DS_ERR_ALLOC     = -1,
    DS_ERR_INVALID   = -2,
    DS_ERR_NULL      = -3,
    DS_ERR_INTERNAL  = -10
} ds_status_t;

static const char *ds_status_name(ds_status_t s) {
    switch (s) {
        case DS_OK:            return "DS_OK";
        case DS_NOT_FOUND:     return "DS_NOT_FOUND";
        case DS_ERR_ALLOC:     return "DS_ERR_ALLOC";
        case DS_ERR_INVALID:   return "DS_ERR_INVALID";
        case DS_ERR_NULL:      return "DS_ERR_NULL";
        case DS_ERR_INTERNAL:  return "DS_ERR_INTERNAL";
    }
    return "?";
}

/* Arena: singly linked list of (block-header + payload). Allocator returns
   payload pointer; arena_destroy frees every block. */
struct ds__arena_block { struct ds__arena_block *next; };

typedef struct {
    struct ds__arena_block *head;
} ds_arena_t;

typedef struct {
    ds_arena_t *arena;
} ds_context_t;

static void ds_context_init(ds_context_t *c) { c->arena = NULL; }

static ds_status_t ds_arena_create(ds_arena_t *a, size_t cap_hint, ds_context_t *c) {
    (void)cap_hint; (void)c;
    a->head = NULL;
    return DS_OK;
}

static void ds_context_use_arena(ds_context_t *c, ds_arena_t *a) { c->arena = a; }

static void *ds_context_alloc(ds_context_t *c, size_t n) {
    struct ds__arena_block *b;
    if (!c || !c->arena) return malloc(n);
    b = (struct ds__arena_block *)malloc(sizeof(*b) + n);
    if (!b) return NULL;
    b->next = c->arena->head;
    c->arena->head = b;
    return (void *)(b + 1);
}

static void ds_arena_destroy(ds_arena_t *a, ds_context_t *c) {
    struct ds__arena_block *b = a->head, *nx;
    (void)c;
    while (b) { nx = b->next; free(b); b = nx; }
    a->head = NULL;
}

/* Dynamic string buffer. */
typedef struct ds_str {
    char  *buf;
    size_t len;
    size_t cap;
} ds_str_t;

static ds_str_t *str_create(void) {
    ds_str_t *s = (ds_str_t *)malloc(sizeof(*s));
    if (!s) return NULL;
    s->buf = NULL; s->len = 0; s->cap = 0;
    return s;
}
static void str_destroy(ds_str_t *s) {
    if (!s) return;
    free(s->buf);
    free(s);
}
static int str__reserve(ds_str_t *s, size_t need) {
    char *nb;
    size_t newcap;
    if (s->cap >= need) return 0;
    newcap = s->cap ? s->cap : 64;
    while (newcap < need) newcap *= 2;
    nb = (char *)realloc(s->buf, newcap);
    if (!nb) return -1;
    s->buf = nb; s->cap = newcap;
    return 0;
}
static int str_append(ds_str_t *s, const void *data, size_t n) {
    if (str__reserve(s, s->len + n + 1) != 0) return -1;
    memcpy(s->buf + s->len, data, n);
    s->len += n;
    s->buf[s->len] = '\0';
    return 0;
}
static int str_append_cstr(ds_str_t *s, const char *c) {
    return str_append(s, c, strlen(c));
}
static int str_pushc(ds_str_t *s, int c) {
    char b = (char)c;
    return str_append(s, &b, 1);
}
static int str_append_vfmt(ds_str_t *s, const char *fmt, va_list ap) {
    va_list ap2;
    int n;
    va_copy(ap2, ap);
    n = vsnprintf(NULL, 0, fmt, ap2);
    va_end(ap2);
    if (n < 0) return -1;
    if (str__reserve(s, s->len + (size_t)n + 1) != 0) return -1;
    vsnprintf(s->buf + s->len, s->cap - s->len, fmt, ap);
    s->len += (size_t)n;
    return 0;
}
#define FUNC_str_len(s)  ((s) ? (s)->len : (size_t)0)
#define FUNC_str_data(s) ((s) ? (s)->buf : (char *)"")

/* String-keyed map. Linear chain — N is small in our usage. */
struct ds__sm_node {
    char *key;
    void *value;
    struct ds__sm_node *next;
};
typedef struct ds_string_map {
    struct ds__sm_node *head;
    ds_context_t       *ctx;
} ds_string_map_t;

static ds_string_map_t *ds_string_map_create_ctx(ds_context_t *c) {
    ds_string_map_t *m = (ds_string_map_t *)ds_context_alloc(c, sizeof(*m));
    if (!m) return NULL;
    m->head = NULL;
    m->ctx  = c;
    return m;
}
static ds_status_t ds_string_map_put(ds_string_map_t *m, const char *key, void *value) {
    struct ds__sm_node *n;
    size_t klen;
    if (!m || !key) return DS_ERR_NULL;
    for (n = m->head; n; n = n->next) {
        if (strcmp(n->key, key) == 0) { n->value = value; return DS_OK; }
    }
    n = (struct ds__sm_node *)ds_context_alloc(m->ctx, sizeof(*n));
    if (!n) return DS_ERR_ALLOC;
    klen = strlen(key);
    n->key = (char *)ds_context_alloc(m->ctx, klen + 1);
    if (!n->key) return DS_ERR_ALLOC;
    memcpy(n->key, key, klen + 1);
    n->value = value;
    n->next  = m->head;
    m->head  = n;
    return DS_OK;
}
static ds_status_t ds_string_map_get(ds_string_map_t *m, const char *key, void **out) {
    struct ds__sm_node *n;
    if (out) *out = NULL;
    if (!m || !key) return DS_ERR_NULL;
    for (n = m->head; n; n = n->next) {
        if (strcmp(n->key, key) == 0) {
            if (out) *out = n->value;
            return DS_OK;
        }
    }
    return DS_NOT_FOUND;
}
