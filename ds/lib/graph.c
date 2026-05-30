#include "graph.h"
#include <float.h>
#include <stdlib.h>
#include <string.h>

static ds_status_t graph_reserve_vertices(ds_graph_t *graph, size_t capacity);
static ds_status_t graph_reserve_edges(ds_graph_t *graph, size_t vertex,
                                       size_t capacity);

static ds_status_t graph_reserve_vertices(ds_graph_t *graph, size_t capacity) {
  ds_graph_vertex_t *vertices;
  size_t i;
  if (capacity <= graph->vertex_capacity) {
    return DS_OK;
  }
  vertices = (ds_graph_vertex_t *)ds_context_realloc(
      graph->context, graph->vertices, capacity * sizeof(*vertices));
  if (vertices == NULL) {
    return DS_ERR_ALLOC;
  }
  for (i = graph->vertex_capacity; i < capacity; i++) {
    vertices[i].data = NULL;
    vertices[i].edges = NULL;
    vertices[i].edge_count = 0U;
    vertices[i].edge_capacity = 0U;
  }
  graph->vertices = vertices;
  graph->vertex_capacity = capacity;
  return DS_OK;
}

static ds_status_t graph_reserve_edges(ds_graph_t *graph, size_t vertex,
                                       size_t capacity) {
  ds_graph_edge_t *edges;
  if (capacity <= graph->vertices[vertex].edge_capacity) {
    return DS_OK;
  }
  edges = (ds_graph_edge_t *)ds_context_realloc(
      graph->context, graph->vertices[vertex].edges, capacity * sizeof(*edges));
  if (edges == NULL) {
    return DS_ERR_ALLOC;
  }
  graph->vertices[vertex].edges = edges;
  graph->vertices[vertex].edge_capacity = capacity;
  return DS_OK;
}

ds_graph_t *FUNC(ds_graph_create)(void) { return FUNC(ds_graph_create_ctx)(NULL); }

ds_graph_t *FUNC(ds_graph_create_ctx)(ds_context_t *context) {
  ds_graph_t *graph;
  if (context == NULL) {
    context = ds_default_context();
  }
  graph = (ds_graph_t *)ds_context_alloc(context, sizeof(*graph));
  if (graph == NULL) {
    return NULL;
  }
  graph->vertices = NULL;
  graph->vertex_count = 0U;
  graph->vertex_capacity = 0U;
  graph->context = context;
  return graph;
}

void FUNC(ds_graph_destroy)(ds_graph_t *graph,
                            void (*destroy_vertex)(void *),
                            void (*destroy_edge)(void *)) {
  size_t i;
  size_t j;
  if (graph == NULL) {
    return;
  }
  for (i = 0U; i < graph->vertex_count; i++) {
    if (destroy_vertex != NULL) {
      destroy_vertex(graph->vertices[i].data);
    }
    if (destroy_edge != NULL) {
      for (j = 0U; j < graph->vertices[i].edge_count; j++) {
        destroy_edge(graph->vertices[i].edges[j].data);
      }
    }
    ds_context_free(graph->context, graph->vertices[i].edges);
  }
  ds_context_free(graph->context, graph->vertices);
  ds_context_free(graph->context, graph);
}

ds_status_t FUNC(ds_graph_add_vertex)(ds_graph_t *graph, void *data,
                                       size_t *out_index) {
  ds_status_t status;
  size_t index;
  if (graph == NULL) {
    return DS_ERR_NULL;
  }
  if (graph->vertex_count == graph->vertex_capacity) {
    status = graph_reserve_vertices(
        graph, graph->vertex_capacity == 0U ? 8U : graph->vertex_capacity * 2U);
    if (status != DS_OK) {
      return status;
    }
  }
  index = graph->vertex_count;
  graph->vertices[index].data = data;
  graph->vertices[index].edge_count = 0U;
  graph->vertex_count++;
  if (out_index != NULL) {
    *out_index = index;
  }
  return DS_OK;
}

