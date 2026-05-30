#ifndef DS_GRAPH_H
#define DS_GRAPH_H

#include "common.h"

#include "context.h"
#include "status.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ds_graph_edge {
  size_t to;
  double weight;
  void *data;
} ds_graph_edge_t;

typedef struct ds_graph_vertex {
  void *data;
  ds_graph_edge_t *edges;
  size_t edge_count;
  size_t edge_capacity;
} ds_graph_vertex_t;

typedef bool (*ds_graph_visit_func_t)(size_t vertex, void *data, void *user);
typedef bool (*ds_graph_edge_visit_func_t)(size_t from,
                                           const ds_graph_edge_t *edge,
                                           void *user);

typedef struct ds_graph {
  ds_graph_vertex_t *vertices;
  size_t vertex_count;
  size_t vertex_capacity;
  ds_context_t *context;
} ds_graph_t;

ds_graph_t *FUNC(ds_graph_create)(void);
ds_graph_t *FUNC(ds_graph_create_ctx)(ds_context_t *context);
void FUNC(ds_graph_destroy)(ds_graph_t *graph,
                            void (*destroy_vertex)(void *),
                            void (*destroy_edge)(void *));
ds_status_t FUNC(ds_graph_add_vertex)(ds_graph_t *graph, void *data,
                                       size_t *out_index);
ds_status_t FUNC(ds_graph_add_edge)(ds_graph_t *graph, size_t from, size_t to,
                                     void *data);
ds_status_t FUNC(ds_graph_add_weighted_edge)(ds_graph_t *graph, size_t from,
                                              size_t to, double weight,
                                              void *data);
ds_status_t FUNC(ds_graph_get_vertex)(ds_graph_t *graph, size_t index,
                                       void **out_data);
ds_status_t FUNC(ds_graph_update_edge)(ds_graph_t *graph, size_t from,
                                        size_t to, double weight, void *data,
                                        void (*destroy_old_edge)(void *));
ds_status_t FUNC(ds_graph_remove_edge)(ds_graph_t *graph, size_t from,
                                        size_t to,
                                        void (*destroy_edge)(void *));
ds_status_t FUNC(ds_graph_visit_edges)(ds_graph_t *graph, size_t from,
                                        ds_graph_edge_visit_func_t visit,
                                        void *user);
size_t FUNC(ds_graph_vertex_count)(ds_graph_t *graph);
size_t FUNC(ds_graph_edge_count)(ds_graph_t *graph, size_t vertex);
ds_status_t FUNC(ds_graph_bfs)(ds_graph_t *graph, size_t start,
                                ds_graph_visit_func_t visit, void *user);
ds_status_t FUNC(ds_graph_dfs)(ds_graph_t *graph, size_t start,
                                ds_graph_visit_func_t visit, void *user);
ds_status_t FUNC(ds_graph_topological_sort)(ds_graph_t *graph,
                                             size_t *out_order,
                                             size_t capacity,
                                             size_t *out_count);
ds_status_t FUNC(ds_graph_dijkstra)(ds_graph_t *graph, size_t start,
                                     double *out_distances,
                                     size_t *out_previous, size_t capacity);
ds_status_t FUNC(ds_graph_bellman_ford)(ds_graph_t *graph, size_t start,
                                          double *out_distances,
                                          size_t *out_previous, size_t capacity,
                                          bool *out_has_negative_cycle);
ds_status_t FUNC(ds_graph_minimum_spanning_tree)(ds_graph_t *graph,
                                                  ds_graph_t *out_tree);
ds_status_t FUNC(ds_graph_connected_components)(ds_graph_t *graph,
                                                 size_t *out_components,
                                                 size_t capacity,
                                                 size_t *out_count);
ds_status_t FUNC(ds_graph_strongly_connected_components)(ds_graph_t *graph,
                                                          size_t *out_components,
                                                          size_t capacity,
                                                          size_t *out_count);

#ifdef __cplusplus
}
#endif

#endif /* DS_GRAPH_H */
