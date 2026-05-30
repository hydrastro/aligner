#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VEC_INIT_CAP 64

typedef struct { unsigned long *p; size_t n, cap; } vec_ul;
typedef struct { unsigned int  *p; size_t n, cap; } vec_ui;

static int vec_ul_push(vec_ul *v, unsigned long x){
  size_t nc; unsigned long *np;
  if (v->n == v->cap) {
    nc = v->cap ? (v->cap * 2u) : VEC_INIT_CAP;
    np = (unsigned long*)realloc(v->p, nc * sizeof(unsigned long));
    if (!np) return -1; v->p = np; v->cap = nc;
  }
  v->p[v->n++] = x; return 0;
}
static int vec_ui_push(vec_ui *v, unsigned int x){
  size_t nc; unsigned int *np;
  if (v->n == v->cap) {
    nc = v->cap ? (v->cap * 2u) : VEC_INIT_CAP;
    np = (unsigned int*)realloc(v->p, nc * sizeof(unsigned int));
    if (!np) return -1; v->p = np; v->cap = nc;
  }
  v->p[v->n++] = x; return 0;
}
static void vec_ul_free(vec_ul *v){ if (v->p) free(v->p); v->p=NULL; v->n=v->cap=0; }
static void vec_ui_free(vec_ui *v){ if (v->p) free(v->p); v->p=NULL; v->n=v->cap=0; }

static int write_bin_ul(const char *path, const vec_ul *v){
  FILE *f = fopen(path, "wb"); if (!f) return -1;
  if (v->n && fwrite(v->p, sizeof(unsigned long), v->n, f) != v->n) { fclose(f); return -1; }
  fclose(f); return 0;
}
static int write_bin_ui(const char *path, const vec_ui *v){
  FILE *f = fopen(path, "wb"); if (!f) return -1;
  if (v->n && fwrite(v->p, sizeof(unsigned int), v->n, f) != v->n) { fclose(f); return -1; }
  fclose(f); return 0;
}

static int hex_to_ul(const char *s, unsigned long *out){
  unsigned long v = 0; int any = 0; char c;
  while ((c = *s++) != 0) {
    unsigned int d;
    if (c >= '0' && c <= '9') d = (unsigned int)(c - '0');
    else if (c >= 'A' && c <= 'F') d = (unsigned int)(c - 'A' + 10u);
    else if (c >= 'a' && c <= 'f') d = (unsigned int)(c - 'a' + 10u);
    else break;
    any = 1; v = (v << 4) | d;
  }
  if (!any) return -1;
  *out = v; return 0;
}

typedef struct {
  const char *unicode_data;
  const char *case_folding;
  const char *comp_excl;
  const char *gb_prop;
  const char *emoji_data;
  const char *out_dir;
} args_t;

static void usage(const char *argv0){
  fprintf(stderr, "usage: %s --unicode-data file --case-folding file --comp-excl file --gb-prop file --emoji-data file --out-dir DIR\n", argv0);
}

static int parse_args(int argc, char **argv, args_t *a){
  int i;
  a->unicode_data = a->case_folding = a->comp_excl = a->gb_prop = a->emoji_data = a->out_dir = NULL;
  for (i=1;i<argc;i++){
    if (strcmp(argv[i], "--unicode-data")==0 && i+1<argc) a->unicode_data = argv[++i];
    else if (strcmp(argv[i], "--case-folding")==0 && i+1<argc) a->case_folding = argv[++i];
    else if (strcmp(argv[i], "--comp-excl")==0 && i+1<argc) a->comp_excl = argv[++i];
    else if (strcmp(argv[i], "--gb-prop")==0 && i+1<argc) a->gb_prop = argv[++i];
    else if (strcmp(argv[i], "--emoji-data")==0 && i+1<argc) a->emoji_data = argv[++i];
    else if (strcmp(argv[i], "--out-dir")==0 && i+1<argc) a->out_dir = argv[++i];
    else { usage(argv[0]); return -1; }
  }
  if (!a->unicode_data || !a->case_folding || !a->comp_excl || !a->gb_prop || !a->emoji_data || !a->out_dir) { usage(argv[0]); return -1; }
  return 0;
}

