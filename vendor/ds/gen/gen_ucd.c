#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct { unsigned long *a; size_t n, cap; } ulvec;
typedef struct { unsigned int *a; size_t n, cap; } uivec;

typedef struct { unsigned long a, b, v; } comp_triplet;
typedef struct { unsigned long start, end, prop; } range3;

int ulvec_push(ulvec *v, unsigned long x) {
  unsigned long *np;
  size_t nc;
  if (v->n == v->cap) {
    nc = v->cap ? v->cap*2u : 8u;
    np = (unsigned long*)realloc(v->a, nc * sizeof(unsigned long));
    if (!np) return -1;
    v->a = np; v->cap = nc;
  }
  v->a[v->n++] = x; return 0;
}
int uivec_push(uivec *v, unsigned int x) {
  unsigned int *np;
  size_t nc;
  if (v->n == v->cap) {
    nc = v->cap ? v->cap*2u : 8u;
    np = (unsigned int*)realloc(v->a, nc * sizeof(unsigned int));
    if (!np) return -1;
    v->a = np; v->cap = nc;
  }
  v->a[v->n++] = x; return 0;
}

char *slurp(const char *path, size_t *out_len) {
  FILE *f = fopen(path, "rb");
  char *buf;
  long n;
  if (!f) return NULL;
  if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
  n = ftell(f);
  if (n < 0) { fclose(f); return NULL; }
  if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
  buf = (char*)malloc((size_t)n + 1u);
  if (!buf) { fclose(f); return NULL; }
  if (fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); fclose(f); return NULL; }
  buf[n] = '\0';
  fclose(f);
  if (out_len) *out_len = (size_t)n;
  return buf;
}

int hex_to_u32(const char *s, unsigned long *out) {
  unsigned long v = 0;
  int any = 0;
  char c;
  while ((c = *s++) != 0) {
    unsigned int d;
    if (c>='0'&&c<='9') d = (unsigned int)(c - '0');
    else if (c>='A'&&c<='F') d = (unsigned int)(c - 'A' + 10);
    else if (c>='a'&&c<='f') d = (unsigned int)(c - 'a' + 10);
    else break;
    any = 1; v = (v << 4) | d;
  }
  if (!any) return -1;
  *out = v; return 0;
}

ulvec CCC_KEYS, CCC_VALS;
ulvec DECOMP_KEYS; uivec DECOMP_CNT, DECOMP_OFF, DECOMP_ISCOMPAT; ulvec DECOMP_POOL;
comp_triplet *COMP = NULL; size_t COMP_N = 0, COMP_CAP = 0;
ulvec TOLOWER_KEYS, TOLOWER_VALS, TOUPPER_KEYS, TOUPPER_VALS;
ulvec FOLD_KEYS, FOLD_POOL; uivec FOLD_CNT, FOLD_OFF;
range3 *GB = NULL; size_t GB_N = 0, GB_CAP = 0;
range3 *EP = NULL; size_t EP_N = 0, EP_CAP = 0;

int comp_push(unsigned long a, unsigned long b, unsigned long v) {
  comp_triplet *np;
  size_t nc;
  if (COMP_N == COMP_CAP) {
    nc = COMP_CAP ? COMP_CAP*2u : 64u;
    np = (comp_triplet*)realloc(COMP, nc * sizeof(comp_triplet));
    if (!np) return -1;
    COMP = np; COMP_CAP = nc;
  }
  COMP[COMP_N].a = a; COMP[COMP_N].b = b; COMP[COMP_N].v = v; COMP_N++;
  return 0;
}
int range_push(range3 **arr, size_t *n, size_t *cap, unsigned long a, unsigned long b, unsigned long p) {
  range3 *np;
  size_t nc;
  if (*n == *cap) {
    nc = *cap ? (*cap)*2u : 64u;
    np = (range3*)realloc(*arr, nc * sizeof(range3));
    if (!np) return -1;
    *arr = np; *cap = nc;
  }
  (*arr)[*n].start = a; (*arr)[*n].end = b; (*arr)[*n].prop = p; (*n)++;
  return 0;
}

