#include "context.h"
#include <stdlib.h>
#include <string.h>

static void *ctx_default_alloc(size_t size, void *user) {
  (void)user;
  return malloc(size);
}
static void *ctx_default_calloc(size_t count, size_t size, void *user) {
  (void)user;
  return calloc(count, size);
}
static void *ctx_default_realloc(void *ptr, size_t size, void *user) {
  (void)user;
  return realloc(ptr, size);
}
static void ctx_default_free(void *ptr, void *user) {
  (void)user;
  free(ptr);
}

void ds_context_init(ds_context_t *context) {
  if (context == NULL) {
    return;
  }
  context->alloc = ctx_default_alloc;
  context->calloc = ctx_default_calloc;
  context->realloc = ctx_default_realloc;
  context->free = ctx_default_free;
  context->user = NULL;
  context->diagnostic_sink = NULL;
  ds_error_clear(&context->last_error);
  context->flags = 0U;
}

ds_context_t *ds_default_context(void) {
  static ds_context_t context = {
      ctx_default_alloc,
      ctx_default_calloc,
      ctx_default_realloc,
      ctx_default_free,
      NULL,
      NULL,
      {DS_OK, NULL, NULL, NULL, NULL, 0UL, NULL},
      0U};
  return &context;
}

void ds_context_set_diagnostic_sink(ds_context_t *context,
                                    ds_diagnostic_sink_t *sink) {
  if (context == NULL) {
    context = ds_default_context();
  }
  context->diagnostic_sink = sink;
}

void *ds_context_alloc(ds_context_t *context, size_t size) {
  if (context == NULL) {
    context = ds_default_context();
  }
  return context->alloc(size, context->user);
}

void *ds_context_calloc(ds_context_t *context, size_t count, size_t size) {
  if (context == NULL) {
    context = ds_default_context();
  }
  return context->calloc(count, size, context->user);
}

void *ds_context_realloc(ds_context_t *context, void *ptr, size_t size) {
  if (context == NULL) {
    context = ds_default_context();
  }
  return context->realloc(ptr, size, context->user);
}

void ds_context_free(ds_context_t *context, void *ptr) {
  if (ptr == NULL) {
    return;
  }
  if (context == NULL) {
    context = ds_default_context();
  }
  context->free(ptr, context->user);
}

void ds_context_clear_error(ds_context_t *context) {
  if (context == NULL) {
    context = ds_default_context();
  }
  ds_error_clear(&context->last_error);
}

const ds_error_t *ds_context_last_error(const ds_context_t *context) {
  if (context == NULL) {
    context = ds_default_context();
  }
  return &context->last_error;
}

ds_status_t ds_context_set_error(ds_context_t *context, ds_status_t status,
                                 const char *id, const char *module,
                                 const char *message, const char *file,
                                 unsigned long line, const char *function) {
  if (context == NULL) {
    context = ds_default_context();
  }
  return ds_error_set(&context->last_error, status, id, module, message, file,
                      line, function);
}

ds_status_t ds_context_set_error_desc(ds_context_t *context,
                                      const ds_error_desc_t *desc,
                                      const char *file, unsigned long line,
                                      const char *function) {
  if (context == NULL) {
    context = ds_default_context();
  }
  return ds_error_set_desc(&context->last_error, desc, file, line, function);
}

void ds_context_emit_diagnostic(ds_context_t *context,
                                const ds_diagnostic_desc_t *desc,
                                const ds_diagnostic_arg_t *args,
                                size_t arg_count, const char *file,
                                unsigned long line, const char *function) {
  if (context == NULL) {
    context = ds_default_context();
  }
  ds_diagnostic_emit_desc(context->diagnostic_sink, desc, args, arg_count, file,
                          line, function);
}