typedef struct { unsigned long cp; unsigned long ccc; } rec_ccc;
typedef struct { unsigned long cp; unsigned int compat; vec_ul seq; } rec_decomp;
typedef struct { unsigned long a,b,val; } rec_pair;
typedef struct { unsigned long key; unsigned int off, cnt; } rec_foldhead;

static int cmp_ccc(const void *aa, const void *bb){
  const rec_ccc *a = (const rec_ccc*)aa, *b = (const rec_ccc*)bb;
  if (a->cp < b->cp) return -1; if (a->cp > b->cp) return 1; return 0;
}
static int cmp_ul(const void *aa, const void *bb){
  unsigned long a = *(const unsigned long*)aa, b = *(const unsigned long*)bb;
  if (a < b) return -1; if (a > b) return 1; return 0;
}
static int cmp_pair(const void *aa, const void *bb){
  const rec_pair *a = (const rec_pair*)aa, *b = (const rec_pair*)bb;
  if (a->a < b->a) return -1; if (a->a > b->a) return 1;
  if (a->b < b->b) return -1; if (a->b > b->b) return 1;
  return 0;
}
static int cmp_ul2(const void *aa, const void *bb){
  const unsigned long *a = (const unsigned long*)aa;
  const unsigned long *b = (const unsigned long*)bb;
  if (a[0] < b[0]) return -1; if (a[0] > b[0]) return 1;
  if (a[1] < b[1]) return -1; if (a[1] > b[1]) return 1;
  return 0;
}