int cmp_ul(const void *a, const void *b) {
  unsigned long aa = *(const unsigned long*)a;
  unsigned long bb = *(const unsigned long*)b;
  if (aa < bb) {return -1; }if (aa > bb) {return 1;} return 0;
}
int cmp_triplet(const void *pa, const void *pb) {
  const comp_triplet *a = (const comp_triplet*)pa;
  const comp_triplet *b = (const comp_triplet*)pb;
  if (a->a < b->a) {return -1; }if (a->a > b->a){ return 1;}
  if (a->b < b->b) {return -1; }if (a->b > b->b){ return 1;}
  return 0;
}
int cmp_range3(const void *pa, const void *pb) {
  const range3 *a = (const range3*)pa;
  const range3 *b = (const range3*)pb;
  if (a->start < b->start) {return -1;} if (a->start > b->start){ return 1;}
  if (a->end < b->end) {return -1;} if (a->end > b->end){ return 1;}
  if (a->prop < b->prop) {return -1;} if (a->prop > b->prop) {return 1;}
  return 0;
}

int load_UnicodeData(const char *path) {
  FILE *f = fopen(path, "rb");
  char line[4096];
  if (!f){ return -1;}

  while (fgets(line, sizeof(line), f)) {
    char *p = line, *fields[15];
    int i;
    for (i=0;i<15;++i) {
      char *semi = strchr(p,';');
      fields[i]=p;
      if (!semi) { if (i<14) fields[i+1]=NULL; break; }
      *semi=0; p=semi+1;
    }
    if (!fields[0]) continue;

    {
      unsigned long cp;
      if (hex_to_u32(fields[0], &cp)!=0) continue;

      if (fields[3] && *fields[3]) {
        const char *q = fields[3];
        unsigned long ccc = 0;
        while (*q>='0' && *q<='9') { ccc = ccc*10 + (unsigned long)(*q - '0'); ++q; }
        if (ulvec_push(&CCC_KEYS, cp)!=0) { fclose(f); return -1; }
        if (ulvec_push(&CCC_VALS, ccc & 0xFFul)!=0) { fclose(f); return -1; }
      }

      if (fields[12] && *fields[12]) {
        unsigned long up;
        if (hex_to_u32(fields[12], &up)==0) {
          if (ulvec_push(&TOUPPER_KEYS, cp)!=0) { fclose(f); return -1; }
          if (ulvec_push(&TOUPPER_VALS, up)!=0) { fclose(f); return -1; }
        }
      }
      if (fields[13] && *fields[13]) {
        unsigned long lo;
        if (hex_to_u32(fields[13], &lo)==0) {
          if (ulvec_push(&TOLOWER_KEYS, cp)!=0) { fclose(f); return -1; }
          if (ulvec_push(&TOLOWER_VALS, lo)!=0) { fclose(f); return -1; }
        }
      }

      if (fields[5] && *fields[5]) {
        const char *q = fields[5];
        int compat = 0;
        ulvec tmp; tmp.a=NULL; tmp.n=0; tmp.cap=0;

        if (*q=='<') {
          compat=1;
          while (*q && *q!='>') ++q;
          if (*q=='>') ++q;
          while (*q==' ') ++q;
        }
        while (*q) {
          unsigned long u; const char *start=q; char hex[16]; size_t l;
          while (*q && *q!=' ') ++q;
          l = (size_t)(q - start); if (l >= sizeof(hex)) l = sizeof(hex)-1;
          memcpy(hex, start, l); hex[l]='\0';
          if (hex_to_u32(hex, &u)!=0) { free(tmp.a); tmp.n=0; break; }
          if (ulvec_push(&tmp, u)!=0) { free(tmp.a); tmp.n=0; break; }
          while (*q==' ') ++q;
        }
        if (tmp.n > 0) {
          if (ulvec_push(&DECOMP_KEYS, cp)!=0) { free(tmp.a); fclose(f); return -1; }
          if (uivec_push(&DECOMP_CNT, (unsigned int)tmp.n)!=0) { free(tmp.a); fclose(f); return -1; }
          if (uivec_push(&DECOMP_ISCOMPAT, (unsigned int)(compat?1u:0u))!=0) { free(tmp.a); fclose(f); return -1; }
          if (uivec_push(&DECOMP_OFF, (unsigned int)DECOMP_POOL.n)!=0) { free(tmp.a); fclose(f); return -1; }
          for (i=0;i<(int)tmp.n;++i) if (ulvec_push(&DECOMP_POOL, tmp.a[i])!=0) { free(tmp.a); fclose(f); return -1; }
        }
        if (tmp.a) free(tmp.a);
      }
    }
  }
  fclose(f);
  return 0;
}