ds_status_t FUNC(ds_graph_add_edge)(ds_graph_t *graph, size_t from, size_t to,
                                     void *data) {
  return FUNC(ds_graph_add_weighted_edge)(graph, from, to, 1.0, data);
}

ds_status_t FUNC(ds_graph_add_weighted_edge)(ds_graph_t *graph, size_t from,
                                              size_t to, double weight,
                                              void *data) {
  ds_status_t status;
  size_t index;
  if (graph == NULL) {
    return DS_ERR_NULL;
  }
  if (from >= graph->vertex_count || to >= graph->vertex_count) {
    return DS_ERR_RANGE;
  }
  if (graph->vertices[from].edge_count == graph->vertices[from].edge_capacity) {
    status = graph_reserve_edges(
        graph, from, graph->vertices[from].edge_capacity == 0U
                         ? 4U
                         : graph->vertices[from].edge_capacity * 2U);
    if (status != DS_OK) {
      return status;
    }
  }
  index = graph->vertices[from].edge_count;
  graph->vertices[from].edges[index].to = to;
  graph->vertices[from].edges[index].weight = weight;
  graph->vertices[from].edges[index].data = data;
  graph->vertices[from].edge_count++;
  return DS_OK;
}

ds_status_t FUNC(ds_graph_get_vertex)(ds_graph_t *graph, size_t index,
                                       void **out_data) {
  if (out_data != NULL) {
    *out_data = NULL;
  }
  if (graph == NULL || out_data == NULL) {
    return DS_ERR_NULL;
  }
  if (index >= graph->vertex_count) {
    return DS_ERR_RANGE;
  }
  *out_data = graph->vertices[index].data;
  return DS_OK;
}


ds_status_t FUNC(ds_graph_update_edge)(ds_graph_t *graph, size_t from,
                                        size_t to, double weight, void *data,
                                        void (*destroy_old_edge)(void *)) {
  size_t i;

  if (graph == NULL) {
    return DS_ERR_NULL;
  }
  if (from >= graph->vertex_count || to >= graph->vertex_count) {
    return DS_ERR_RANGE;
  }
  for (i = 0U; i < graph->vertices[from].edge_count; i++) {
    if (graph->vertices[from].edges[i].to == to) {
      if (destroy_old_edge != NULL) {
        destroy_old_edge(graph->vertices[from].edges[i].data);
      }
      graph->vertices[from].edges[i].weight = weight;
      graph->vertices[from].edges[i].data = data;
      return DS_OK;
    }
  }
  return FUNC(ds_graph_add_weighted_edge)(graph, from, to, weight, data);
}

ds_status_t FUNC(ds_graph_remove_edge)(ds_graph_t *graph, size_t from,
                                        size_t to,
                                        void (*destroy_edge)(void *)) {
  size_t i;
  size_t j;

  if (graph == NULL) {
    return DS_ERR_NULL;
  }
  if (from >= graph->vertex_count || to >= graph->vertex_count) {
    return DS_ERR_RANGE;
  }
  for (i = 0U; i < graph->vertices[from].edge_count; i++) {
    if (graph->vertices[from].edges[i].to == to) {
      if (destroy_edge != NULL) {
        destroy_edge(graph->vertices[from].edges[i].data);
      }
      for (j = i + 1U; j < graph->vertices[from].edge_count; j++) {
        graph->vertices[from].edges[j - 1U] = graph->vertices[from].edges[j];
      }
      graph->vertices[from].edge_count--;
      return DS_OK;
    }
  }
  return DS_NOT_FOUND;
}

ds_status_t FUNC(ds_graph_visit_edges)(ds_graph_t *graph, size_t from,
                                        ds_graph_edge_visit_func_t visit,
                                        void *user) {
  size_t i;

  if (graph == NULL || visit == NULL) {
    return DS_ERR_NULL;
  }
  if (from >= graph->vertex_count) {
    return DS_ERR_RANGE;
  }
  for (i = 0U; i < graph->vertices[from].edge_count; i++) {
    if (!visit(from, &graph->vertices[from].edges[i], user)) {
      return DS_STOP;
    }
  }
  return DS_OK;
}