static int parse_unicode_data(const char *path,
                              vec_ul *ccc_keys, vec_ul *ccc_vals,
                              rec_decomp **out_decomp, size_t *out_decomp_n,
                              vec_ul *to_upper_keys, vec_ul *to_upper_vals,
                              vec_ul *to_lower_keys, vec_ul *to_lower_vals) {
  FILE *f = fopen(path, "rb");
  char line[4096];
  rec_ccc *arr_ccc = NULL; size_t n_ccc = 0, cap_ccc = 0;
  rec_decomp *de = NULL; size_t nde = 0, capde = 0;
  if (!f) return -1;

  while (fgets(line, sizeof(line), f)) {
    char *p = line, *fields[15];
    int i;
    for (i=0;i<15;i++){
      char *semi = strchr(p, ';');
      fields[i] = p;
      if (!semi){ if (i<14) fields[i+1] = NULL; break; }
      *semi = 0; p = semi + 1;
    }
    if (!fields[0]) continue;

    {
      unsigned long cp;
      if (hex_to_ul(fields[0], &cp) != 0) continue;

      if (fields[3] && *fields[3]) {
        unsigned long ccc = 0; const char *q = fields[3];
        while (*q >= '0' && *q <= '9') { ccc = ccc*10ul + (unsigned long)(*q - '0'); q++; }
        if (n_ccc == cap_ccc) {
          size_t nc = cap_ccc ? cap_ccc*2u : 1024u;
          rec_ccc *np = (rec_ccc*)realloc(arr_ccc, nc * sizeof(rec_ccc));
          if (!np) { fclose(f); return -1; }
          arr_ccc = np; cap_ccc = nc;
        }
        arr_ccc[n_ccc].cp = cp;
        arr_ccc[n_ccc].ccc = ccc;
        n_ccc++;
      }

      if (fields[5] && *fields[5]) {
        const char *q = fields[5];
        int compat = 0;
        vec_ul seq; seq.p = NULL; seq.n = 0; seq.cap = 0;

        if (*q == '<') {
          compat = 1;
          while (*q && *q != '>') q++;
          if (*q == '>') q++;
          while (*q == ' ') q++;
        }
        while (*q) {
          const char *start = q;
          char hex[16]; size_t l; unsigned long u;
          while (*q && *q != ' ') q++;
          l = (size_t)(q - start);
          if (l >= sizeof(hex)) l = sizeof(hex)-1u;
          memcpy(hex, start, l); hex[l] = '\0';
          if (hex_to_ul(hex, &u) != 0) { vec_ul_free(&seq); seq.n = 0; break; }
          if (vec_ul_push(&seq, u) != 0) { vec_ul_free(&seq); seq.n = 0; break; }
          while (*q == ' ') q++;
        }
        if (seq.n > 0) {
          if (nde == capde) {
            size_t nc = capde ? capde*2u : 512u;
            rec_decomp *npd = (rec_decomp*)realloc(de, nc * sizeof(rec_decomp));
            if (!npd) { vec_ul_free(&seq); fclose(f); return -1; }
            de = npd; capde = nc;
          }
          de[nde].cp = cp; de[nde].compat = (unsigned int)compat; de[nde].seq = seq; nde++;
        } else {
          vec_ul_free(&seq);
        }
      }

      if (fields[12] && *fields[12]) {
        unsigned long up;
        if (hex_to_ul(fields[12], &up) == 0) {
          if (vec_ul_push(to_upper_keys, cp) != 0) { fclose(f); return -1; }
          if (vec_ul_push(to_upper_vals, up) != 0)  { fclose(f); return -1; }
        }
      }
      if (fields[13] && *fields[13]) {
        unsigned long lo;
        if (hex_to_ul(fields[13], &lo) == 0) {
          if (vec_ul_push(to_lower_keys, cp) != 0) { fclose(f); return -1; }
          if (vec_ul_push(to_lower_vals, lo) != 0) { fclose(f); return -1; }
        }
      }
    }
  }
  fclose(f);

  if (n_ccc > 0) qsort(arr_ccc, n_ccc, sizeof(arr_ccc[0]), cmp_ccc);
  {
    size_t i;
    for (i=0;i<n_ccc;i++){
      if (vec_ul_push(ccc_keys, arr_ccc[i].cp) != 0) return -1;
      if (vec_ul_push(ccc_vals, arr_ccc[i].ccc) != 0) return -1;
    }
  }
  free(arr_ccc);

  if (nde > 0) {
    size_t i;
    for (i=1;i<nde;i++){
      rec_decomp tmp = de[i];
      size_t j = i;
      while (j>0 && de[j-1].cp > tmp.cp) { de[j] = de[j-1]; j--; }
      de[j] = tmp;
    }
  }

  *out_decomp = de; *out_decomp_n = nde;
  return 0;
}

static int load_comp_excl(const char *path, vec_ul *ex){
  FILE *f = fopen(path, "rb");
  char line[1024];
  if (!f) return -1;
  while (fgets(line, sizeof(line), f)) {
    char *p = line, *hash; unsigned long cp;
    if (*p == '#' || *p == '\n' || *p == '\0') continue;
    hash = strchr(p, '#'); if (hash) *hash = 0;
    while (*p==' '||*p=='\t') p++;
    if (*p == 0) continue;
    {
      char *q = p;
      while (*q && *q!=' ' && *q!='\t' && *q!='\n') q++;
      *q = 0;
    }
    if (hex_to_ul(p, &cp) == 0) {
      if (vec_ul_push(ex, cp) != 0) { fclose(f); return -1; }
    }
  }
  fclose(f);
  if (ex->n > 0) qsort(ex->p, ex->n, sizeof(unsigned long), cmp_ul);
  return 0;
}
static int in_ul_set(const vec_ul *ex, unsigned long x){
  size_t lo = 0, hi = ex->n;
  while (lo < hi){
    size_t mid = lo + (hi - lo)/2u;
    unsigned long k = ex->p[mid];
    if (x < k) hi = mid;
    else if (x > k) lo = mid + 1u;
    else return 1;
  }
  return 0;
}