int load_CompositionExclusions(const char *path, ulvec *excl) {
  FILE *f = fopen(path, "rb");
  char line[1024];
  if (!f) return -1;
  while (fgets(line,sizeof(line),f)) {
    char *p=line, *hash, *q;
    unsigned long cp;
    if (*p=='#' || *p=='\n' || *p=='\0') continue;
    hash = strchr(p,'#'); if (hash) *hash=0;
    q = p; while (*q && *q!=' ' && *q!='\t' && *q!='\n') ++q; *q=0;
    if (!*p) continue;
    if (hex_to_u32(p,&cp)==0) { if (ulvec_push(excl, cp)!=0) { fclose(f); return -1; } }
  }
  fclose(f);
  return 0;
}
int ul_contains(const ulvec *v, unsigned long x) {
  size_t i;
  for (i=0;i<v->n;++i) if (v->a[i]==x) return 1;
  return 0;
}

int build_comp_pairs(const ulvec *keys, const uivec *cnt, const uivec *off,
                     const ulvec *pool, const uivec *iscompat, const ulvec *excl) {
  size_t i;
  for (i=0;i<keys->n;++i) {
    unsigned long cp = keys->a[i];
    unsigned int c = cnt->a[i];
    unsigned int o = off->a[i];
    unsigned int kcompat = iscompat->a[i];
    if (kcompat) continue;
    if (c == 2u) {
      unsigned long a = pool->a[o+0];
      unsigned long b = pool->a[o+1];
      if (!ul_contains(excl, cp)) {
        if (comp_push(a,b,cp)!=0) return -1;
      }
    }
  }
  return 0;
}

int load_CaseFolding(const char *path) {
  FILE *f = fopen(path, "rb");
  char line[4096];
  if (!f) return -1;
  while (fgets(line, sizeof(line), f)) {
    char *p=line; unsigned long cp; char *semi;
    if (*p=='#' || *p=='\n' || *p=='\0') continue;
    semi = strchr(p,';'); if (!semi) continue; *semi=0;
    if (hex_to_u32(p,&cp)!=0) continue;

    p = semi+1; while (*p==' ') ++p;
    if (!*p) continue;

    if (*p!='C' && *p!='S' && *p!='F') continue;
    {
      char status = *p;
      ulvec map; map.a=NULL; map.n=0; map.cap=0;

      p = strchr(p,';'); if(!p) continue; ++p; while(*p==' ') ++p;
      while (*p && *p!='#' && *p!='\n') {
        char hex[16]; char *q = p; size_t l;
        unsigned long u;
        while (*q && *q!=' ' && *q!='\n' && *q!='#') ++q;
        l = (size_t)(q - p); if (l >= sizeof(hex)) l = sizeof(hex)-1;
        memcpy(hex, p, l); hex[l]='\0';
        if (hex_to_u32(hex, &u)==0) {
          if (ulvec_push(&map, u)!=0) { free(map.a); fclose(f); return -1; }
        }
        p = q;
        while (*p==' ') ++p;
      }
      if (map.n > 0) {
        if (status!='F' && map.n>1) map.n=1;
        if (ulvec_push(&FOLD_KEYS, cp)!=0) { free(map.a); fclose(f); return -1; }
        if (uivec_push(&FOLD_CNT, (unsigned int)map.n)!=0) { free(map.a); fclose(f); return -1; }
        if (uivec_push(&FOLD_OFF, (unsigned int)FOLD_POOL.n)!=0) { free(map.a); fclose(f); return -1; }
        {
          size_t i2;
          for (i2=0;i2<map.n;++i2) if (ulvec_push(&FOLD_POOL, map.a[i2])!=0) { free(map.a); fclose(f); return -1; }
        }
      }
      if (map.a) free(map.a);
    }
  }
  fclose(f);
  return 0;
}