size_t FUNC(ds_graph_vertex_count)(ds_graph_t *graph) {
  if (graph == NULL) {
    return 0U;
  }
  return graph->vertex_count;
}

size_t FUNC(ds_graph_edge_count)(ds_graph_t *graph, size_t vertex) {
  if (graph == NULL || vertex >= graph->vertex_count) {
    return 0U;
  }
  return graph->vertices[vertex].edge_count;
}

ds_status_t FUNC(ds_graph_bfs)(ds_graph_t *graph, size_t start,
                                ds_graph_visit_func_t visit, void *user) {
  bool *seen;
  size_t *queue;
  size_t head;
  size_t tail;
  size_t vertex;
  size_t i;
  size_t to;
  if (graph == NULL || visit == NULL) {
    return DS_ERR_NULL;
  }
  if (start >= graph->vertex_count) {
    return DS_ERR_RANGE;
  }
  seen = (bool *)calloc(graph->vertex_count, sizeof(*seen));
  if (seen == NULL) {
    return DS_ERR_ALLOC;
  }
  queue = (size_t *)malloc(graph->vertex_count * sizeof(*queue));
  if (queue == NULL) {
    free(seen);
    return DS_ERR_ALLOC;
  }
  head = 0U;
  tail = 0U;
  seen[start] = true;
  queue[tail] = start;
  tail++;
  while (head < tail) {
    vertex = queue[head];
    head++;
    if (!visit(vertex, graph->vertices[vertex].data, user)) {
      free(queue);
      free(seen);
      return DS_STOP;
    }
    for (i = 0U; i < graph->vertices[vertex].edge_count; i++) {
      to = graph->vertices[vertex].edges[i].to;
      if (!seen[to]) {
        seen[to] = true;
        queue[tail] = to;
        tail++;
      }
    }
  }
  free(queue);
  free(seen);
  return DS_OK;
}

static ds_status_t graph_dfs_visit(ds_graph_t *graph, size_t vertex,
                                   bool *seen, ds_graph_visit_func_t visit,
                                   void *user) {
  size_t i;
  size_t to;
  seen[vertex] = true;
  if (!visit(vertex, graph->vertices[vertex].data, user)) {
    return DS_STOP;
  }
  for (i = 0U; i < graph->vertices[vertex].edge_count; i++) {
    to = graph->vertices[vertex].edges[i].to;
    if (!seen[to]) {
      ds_status_t status;
      status = graph_dfs_visit(graph, to, seen, visit, user);
      if (status != DS_OK) {
        return status;
      }
    }
  }
  return DS_OK;
}

ds_status_t FUNC(ds_graph_dfs)(ds_graph_t *graph, size_t start,
                                ds_graph_visit_func_t visit, void *user) {
  bool *seen;
  ds_status_t status;
  if (graph == NULL || visit == NULL) {
    return DS_ERR_NULL;
  }
  if (start >= graph->vertex_count) {
    return DS_ERR_RANGE;
  }
  seen = (bool *)calloc(graph->vertex_count, sizeof(*seen));
  if (seen == NULL) {
    return DS_ERR_ALLOC;
  }
  status = graph_dfs_visit(graph, start, seen, visit, user);
  free(seen);
  return status;
}