static int build_comp(const rec_decomp *de, size_t nde, const vec_ul *ex,
                      vec_ul *keys_a, vec_ul *keys_b, vec_ul *vals){
  size_t i;
  for (i=0;i<nde;i++){
    if (de[i].compat) continue;
    if (de[i].seq.n == 2u) {
      unsigned long cp = de[i].cp;
      if (in_ul_set(ex, cp)) continue;
      if (vec_ul_push(keys_a, de[i].seq.p[0]) != 0) return -1;
      if (vec_ul_push(keys_b, de[i].seq.p[1]) != 0) return -1;
      if (vec_ul_push(vals,   cp) != 0) return -1;
    }
  }
  if (keys_a->n > 0) {
    size_t n = keys_a->n, k;
    unsigned long *tmp = (unsigned long*)malloc(n * 3u * sizeof(unsigned long));
    if (!tmp) return -1;
    for (k=0;k<n;k++){ tmp[3u*k+0]=keys_a->p[k]; tmp[3u*k+1]=keys_b->p[k]; tmp[3u*k+2]=vals->p[k]; }
    qsort(tmp, n, 3u*sizeof(unsigned long), cmp_ul2);
    for (k=0;k<n;k++){ keys_a->p[k]=tmp[3u*k+0]; keys_b->p[k]=tmp[3u*k+1]; vals->p[k]=tmp[3u*k+2]; }
    free(tmp);
  }
  return 0;
}

static int load_case_folding(const char *path,
                             vec_ul *keys, vec_ui *off, vec_ui *cnt, vec_ul *pool){
  FILE *f = fopen(path, "rb");
  char line[4096];
  if (!f) return -1;

  while (fgets(line, sizeof(line), f)) {
    char *p = line; char *semi; unsigned long cp;
    char status;
    vec_ul seq; seq.p=NULL; seq.n=0; seq.cap=0;

    if (*p=='#' || *p=='\n' || *p=='\0') continue;

    semi = strchr(p, ';'); if (!semi) continue;
    *semi = 0;
    if (hex_to_ul(p, &cp) != 0) continue;

    p = semi+1; while (*p==' ') p++;
    if (!*p) continue;
    status = *p;
    p = strchr(p, ';'); if (!p) continue; p++; while (*p==' ') p++;

    while (*p && *p!='#' && *p!='\n') {
      char *q = p; char hex[16]; size_t l; unsigned long u;
      while (*q && *q!=' ' && *q!='\n' && *q!='#') q++;
      l = (size_t)(q - p); if (l >= sizeof(hex)) l = sizeof(hex)-1u;
      memcpy(hex, p, l); hex[l] = '\0';
      if (hex_to_ul(hex, &u) == 0) {
        if (vec_ul_push(&seq, u) != 0) { vec_ul_free(&seq); fclose(f); return -1; }
      }
      p = q; while (*p==' ') p++;
    }
    if (seq.n > 0) {
      if (status != 'F' && seq.n > 1u) seq.n = 1u;

      if (vec_ul_push(keys, cp) != 0) { vec_ul_free(&seq); fclose(f); return -1; }
      if (vec_ui_push(off, (unsigned int)pool->n) != 0) { vec_ul_free(&seq); fclose(f); return -1; }
      if (vec_ui_push(cnt, (unsigned int)seq.n) != 0)  { vec_ul_free(&seq); fclose(f); return -1; }
      {
        size_t i;
        for (i=0;i<seq.n;i++) if (vec_ul_push(pool, seq.p[i]) != 0) { vec_ul_free(&seq); fclose(f); return -1; }
      }
    }
    vec_ul_free(&seq);
  }
  fclose(f);

  if (keys->n > 0) {
    size_t n = keys->n, i, j;
    for (i=1;i<n;i++){
      unsigned long kkey = keys->p[i];
      unsigned int  koff = off->p[i];
      unsigned int  kcnt = cnt->p[i];
      size_t base = (size_t)koff, len = (size_t)kcnt, tail;
      unsigned long *tmp = NULL;
      if (len > 0) {
        tmp = (unsigned long*)malloc(len * sizeof(unsigned long));
        if (!tmp) return -1;
        for (j=0;j<len;j++) tmp[j] = pool->p[base + j];
      }
      j = i;
      while (j>0 && keys->p[j-1] > kkey) {
        keys->p[j] = keys->p[j-1];
        off->p[j]  = off->p[j-1];
        cnt->p[j]  = cnt->p[j-1];
        j--;
      }
      keys->p[j] = kkey; off->p[j] = koff; cnt->p[j] = kcnt;
      if (tmp) free(tmp);
    }
  }
  return 0;
}