int load_GB(const char *path) {
  FILE *f = fopen(path, "rb");
  char line[1024];
  if (!f) return -1;

  while (fgets(line,sizeof(line),f)) {
    char *p=line, *semi, *hash, *dots, *end, *prop;
    unsigned long a, b;
    if (*p=='#' || *p=='\n' || *p=='\0') continue;

    semi = strchr(p,';'); if (!semi) continue; *semi=0;
    hash = strchr(semi+1,'#'); if (hash) *hash=0;

    while (*p==' '||*p=='\t') ++p;
    dots = strstr(p,"..");
    if (dots) {
      *dots=0;
      if (hex_to_u32(p,&a)!=0) continue;
      if (hex_to_u32(dots+2,&b)!=0) continue;
    } else {
      if (hex_to_u32(p,&a)!=0) continue;
      b = a;
    }

    prop = semi+1; while (*prop==' '||*prop=='\t') ++prop;
    end = prop; while (*end && *end!=' '&&*end!='\t'&&*end!='\n') ++end; *end=0;

    {
      unsigned long val = 0;
      if (strcmp(prop,"CR")==0) val = 1;
      else if (strcmp(prop,"LF")==0) val = 2;
      else if (strcmp(prop,"Control")==0) val = 3;
      else if (strcmp(prop,"Extend")==0) val = 4;
      else if (strcmp(prop,"ZWJ")==0) val = 5;
      else if (strcmp(prop,"SpacingMark")==0) val = 6;
      else if (strcmp(prop,"Prepend")==0) val = 7;
      else if (strcmp(prop,"Regional_Indicator")==0) val = 8;
      else if (strcmp(prop,"L")==0) val = 9;
      else if (strcmp(prop,"V")==0) val = 10;
      else if (strcmp(prop,"T")==0) val = 11;
      else if (strcmp(prop,"LV")==0) val = 12;
      else if (strcmp(prop,"LVT")==0) val = 13;
      if (range_push(&GB, &GB_N, &GB_CAP, a, b, val)!=0) { fclose(f); return -1; }
    }
  }
  fclose(f);
  return 0;
}

int load_EP(const char *path) {
  FILE *f = fopen(path, "rb");
  char line[1024];
  if (!f) return -1;

  while (fgets(line, sizeof(line), f)) {
    char *p = line, *semi, *hash, *dots, *end, *prop;
    unsigned long a, b;

    if (*p=='#' || *p=='\n' || *p=='\0') continue;
    hash = strchr(p,'#'); if (hash) *hash = 0;
    semi = strchr(p,';'); if (!semi) continue; *semi = 0;

    while (*p==' '||*p=='\t') ++p;
    dots = strstr(p, "..");
    if (dots) {
      *dots = 0;
      if (hex_to_u32(p, &a)!=0) continue;
      if (hex_to_u32(dots+2, &b)!=0) continue;
    } else {
      if (hex_to_u32(p, &a)!=0) continue;
      b = a;
    }

    prop = semi+1; while (*prop==' '||*prop=='\t') ++prop;
    end = prop; while (*end && *end!=' '&&*end!='\t'&&*end!='\n') ++end; *end = 0;

    if (strcmp(prop, "Extended_Pictographic")==0) {
      if (range_push(&EP, &EP_N, &EP_CAP, a, b, 1u)!=0) { fclose(f); return -1; }
    }
  }
  fclose(f);
  return 0;
}