ds_status_t FUNC(ds_graph_topological_sort)(ds_graph_t *graph,
                                             size_t *out_order,
                                             size_t capacity,
                                             size_t *out_count) {
  size_t *indegree;
  size_t *queue;
  size_t head;
  size_t tail;
  size_t produced;
  size_t i;
  size_t j;
  size_t to;
  if (out_count != NULL) {
    *out_count = 0U;
  }
  if (graph == NULL || out_order == NULL || out_count == NULL) {
    return DS_ERR_NULL;
  }
  if (capacity < graph->vertex_count) {
    return DS_ERR_RANGE;
  }
  indegree = (size_t *)calloc(graph->vertex_count, sizeof(*indegree));
  if (indegree == NULL) {
    return DS_ERR_ALLOC;
  }
  queue = (size_t *)malloc(graph->vertex_count * sizeof(*queue));
  if (queue == NULL) {
    free(indegree);
    return DS_ERR_ALLOC;
  }
  for (i = 0U; i < graph->vertex_count; i++) {
    for (j = 0U; j < graph->vertices[i].edge_count; j++) {
      indegree[graph->vertices[i].edges[j].to]++;
    }
  }
  head = 0U;
  tail = 0U;
  for (i = 0U; i < graph->vertex_count; i++) {
    if (indegree[i] == 0U) {
      queue[tail] = i;
      tail++;
    }
  }
  produced = 0U;
  while (head < tail) {
    i = queue[head];
    head++;
    out_order[produced] = i;
    produced++;
    for (j = 0U; j < graph->vertices[i].edge_count; j++) {
      to = graph->vertices[i].edges[j].to;
      indegree[to]--;
      if (indegree[to] == 0U) {
        queue[tail] = to;
        tail++;
      }
    }
  }
  free(queue);
  free(indegree);
  *out_count = produced;
  if (produced != graph->vertex_count) {
    return DS_ERR_STATE;
  }
  return DS_OK;
}


ds_status_t FUNC(ds_graph_dijkstra)(ds_graph_t *graph, size_t start,
                                     double *out_distances,
                                     size_t *out_previous, size_t capacity) {
  bool *done;
  size_t i;
  size_t j;
  size_t best;
  size_t to;
  double best_distance;
  double next_distance;

  if (graph == NULL || out_distances == NULL || out_previous == NULL) {
    return DS_ERR_NULL;
  }
  if (start >= graph->vertex_count || capacity < graph->vertex_count) {
    return DS_ERR_RANGE;
  }

  for (i = 0U; i < graph->vertex_count; i++) {
    for (j = 0U; j < graph->vertices[i].edge_count; j++) {
      if (graph->vertices[i].edges[j].weight < 0.0) {
        return DS_ERR_RANGE;
      }
    }
  }

  done = (bool *)calloc(graph->vertex_count, sizeof(*done));
  if (done == NULL) {
    return DS_ERR_ALLOC;
  }

  for (i = 0U; i < graph->vertex_count; i++) {
    out_distances[i] = DBL_MAX;
    out_previous[i] = (size_t)-1;
  }
  out_distances[start] = 0.0;

  for (i = 0U; i < graph->vertex_count; i++) {
    best = graph->vertex_count;
    best_distance = DBL_MAX;
    for (j = 0U; j < graph->vertex_count; j++) {
      if (!done[j] && out_distances[j] < best_distance) {
        best_distance = out_distances[j];
        best = j;
      }
    }
    if (best == graph->vertex_count) {
      break;
    }
    done[best] = true;
    for (j = 0U; j < graph->vertices[best].edge_count; j++) {
      to = graph->vertices[best].edges[j].to;
      if (!done[to] && out_distances[best] < DBL_MAX) {
        next_distance = out_distances[best] + graph->vertices[best].edges[j].weight;
        if (next_distance < out_distances[to]) {
          out_distances[to] = next_distance;
          out_previous[to] = best;
        }
      }
    }
  }

  free(done);
  return DS_OK;
}