static int load_gb_ranges(const char *path, vec_ul *out){
  FILE *f = fopen(path, "rb");
  char line[1024];
  if (!f) return -1;

  while (fgets(line, sizeof(line), f)) {
    char *p=line, *semi, *hash, *dots, *end, *prop;
    unsigned long a, b, tag = 0ul;

    if (*p=='#' || *p=='\n' || *p=='\0') continue;
    semi = strchr(p,';'); if (!semi) continue; *semi = 0;
    hash = strchr(semi+1,'#'); if (hash) *hash=0;

    while (*p==' '||*p=='\t') p++;
    dots = strstr(p,"..");
    if (dots) { *dots = 0; if (hex_to_ul(p,&a)!=0) continue; if (hex_to_ul(dots+2,&b)!=0) continue; }
    else { if (hex_to_ul(p,&a)!=0) continue; b = a; }

    prop = semi+1; while (*prop==' '||*prop=='\t') prop++;
    end = prop; while (*end && *end!=' '&&*end!='\t'&&*end!='\n') end++; *end=0;

    if (strcmp(prop,"CR")==0) tag=1ul;
    else if (strcmp(prop,"LF")==0) tag=2ul;
    else if (strcmp(prop,"Control")==0) tag=3ul;
    else if (strcmp(prop,"Extend")==0) tag=4ul;
    else if (strcmp(prop,"ZWJ")==0) tag=5ul;
    else if (strcmp(prop,"SpacingMark")==0) tag=6ul;
    else if (strcmp(prop,"Prepend")==0) tag=7ul;
    else if (strcmp(prop,"Regional_Indicator")==0) tag=8ul;
    else if (strcmp(prop,"L")==0) tag=9ul;
    else if (strcmp(prop,"V")==0) tag=10ul;
    else if (strcmp(prop,"T")==0) tag=11ul;
    else if (strcmp(prop,"LV")==0) tag=12ul;
    else if (strcmp(prop,"LVT")==0) tag=13ul;
    else tag=0ul;

    if (vec_ul_push(out, a)!=0) { fclose(f); return -1; }
    if (vec_ul_push(out, b)!=0) { fclose(f); return -1; }
    if (vec_ul_push(out, tag)!=0){ fclose(f); return -1; }
  }
  fclose(f);
  return 0;
}

static int load_ep_ranges(const char *path, vec_ul *out){
  FILE *f = fopen(path, "rb");
  char line[1024];
  if (!f) return -1;
  while (fgets(line,sizeof(line),f)) {
    char *p=line, *semi, *hash, *dots, *prop, *end;
    unsigned long a, b;
    if (*p=='#'||*p=='\n'||*p=='\0') continue;
    hash = strchr(p,'#'); if (hash) *hash = 0;
    semi = strchr(p,';'); if (!semi) continue; *semi=0;

    while (*p==' '||*p=='\t') p++;
    dots = strstr(p,"..");
    if (dots) { *dots=0; if (hex_to_ul(p,&a)!=0) continue; if (hex_to_ul(dots+2,&b)!=0) continue; }
    else { if (hex_to_ul(p,&a)!=0) continue; b=a; }

    prop = semi+1; while (*prop==' '||*prop=='\t') prop++;
    end = prop; while (*end && *end!=' '&&*end!='\t'&&*end!='\n') end++; *end=0;

    if (strcmp(prop, "Extended_Pictographic")==0) {
      if (vec_ul_push(out, a)!=0) { fclose(f); return -1; }
      if (vec_ul_push(out, b)!=0) { fclose(f); return -1; }
    }
  }
  fclose(f);
  return 0;
}