int sort_and_dedupe_maps(void) {
  size_t i, w;

  if (CCC_KEYS.n != CCC_VALS.n) return -1;
  for (i=1;i<CCC_KEYS.n;++i) {
    unsigned long k = CCC_KEYS.a[i], v = CCC_VALS.a[i];
    size_t j = i;
    while (j>0 && CCC_KEYS.a[j-1] > k) {
      CCC_KEYS.a[j] = CCC_KEYS.a[j-1];
      CCC_VALS.a[j] = CCC_VALS.a[j-1];
      --j;
    }
    CCC_KEYS.a[j] = k; CCC_VALS.a[j] = v;
  }
  w = 0;
  for (i=0;i<CCC_KEYS.n;++i) {
    if (w>0 && CCC_KEYS.a[w-1]==CCC_KEYS.a[i]) { CCC_VALS.a[w-1] = CCC_VALS.a[i]; }
    else { CCC_KEYS.a[w] = CCC_KEYS.a[i]; CCC_VALS.a[w] = CCC_VALS.a[i]; ++w; }
  }
  CCC_KEYS.n = CCC_VALS.n = w;

  for (i=1;i<TOLOWER_KEYS.n;++i) {
    unsigned long k = TOLOWER_KEYS.a[i], v = TOLOWER_VALS.a[i];
    size_t j = i;
    while (j>0 && TOLOWER_KEYS.a[j-1] > k) {
      TOLOWER_KEYS.a[j] = TOLOWER_KEYS.a[j-1];
      TOLOWER_VALS.a[j] = TOLOWER_VALS.a[j-1];
      --j;
    }
    TOLOWER_KEYS.a[j] = k; TOLOWER_VALS.a[j] = v;
  }
  w = 0;
  for (i=0;i<TOLOWER_KEYS.n;++i) {
    if (w>0 && TOLOWER_KEYS.a[w-1]==TOLOWER_KEYS.a[i]) { TOLOWER_VALS.a[w-1] = TOLOWER_VALS.a[i]; }
    else { TOLOWER_KEYS.a[w] = TOLOWER_KEYS.a[i]; TOLOWER_VALS.a[w] = TOLOWER_VALS.a[i]; ++w; }
  }
  TOLOWER_KEYS.n = TOLOWER_VALS.n = w;

  for (i=1;i<TOUPPER_KEYS.n;++i) {
    unsigned long k = TOUPPER_KEYS.a[i], v = TOUPPER_VALS.a[i];
    size_t j = i;
    while (j>0 && TOUPPER_KEYS.a[j-1] > k) {
      TOUPPER_KEYS.a[j] = TOUPPER_KEYS.a[j-1];
      TOUPPER_VALS.a[j] = TOUPPER_VALS.a[j-1];
      --j;
    }
    TOUPPER_KEYS.a[j] = k; TOUPPER_VALS.a[j] = v;
  }
  w = 0;
  for (i=0;i<TOUPPER_KEYS.n;++i) {
    if (w>0 && TOUPPER_KEYS.a[w-1]==TOUPPER_KEYS.a[i]) { TOUPPER_VALS.a[w-1] = TOUPPER_VALS.a[i]; }
    else { TOUPPER_KEYS.a[w] = TOUPPER_KEYS.a[i]; TOUPPER_VALS.a[w] = TOUPPER_VALS.a[i]; ++w; }
  }
  TOUPPER_KEYS.n = TOUPPER_VALS.n = w;

  for (i=1;i<FOLD_KEYS.n;++i) {
    unsigned long k = FOLD_KEYS.a[i];
    unsigned int cnt = FOLD_CNT.a[i], off = FOLD_OFF.a[i];
    size_t j = i;
    while (j>0 && FOLD_KEYS.a[j-1] > k) {
      FOLD_KEYS.a[j] = FOLD_KEYS.a[j-1];
      FOLD_CNT.a[j] = FOLD_CNT.a[j-1];
      FOLD_OFF.a[j] = FOLD_OFF.a[j-1];
      --j;
    }
    FOLD_KEYS.a[j] = k; FOLD_CNT.a[j] = cnt; FOLD_OFF.a[j] = off;
  }

  for (i=1;i<DECOMP_KEYS.n;++i) {
    unsigned long k = DECOMP_KEYS.a[i];
    unsigned int cnt = DECOMP_CNT.a[i], off = DECOMP_OFF.a[i], ic = DECOMP_ISCOMPAT.a[i];
    size_t j = i;
    while (j>0 && DECOMP_KEYS.a[j-1] > k) {
      DECOMP_KEYS.a[j] = DECOMP_KEYS.a[j-1];
      DECOMP_CNT.a[j] = DECOMP_CNT.a[j-1];
      DECOMP_OFF.a[j] = DECOMP_OFF.a[j-1];
      DECOMP_ISCOMPAT.a[j] = DECOMP_ISCOMPAT.a[j-1];
      --j;
    }
    DECOMP_KEYS.a[j] = k; DECOMP_CNT.a[j] = cnt; DECOMP_OFF.a[j] = off; DECOMP_ISCOMPAT.a[j] = ic;
  }

  qsort(COMP, COMP_N, sizeof(comp_triplet), cmp_triplet);
  qsort(GB, GB_N, sizeof(range3), cmp_range3);
  qsort(EP, EP_N, sizeof(range3), cmp_range3);
  return 0;
}