ds_status_t FUNC(ds_graph_bellman_ford)(ds_graph_t *graph, size_t start,
                                          double *out_distances,
                                          size_t *out_previous, size_t capacity,
                                          bool *out_has_negative_cycle) {
  size_t i;
  size_t j;
  size_t k;
  size_t to;
  bool changed;
  double next_distance;

  if (out_has_negative_cycle != NULL) {
    *out_has_negative_cycle = false;
  }
  if (graph == NULL || out_distances == NULL || out_previous == NULL ||
      out_has_negative_cycle == NULL) {
    return DS_ERR_NULL;
  }
  if (start >= graph->vertex_count || capacity < graph->vertex_count) {
    return DS_ERR_RANGE;
  }
  for (i = 0U; i < graph->vertex_count; i++) {
    out_distances[i] = DBL_MAX;
    out_previous[i] = (size_t)-1;
  }
  out_distances[start] = 0.0;

  for (i = 1U; i < graph->vertex_count; i++) {
    changed = false;
    for (j = 0U; j < graph->vertex_count; j++) {
      if (out_distances[j] >= DBL_MAX) {
        continue;
      }
      for (k = 0U; k < graph->vertices[j].edge_count; k++) {
        to = graph->vertices[j].edges[k].to;
        next_distance = out_distances[j] + graph->vertices[j].edges[k].weight;
        if (next_distance < out_distances[to]) {
          out_distances[to] = next_distance;
          out_previous[to] = j;
          changed = true;
        }
      }
    }
    if (!changed) {
      break;
    }
  }

  for (j = 0U; j < graph->vertex_count; j++) {
    if (out_distances[j] >= DBL_MAX) {
      continue;
    }
    for (k = 0U; k < graph->vertices[j].edge_count; k++) {
      to = graph->vertices[j].edges[k].to;
      if (out_distances[j] + graph->vertices[j].edges[k].weight <
          out_distances[to]) {
        *out_has_negative_cycle = true;
        break;
      }
    }
  }

  return DS_OK;
}

typedef struct graph_mst_edge {
  size_t from;
  size_t to;
  double weight;
  void *data;
} graph_mst_edge_t;

static int graph_mst_edge_compare(const void *a, const void *b) {
  const graph_mst_edge_t *ea;
  const graph_mst_edge_t *eb;

  ea = (const graph_mst_edge_t *)a;
  eb = (const graph_mst_edge_t *)b;
  if (ea->weight < eb->weight) {
    return -1;
  }
  if (ea->weight > eb->weight) {
    return 1;
  }
  return 0;
}

static size_t graph_uf_find(size_t *parent, size_t index) {
  while (parent[index] != index) {
    parent[index] = parent[parent[index]];
    index = parent[index];
  }
  return index;
}

ds_status_t FUNC(ds_graph_minimum_spanning_tree)(ds_graph_t *graph,
                                                  ds_graph_t *out_tree) {
  graph_mst_edge_t *edges;
  size_t edge_total;
  size_t edge_index;
  size_t i;
  size_t j;
  size_t added;
  size_t root_a;
  size_t root_b;
  size_t *parent;
  size_t ignored_index;
  ds_status_t status;

  if (graph == NULL || out_tree == NULL) {
    return DS_ERR_NULL;
  }
  if (out_tree->vertex_count != 0U) {
    return DS_ERR_STATE;
  }

  edge_total = 0U;
  for (i = 0U; i < graph->vertex_count; i++) {
    edge_total += graph->vertices[i].edge_count;
  }
  edges = NULL;
  if (edge_total != 0U) {
    edges = (graph_mst_edge_t *)malloc(edge_total * sizeof(*edges));
    if (edges == NULL) {
      return DS_ERR_ALLOC;
    }
  }
  parent = (size_t *)malloc(graph->vertex_count * sizeof(*parent));
  if (parent == NULL && graph->vertex_count != 0U) {
    free(edges);
    return DS_ERR_ALLOC;
  }

  for (i = 0U; i < graph->vertex_count; i++) {
    status = FUNC(ds_graph_add_vertex)(out_tree, graph->vertices[i].data,
                                       &ignored_index);
    if (status != DS_OK) {
      free(parent);
      free(edges);
      return status;
    }
    parent[i] = i;
  }

  edge_index = 0U;
  for (i = 0U; i < graph->vertex_count; i++) {
    for (j = 0U; j < graph->vertices[i].edge_count; j++) {
      edges[edge_index].from = i;
      edges[edge_index].to = graph->vertices[i].edges[j].to;
      edges[edge_index].weight = graph->vertices[i].edges[j].weight;
      edges[edge_index].data = graph->vertices[i].edges[j].data;
      edge_index++;
    }
  }
  qsort(edges, edge_total, sizeof(*edges), graph_mst_edge_compare);

  added = 0U;
  for (i = 0U; i < edge_total && added + 1U < graph->vertex_count; i++) {
    root_a = graph_uf_find(parent, edges[i].from);
    root_b = graph_uf_find(parent, edges[i].to);
    if (root_a != root_b) {
      parent[root_b] = root_a;
      status = FUNC(ds_graph_add_weighted_edge)(out_tree, edges[i].from,
                                                edges[i].to, edges[i].weight,
                                                edges[i].data);
      if (status != DS_OK) {
        free(parent);
        free(edges);
        return status;
      }
      added++;
    }
  }

  free(parent);
  free(edges);
  if (graph->vertex_count != 0U && added + 1U != graph->vertex_count) {
    return DS_ERR_STATE;
  }
  return DS_OK;
}

