# ds

`ds` is an intrusive, allocator-aware C data-structure library with optional
thread-safe symbol variants, structured diagnostics/errors, persistent/versioned
containers, and a generic retroactive history layer.

The library is written to keep the low-level C control surface visible while
also offering newer high-level wrappers such as config-based hash tables,
string maps/sets, persistent tries, persistent red-black trees, graph
algorithms, and timeline/history utilities.

## Features

Core mutable containers:

- AVL tree
- Binary search tree
- B-tree
- Deque
- Doubly linked list
- Hash table
- Heap
- Singly linked list
- Queue
- Red-black tree
- Stack
- Trie

Newer high-level modules:

- `ds_status_t`, `ds_error_t`, `ds_diagnostic_t`, and `ds_context_t`
- context-backed arena, pool, and debug allocators
- config-based hash table and trie APIs
- custom hash-table bucket stores
- `ds_string_map_t` and `ds_string_set_t`
- structural-sharing persistent trie
- structural-sharing persistent red-black tree
- generic history/timeline layer with branching, retroactive edits,
  checkpoints, transactions, merge hooks, and portable archive helpers
- directed/weighted graph module with BFS, DFS, topological sort, Dijkstra,
  Bellman-Ford, connected components, strongly connected components, and MST
  helpers

## Build

### With Nix

```sh
nix build
```

### With Make

```sh
make
make test
```

By default, `make` builds both normal and thread-safe library variants:

```txt
libds.a
libds.so
libds_safe.a
libds_safe.so
```

Install and uninstall are available:

```sh
make install
make uninstall
```

## Testing and validation

```sh
make test        # build and run all normal tests
make test-safe   # build safe variants and run thread-safe regression tests
make sanitize    # run tests under ASAN/UBSAN
make examples    # build example programs
make valgrind    # run tests, safe tests, and examples under Valgrind
make check       # alias for make test
```

`make valgrind` runs each executable sequentially and prints a banner before
it runs each one. If Valgrind reports an error or definite/possible leak, the
current executable exits nonzero and the loop stops. The failed binary is the
last `== valgrind ... ==` banner printed.

The split Valgrind targets are:

```sh
make valgrind-test
make valgrind-safe
make valgrind-examples
```

You can override the tool and flags:

```sh
make valgrind VALGRIND=/usr/bin/valgrind
make valgrind VALGRIND_FLAGS="--leak-check=full --error-exitcode=99"
```

## Usage

Use the central header for the normal build:

```c
#include <ds.h>
```

Use `ds_safe.h` when compiling a translation unit against the safe symbol set:

```c
#include <ds_safe.h>
```

`ds_safe.h` defines `DS_THREAD_SAFE` before including `ds.h`, so declarations
map to the `*_safe` symbol names through the library's `FUNC(...)` macro. Link
against `libds_safe.a` or `libds_safe.so` for those symbols.

Compile against an installed library with:

```sh
gcc -o example example.c -lds
```

Compile against the safe variant with:

```sh
gcc -o example_safe example_safe.c -lds_safe -pthread
```

For concrete usage, see:

```txt
examples/hash_table_config.c
examples/string_map.c
examples/persistent_trie.c
examples/allocators.c
```

## API style

The library currently has two API generations.

Legacy APIs expose the original intrusive containers directly and often return
private sentinels such as `tree->nil` or `table->nil` for missing values. These
remain for compatibility, but new code should prefer the newer `ds_`-prefixed
APIs where available.

New APIs generally use:

```txt
ds_status_t return value
out-parameters for returned data
ds_context_t for allocation, diagnostics, and primary errors
config structs for callbacks and ownership policy
```

Example style:

```c
void *value;
ds_status_t status;

status = ds_string_map_get(map, "name", &value);
if (status == DS_OK) {
  /* value found */
} else if (status == DS_NOT_FOUND) {
  /* key missing */
}
```

## Diagnostics and errors

`ds` separates failure handling from observability:

- `ds_status_t` is the cheap control-flow result.
- `ds_error_t` is the primary failure object stored in a `ds_context_t`.
- `ds_diagnostic_t` is a structured event emitted to a diagnostic sink.

A successful operation may emit diagnostics. A failed operation returns a status
and may set one primary error in the context.

See `docs/diagnostics.md`.

## Ownership

By default, containers own only the internal memory they allocate. User keys,
values, payloads, and intrusive nodes are borrowed unless a config or destroy
callback says otherwise.

See `docs/ownership.md`.

## Thread-safety

The safe build provides separate `*_safe` symbols guarded by recursive mutexes
inside participating containers. It is intended to protect individual container
operations, not arbitrary multi-operation transactions. Iteration and composite
workflows may still require external synchronization unless the specific API
documents otherwise.

For multi-threaded code that cares about diagnostics or last-error state, prefer
an explicit `ds_context_t` per thread or per object. The default context is a
process-global fallback.

## Documentation

Additional docs:

```txt
docs/api-overview.md
docs/diagnostics.md
docs/history.md
docs/ownership.md
docs/roadmap.md
docs/testing.md
```

## Contributing

Before submitting changes:

```sh
make test
make test-safe
make sanitize
make examples
make valgrind    # when Valgrind is installed
```

Also format C changes with `clang-format`.