int write_header(const char *out_h) {
  FILE *h = fopen(out_h, "wb");
  if (!h) return -1;
  fprintf(h,
    "#ifndef DS_UNICODE_TABLES_H\n"
    "#define DS_UNICODE_TABLES_H\n"
    "#include <stddef.h>\n\n"
    "typedef struct { unsigned long start, end, prop; } ucd_range_t;\n"
    "extern const unsigned long U_CCC_KEYS[];\n"
    "extern const unsigned long U_CCC_VALS[];\n"
    "extern const size_t U_CCC_LEN;\n\n"
    "extern const unsigned long U_DECOMP_KEYS[];\n"
    "extern const unsigned int  U_DECOMP_CNT[];\n"
    "extern const unsigned int  U_DECOMP_OFF[];\n"
    "extern const unsigned int  U_DECOMP_ISCOMPAT[];\n"
    "extern const unsigned long U_DECOMP_POOL[];\n"
    "extern const size_t        U_DECOMP_LEN;\n"
    "extern const size_t        U_DECOMP_POOL_LEN;\n\n"
    "extern const unsigned long U_COMP_KEYS_A[];\n"
    "extern const unsigned long U_COMP_KEYS_B[];\n"
    "extern const unsigned long U_COMP_VALS[];\n"
    "extern const size_t        U_COMP_LEN;\n\n"
    "extern const unsigned long U_TOLOWER_KEYS[];\n"
    "extern const unsigned long U_TOLOWER_VALS[];\n"
    "extern const size_t        U_TOLOWER_LEN;\n"
    "extern const unsigned long U_TOUPPER_KEYS[];\n"
    "extern const unsigned long U_TOUPPER_VALS[];\n"
    "extern const size_t        U_TOUPPER_LEN;\n\n"
    "extern const unsigned long U_FOLD_KEYS[];\n"
    "extern const unsigned int  U_FOLD_CNT[];\n"
    "extern const unsigned int  U_FOLD_OFF[];\n"
    "extern const unsigned long U_FOLD_POOL[];\n"
    "extern const size_t        U_FOLD_LEN;\n"
    "extern const size_t        U_FOLD_POOL_LEN;\n\n"
    "extern const ucd_range_t   U_GB_RANGES[];\n"
    "extern const size_t        U_GB_LEN;\n"
    "extern const ucd_range_t   U_EP_RANGES[];\n"
    "extern const size_t        U_EP_LEN;\n\n"
    "#endif/* DS_UNICODE_TABLES_H */\n");
  fclose(h);
  return 0;
}