ds_status_t FUNC(ds_graph_connected_components)(ds_graph_t *graph,
                                                 size_t *out_components,
                                                 size_t capacity,
                                                 size_t *out_count) {
  size_t *parent;
  size_t *roots;
  size_t i;
  size_t j;
  size_t root;
  size_t count;
  size_t root_a;
  size_t root_b;
  bool found;

  if (out_count != NULL) {
    *out_count = 0U;
  }
  if (graph == NULL || out_components == NULL || out_count == NULL) {
    return DS_ERR_NULL;
  }
  if (capacity < graph->vertex_count) {
    return DS_ERR_RANGE;
  }

  parent = (size_t *)ds_context_alloc(graph->context,
                                      graph->vertex_count * sizeof(*parent));
  roots = (size_t *)ds_context_alloc(graph->context,
                                     graph->vertex_count * sizeof(*roots));
  if (parent == NULL || roots == NULL) {
    ds_context_free(graph->context, parent);
    ds_context_free(graph->context, roots);
    return DS_ERR_ALLOC;
  }

  for (i = 0U; i < graph->vertex_count; i++) {
    parent[i] = i;
  }
  for (i = 0U; i < graph->vertex_count; i++) {
    for (j = 0U; j < graph->vertices[i].edge_count; j++) {
      root_a = graph_uf_find(parent, i);
      root_b = graph_uf_find(parent, graph->vertices[i].edges[j].to);
      if (root_a != root_b) {
        parent[root_b] = root_a;
      }
    }
  }

  count = 0U;
  for (i = 0U; i < graph->vertex_count; i++) {
    root = graph_uf_find(parent, i);
    found = false;
    for (j = 0U; j < count; j++) {
      if (roots[j] == root) {
        out_components[i] = j;
        found = true;
        break;
      }
    }
    if (!found) {
      roots[count] = root;
      out_components[i] = count;
      count++;
    }
  }

  ds_context_free(graph->context, roots);
  ds_context_free(graph->context, parent);
  *out_count = count;
  return DS_OK;
}

static void graph_scc_order_dfs(ds_graph_t *graph, size_t vertex, bool *seen,
                                size_t *order, size_t *count) {
  size_t i;
  size_t to;

  seen[vertex] = true;
  for (i = 0U; i < graph->vertices[vertex].edge_count; i++) {
    to = graph->vertices[vertex].edges[i].to;
    if (!seen[to]) {
      graph_scc_order_dfs(graph, to, seen, order, count);
    }
  }
  order[*count] = vertex;
  (*count)++;
}

static void graph_scc_reverse_mark(size_t vertex, size_t component,
                                   size_t *components,
                                   const size_t *reverse_offsets,
                                   const size_t *reverse_edges) {
  size_t i;
  size_t from;

  components[vertex] = component;
  for (i = reverse_offsets[vertex]; i < reverse_offsets[vertex + 1U]; i++) {
    from = reverse_edges[i];
    if (components[from] == (size_t)-1) {
      graph_scc_reverse_mark(from, component, components, reverse_offsets,
                             reverse_edges);
    }
  }
}