int main(int argc, char **argv){
  args_t a;
  rec_decomp *de = NULL; size_t nde = 0;
  vec_ul ccc_keys; vec_ul ccc_vals;
  vec_ul to_up_k, to_up_v, to_lo_k, to_lo_v;
  vec_ul comp_ex; vec_ul comp_a, comp_b, comp_v;
  vec_ul de_keys; vec_ui de_cnt; vec_ui de_compat; vec_ui de_off; vec_ul de_pool;
  vec_ul fold_keys; vec_ui fold_off; vec_ui fold_cnt; vec_ul fold_pool;
  vec_ul gb_triplets; vec_ul ep_pairs;

  char path[512];

  ccc_keys.p=ccc_vals.p=NULL; ccc_keys.n=ccc_keys.cap=0; ccc_vals.n=ccc_vals.cap=0;
  to_up_k.p=to_up_v.p=to_lo_k.p=to_lo_v.p=NULL;
  to_up_k.n=to_up_k.cap=to_up_v.n=to_up_v.cap=0;
  to_lo_k.n=to_lo_k.cap=to_lo_v.n=to_lo_v.cap=0;

  comp_ex.p=comp_a.p=comp_b.p=comp_v.p=NULL;
  comp_ex.n=comp_ex.cap=comp_a.n=comp_a.cap=comp_b.n=comp_b.cap=comp_v.n=comp_v.cap=0;

  de_keys.p=de_pool.p=NULL; de_cnt.p=de_compat.p=de_off.p=NULL;
  de_keys.n=de_keys.cap=de_pool.n=de_pool.cap=0;
  de_cnt.n=de_cnt.cap=de_compat.n=de_compat.cap=de_off.n=de_off.cap=0;

  fold_keys.p=fold_pool.p=NULL; fold_off.p=fold_cnt.p=NULL;
  fold_keys.n=fold_keys.cap=fold_pool.n=fold_pool.cap=0;
  fold_off.n=fold_off.cap=fold_cnt.n=fold_cnt.cap=0;

  gb_triplets.p=ep_pairs.p=NULL; gb_triplets.n=gb_triplets.cap=0; ep_pairs.n=ep_pairs.cap=0;

  if (parse_args(argc, argv, &a) != 0) return 2;

  if (parse_unicode_data(a.unicode_data, &ccc_keys, &ccc_vals, &de, &nde,
                         &to_up_k, &to_up_v, &to_lo_k, &to_lo_v) != 0) goto fail;

  if (load_comp_excl(a.comp_excl, &comp_ex) != 0) goto fail;

  if (build_comp(de, nde, &comp_ex, &comp_a, &comp_b, &comp_v) != 0) goto fail;

  {
    size_t i; unsigned int pool_off = 0u;
    for (i=0;i<nde;i++){
      if (vec_ul_push(&de_keys, de[i].cp)!=0) goto fail;
      if (vec_ui_push(&de_cnt, (unsigned int)de[i].seq.n)!=0) goto fail;
      if (vec_ui_push(&de_compat, (unsigned int)de[i].compat)!=0) goto fail;
      if (vec_ui_push(&de_off, pool_off)!=0) goto fail;
      {
        size_t j;
        for (j=0;j<de[i].seq.n;j++){
          if (vec_ul_push(&de_pool, de[i].seq.p[j])!=0) goto fail;
          pool_off++;
        }
      }
    }
  }

  if (load_case_folding(a.case_folding, &fold_keys, &fold_off, &fold_cnt, &fold_pool) != 0) goto fail;

  if (load_gb_ranges(a.gb_prop, &gb_triplets) != 0) goto fail;

  if (load_ep_ranges(a.emoji_data, &ep_pairs) != 0) goto fail;

  sprintf(path, "%s/ccc_keys.bin", a.out_dir); if (write_bin_ul(path, &ccc_keys)!=0) goto fail;
  sprintf(path, "%s/ccc_vals.bin", a.out_dir); if (write_bin_ul(path, &ccc_vals)!=0) goto fail;

  sprintf(path, "%s/decomp_keys.bin", a.out_dir); if (write_bin_ul(path, &de_keys)!=0) goto fail;
  sprintf(path, "%s/decomp_cnt.bin", a.out_dir); if (write_bin_ui(path, &de_cnt)!=0) goto fail;
  sprintf(path, "%s/decomp_compat.bin", a.out_dir); if (write_bin_ui(path, &de_compat)!=0) goto fail;
  sprintf(path, "%s/decomp_off.bin", a.out_dir); if (write_bin_ui(path, &de_off)!=0) goto fail;
  sprintf(path, "%s/decomp_pool.bin", a.out_dir); if (write_bin_ul(path, &de_pool)!=0) goto fail;

  sprintf(path, "%s/comp_keys_a.bin", a.out_dir); if (write_bin_ul(path, &comp_a)!=0) goto fail;
  sprintf(path, "%s/comp_keys_b.bin", a.out_dir); if (write_bin_ul(path, &comp_b)!=0) goto fail;
  sprintf(path, "%s/comp_vals.bin", a.out_dir);   if (write_bin_ul(path, &comp_v)!=0) goto fail;

  sprintf(path, "%s/fold_keys.bin", a.out_dir); if (write_bin_ul(path, &fold_keys)!=0) goto fail;
  sprintf(path, "%s/fold_off.bin",  a.out_dir); if (write_bin_ui(path, &fold_off)!=0) goto fail;
  sprintf(path, "%s/fold_cnt.bin",  a.out_dir); if (write_bin_ui(path, &fold_cnt)!=0) goto fail;
  sprintf(path, "%s/fold_pool.bin", a.out_dir); if (write_bin_ul(path, &fold_pool)!=0) goto fail;

  sprintf(path, "%s/toupper_keys.bin", a.out_dir); if (write_bin_ul(path, &to_up_k)!=0) goto fail;
  sprintf(path, "%s/toupper_vals.bin", a.out_dir); if (write_bin_ul(path, &to_up_v)!=0) goto fail;
  sprintf(path, "%s/tolower_keys.bin", a.out_dir); if (write_bin_ul(path, &to_lo_k)!=0) goto fail;
  sprintf(path, "%s/tolower_vals.bin", a.out_dir); if (write_bin_ul(path, &to_lo_v)!=0) goto fail;

  sprintf(path, "%s/gb_ranges.bin", a.out_dir); if (write_bin_ul(path, &gb_triplets)!=0) goto fail;

  sprintf(path, "%s/ep_ranges.bin", a.out_dir); if (write_bin_ul(path, &ep_pairs)!=0) goto fail;

  {
    size_t i;
    for (i=0;i<nde;i++) vec_ul_free(&de[i].seq);
    free(de);
  }
  vec_ul_free(&ccc_keys); vec_ul_free(&ccc_vals);
  vec_ul_free(&to_up_k); vec_ul_free(&to_up_v); vec_ul_free(&to_lo_k); vec_ul_free(&to_lo_v);
  vec_ul_free(&comp_ex); vec_ul_free(&comp_a); vec_ul_free(&comp_b); vec_ul_free(&comp_v);
  vec_ul_free(&de_keys); vec_ui_free(&de_cnt); vec_ui_free(&de_compat); vec_ui_free(&de_off); vec_ul_free(&de_pool);
  vec_ul_free(&fold_keys); vec_ui_free(&fold_off); vec_ui_free(&fold_cnt); vec_ul_free(&fold_pool);
  vec_ul_free(&gb_triplets); vec_ul_free(&ep_pairs);
  return 0;

fail:
  fprintf(stderr, "pack_ucd: failed\n");
  return 1;
}