int write_source(const char *out_c) {
  FILE *c = fopen(out_c, "wb");
  size_t i;
  if (!c) return -1;

  fprintf(c, "#include <stddef.h>\n#include \"unicode_tables.h\"\n\n");

  fprintf(c, "const unsigned long U_CCC_KEYS[] = {");
  for (i=0;i<CCC_KEYS.n;++i) { if (i) fprintf(c,","); fprintf(c,"0x%lXul",(unsigned long)CCC_KEYS.a[i]); }
  fprintf(c, "};\n");
  fprintf(c, "const unsigned long U_CCC_VALS[] = {");
  for (i=0;i<CCC_VALS.n;++i) { if (i) fprintf(c,","); fprintf(c,"0x%lXul",(unsigned long)CCC_VALS.a[i]); }
  fprintf(c, "};\n");
  fprintf(c, "const size_t U_CCC_LEN = %luu;\n\n", (unsigned long)CCC_KEYS.n);

  fprintf(c, "const unsigned long U_DECOMP_KEYS[] = {");
  for (i=0;i<DECOMP_KEYS.n;++i) { if (i) fprintf(c,","); fprintf(c,"0x%lXul", DECOMP_KEYS.a[i]); }
  fprintf(c, "};\n");
  fprintf(c, "const unsigned int U_DECOMP_CNT[] = {");
  for (i=0;i<DECOMP_CNT.n;++i) { if (i) fprintf(c,","); fprintf(c,"%u", DECOMP_CNT.a[i]); }
  fprintf(c, "};\n");
  fprintf(c, "const unsigned int U_DECOMP_OFF[] = {");
  for (i=0;i<DECOMP_OFF.n;++i) { if (i) fprintf(c,","); fprintf(c,"%u", DECOMP_OFF.a[i]); }
  fprintf(c, "};\n");
  fprintf(c, "const unsigned int U_DECOMP_ISCOMPAT[] = {");
  for (i=0;i<DECOMP_ISCOMPAT.n;++i) { if (i) fprintf(c,","); fprintf(c,"%u", DECOMP_ISCOMPAT.a[i]); }
  fprintf(c, "};\n");
  fprintf(c, "const unsigned long U_DECOMP_POOL[] = {");
  for (i=0;i<DECOMP_POOL.n;++i) { if (i) fprintf(c,","); fprintf(c,"0x%lXul", DECOMP_POOL.a[i]); }
  fprintf(c, "};\n");
  fprintf(c, "const size_t U_DECOMP_LEN = %luu;\n", (unsigned long)DECOMP_KEYS.n);
  fprintf(c, "const size_t U_DECOMP_POOL_LEN = %luu;\n\n", (unsigned long)DECOMP_POOL.n);

  fprintf(c, "const unsigned long U_COMP_KEYS_A[] = {");
  for (i=0;i<COMP_N;++i) { if (i) fprintf(c,","); fprintf(c,"0x%lXul", COMP[i].a); }
  fprintf(c, "};\n");
  fprintf(c, "const unsigned long U_COMP_KEYS_B[] = {");
  for (i=0;i<COMP_N;++i) { if (i) fprintf(c,","); fprintf(c,"0x%lXul", COMP[i].b); }
  fprintf(c, "};\n");
  fprintf(c, "const unsigned long U_COMP_VALS[] = {");
  for (i=0;i<COMP_N;++i) { if (i) fprintf(c,","); fprintf(c,"0x%lXul", COMP[i].v); }
  fprintf(c, "};\n");
  fprintf(c, "const size_t U_COMP_LEN = %luu;\n\n", (unsigned long)COMP_N);

  fprintf(c, "const unsigned long U_TOLOWER_KEYS[] = {");
  for (i=0;i<TOLOWER_KEYS.n;++i) { if (i) fprintf(c,","); fprintf(c,"0x%lXul", TOLOWER_KEYS.a[i]); }
  fprintf(c, "};\n");
  fprintf(c, "const unsigned long U_TOLOWER_VALS[] = {");
  for (i=0;i<TOLOWER_VALS.n;++i) { if (i) fprintf(c,","); fprintf(c,"0x%lXul", TOLOWER_VALS.a[i]); }
  fprintf(c, "};\n");
  fprintf(c, "const size_t U_TOLOWER_LEN = %luu;\n\n", (unsigned long)TOLOWER_KEYS.n);

  fprintf(c, "const unsigned long U_TOUPPER_KEYS[] = {");
  for (i=0;i<TOUPPER_KEYS.n;++i) { if (i) fprintf(c,","); fprintf(c,"0x%lXul", TOUPPER_KEYS.a[i]); }
  fprintf(c, "};\n");
  fprintf(c, "const unsigned long U_TOUPPER_VALS[] = {");
  for (i=0;i<TOUPPER_VALS.n;++i) { if (i) fprintf(c,","); fprintf(c,"0x%lXul", TOUPPER_VALS.a[i]); }
  fprintf(c, "};\n");
  fprintf(c, "const size_t U_TOUPPER_LEN = %luu;\n\n", (unsigned long)TOUPPER_KEYS.n);

  fprintf(c, "const unsigned long U_FOLD_KEYS[] = {");
  for (i=0;i<FOLD_KEYS.n;++i) { if (i) fprintf(c,","); fprintf(c,"0x%lXul", FOLD_KEYS.a[i]); }
  fprintf(c, "};\n");
  fprintf(c, "const unsigned int U_FOLD_CNT[] = {");
  for (i=0;i<FOLD_CNT.n;++i) { if (i) fprintf(c,","); fprintf(c,"%u", FOLD_CNT.a[i]); }
  fprintf(c, "};\n");
  fprintf(c, "const unsigned int U_FOLD_OFF[] = {");
  for (i=0;i<FOLD_OFF.n;++i) { if (i) fprintf(c,","); fprintf(c,"%u", FOLD_OFF.a[i]); }
  fprintf(c, "};\n");
  fprintf(c, "const unsigned long U_FOLD_POOL[] = {");
  for (i=0;i<FOLD_POOL.n;++i) { if (i) fprintf(c,","); fprintf(c,"0x%lXul", FOLD_POOL.a[i]); }
  fprintf(c, "};\n");
  fprintf(c, "const size_t U_FOLD_LEN = %luu;\n", (unsigned long)FOLD_KEYS.n);
  fprintf(c, "const size_t U_FOLD_POOL_LEN = %luu;\n\n", (unsigned long)FOLD_POOL.n);

  fprintf(c, "const ucd_range_t U_GB_RANGES[] = {");
  for (i=0;i<GB_N;++i) {
    if (i) fprintf(c,",");
    fprintf(c,"{0x%lXul,0x%lXul,%luul}", GB[i].start, GB[i].end, (unsigned long)GB[i].prop);
  }
  fprintf(c, "};\n");
  fprintf(c, "const size_t U_GB_LEN = %luu;\n\n", (unsigned long)GB_N);

  fprintf(c, "const ucd_range_t U_EP_RANGES[] = {");
  for (i=0;i<EP_N;++i) {
    if (i) fprintf(c,",");
    fprintf(c,"{0x%lXul,0x%lXul,%luul}", EP[i].start, EP[i].end, (unsigned long)EP[i].prop);
  }
  fprintf(c, "};\n");
  fprintf(c, "const size_t U_EP_LEN = %luu;\n", (unsigned long)EP_N);

  fclose(c);
  return 0;
}