ds_status_t FUNC(ds_graph_strongly_connected_components)(ds_graph_t *graph,
                                                          size_t *out_components,
                                                          size_t capacity,
                                                          size_t *out_count) {
  bool *seen;
  size_t *order;
  size_t *reverse_counts;
  size_t *reverse_offsets;
  size_t *reverse_fill;
  size_t *reverse_edges;
  size_t i;
  size_t j;
  size_t count;
  size_t component_count;
  size_t vertex;
  size_t edge_total;
  size_t to;
  size_t pos;

  if (out_count != NULL) {
    *out_count = 0U;
  }
  if (graph == NULL || out_components == NULL || out_count == NULL) {
    return DS_ERR_NULL;
  }
  if (capacity < graph->vertex_count) {
    return DS_ERR_RANGE;
  }
  seen = (bool *)ds_context_calloc(graph->context, graph->vertex_count,
                                   sizeof(*seen));
  order = (size_t *)ds_context_alloc(graph->context,
                                     graph->vertex_count * sizeof(*order));
  reverse_counts = (size_t *)ds_context_calloc(graph->context,
                                               graph->vertex_count,
                                               sizeof(*reverse_counts));
  reverse_offsets = (size_t *)ds_context_alloc(
      graph->context, (graph->vertex_count + 1U) * sizeof(*reverse_offsets));
  reverse_fill = (size_t *)ds_context_alloc(graph->context,
                                            graph->vertex_count *
                                                sizeof(*reverse_fill));
  if (seen == NULL || order == NULL || reverse_counts == NULL ||
      reverse_offsets == NULL || reverse_fill == NULL) {
    ds_context_free(graph->context, reverse_fill);
    ds_context_free(graph->context, reverse_offsets);
    ds_context_free(graph->context, reverse_counts);
    ds_context_free(graph->context, order);
    ds_context_free(graph->context, seen);
    return DS_ERR_ALLOC;
  }

  edge_total = 0U;
  for (i = 0U; i < graph->vertex_count; i++) {
    out_components[i] = (size_t)-1;
    edge_total += graph->vertices[i].edge_count;
    for (j = 0U; j < graph->vertices[i].edge_count; j++) {
      reverse_counts[graph->vertices[i].edges[j].to]++;
    }
  }

  reverse_offsets[0] = 0U;
  for (i = 0U; i < graph->vertex_count; i++) {
    reverse_offsets[i + 1U] = reverse_offsets[i] + reverse_counts[i];
    reverse_fill[i] = reverse_offsets[i];
  }

  reverse_edges = (size_t *)ds_context_alloc(graph->context,
                                             edge_total * sizeof(*reverse_edges));
  if (edge_total != 0U && reverse_edges == NULL) {
    ds_context_free(graph->context, reverse_fill);
    ds_context_free(graph->context, reverse_offsets);
    ds_context_free(graph->context, reverse_counts);
    ds_context_free(graph->context, order);
    ds_context_free(graph->context, seen);
    return DS_ERR_ALLOC;
  }

  for (i = 0U; i < graph->vertex_count; i++) {
    for (j = 0U; j < graph->vertices[i].edge_count; j++) {
      to = graph->vertices[i].edges[j].to;
      pos = reverse_fill[to];
      reverse_edges[pos] = i;
      reverse_fill[to]++;
    }
  }

  count = 0U;
  for (i = 0U; i < graph->vertex_count; i++) {
    if (!seen[i]) {
      graph_scc_order_dfs(graph, i, seen, order, &count);
    }
  }
  component_count = 0U;
  while (count != 0U) {
    count--;
    vertex = order[count];
    if (out_components[vertex] == (size_t)-1) {
      graph_scc_reverse_mark(vertex, component_count, out_components,
                             reverse_offsets, reverse_edges);
      component_count++;
    }
  }

  ds_context_free(graph->context, reverse_edges);
  ds_context_free(graph->context, reverse_fill);
  ds_context_free(graph->context, reverse_offsets);
  ds_context_free(graph->context, reverse_counts);
  ds_context_free(graph->context, order);
  ds_context_free(graph->context, seen);
  *out_count = component_count;
  return DS_OK;
}