int main(int argc, char **argv) {
  const char *path_unicode = NULL;
  const char *path_casefold = NULL;
  const char *path_compexcl = NULL;
  const char *path_gb = NULL;
  const char *path_emoji = NULL;
  const char *out_c = NULL;
  const char *out_h = NULL;
  ulvec excl; excl.a=NULL; excl.n=0; excl.cap=0;

  int i;

  for (i=1;i<argc;i++) {
    if (!strcmp(argv[i], "--unicode-data") && i+1<argc) path_unicode = argv[++i];
    else if (!strcmp(argv[i], "--case-folding") && i+1<argc) path_casefold = argv[++i];
    else if (!strcmp(argv[i], "--comp-excl") && i+1<argc) path_compexcl = argv[++i];
    else if (!strcmp(argv[i], "--gb-prop") && i+1<argc) path_gb = argv[++i];
    else if (!strcmp(argv[i], "--emoji-data") && i+1<argc) path_emoji = argv[++i];
    else if (!strcmp(argv[i], "--out-c") && i+1<argc) out_c = argv[++i];
    else if (!strcmp(argv[i], "--out-h") && i+1<argc) out_h = argv[++i];
  }

  if (!path_unicode || !path_casefold || !path_compexcl || !path_gb || !path_emoji || !out_c || !out_h) {
    fprintf(stderr, "usage: gen_ucd --unicode-data U --case-folding C --comp-excl X --gb-prop G --emoji-data E --out-c out.c --out-h out.h\n");
    return 1;
  }

  if (load_UnicodeData(path_unicode)!=0) { fprintf(stderr,"UnicodeData parse failed\n"); return 1; }
  if (load_CaseFolding(path_casefold)!=0) { fprintf(stderr,"CaseFolding parse failed\n"); return 1; }
  if (load_CompositionExclusions(path_compexcl, &excl)!=0) { fprintf(stderr,"CompExcl parse failed\n"); return 1; }
  if (build_comp_pairs(&DECOMP_KEYS, &DECOMP_CNT, &DECOMP_OFF, &DECOMP_POOL, &DECOMP_ISCOMPAT, &excl)!=0) { fprintf(stderr,"Comp pairs build failed\n"); return 1; }
  if (load_GB(path_gb)!=0) { fprintf(stderr,"GB parse failed\n"); return 1; }
  if (load_EP(path_emoji)!=0) { fprintf(stderr,"EP parse failed\n"); return 1; }
  if (sort_and_dedupe_maps()!=0) { fprintf(stderr,"sort/dedupe failed\n"); return 1; }

  if (write_header(out_h)!=0) { fprintf(stderr,"write header failed\n"); return 1; }
  if (write_source(out_c)!=0) { fprintf(stderr,"write source failed\n"); return 1; }

  return 0;
}
