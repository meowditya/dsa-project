# INTERFACE.md — common interface and structural invariants

Owner: Architect. Merged in PR #1, **before any structure was implemented**. Any later change needs
a PR approved by the Structure A owner, the Structure B owner and the Oracle owner.
Code form: `include/event_queue.h` (types and the abstract class) and `include/rng.h` (randomness).

## 1. Types

| Name | Definition | Meaning |
|---|---|---|
| `Time` | `int64_t` | simulated time in integer microseconds |
| `Seq` | `uint64_t` | insertion number 0, 1, 2, … assigned by the queue; never reused; usable values `0 … kSeqLimit−1` (`kSeqLimit = UINT64_MAX − 1`; the top two values are reserved as hash-table markers) |
| `Key` | `{Time time; Seq seq;}` | `a < b` ⇔ `a.time < b.time`, or equal times and `a.seq < b.seq` |
| `Handle` | `= Key` | returned by `insert`; names one event of the issuing queue |
| `Event` | `{Key key; uint32_t type; uint32_t data;}` | 24 bytes on our toolchain (checked by `static_assert`); only `key` takes part in ordering |

Seqs are unique within a queue, so no two keys are equal and `<` is a strict total order. Events with
equal times leave in insertion order (FIFO).

## 2. Abstract state

A queue represents a set **P of pending events**: inserted, not yet popped, not yet successfully
cancelled. All behaviour below is defined on P. How a structure stores P (the lazy heap may still hold
cancelled entries) is invisible to callers.

## 3. Operations

| Operation | Effect on P | Returns |
|---|---|---|
| `insert(t, type, data)` | adds e with `e.key = (t, issued())` | the handle `e.key` |
| `cancel(h)` | removes the e ∈ P with `e.key == h`, if any | `true` iff it removed one |
| `pop_min(out)` | removes the minimum-key e, if P ≠ ∅ | `true` and `out = e`; `false` if empty |
| `peek_min(out)` | none | as `pop_min` without removal |
| `successor(t, out)` | none | the minimum-key e ∈ P with `e.key.time > t` (strict) |
| `size()` | none | \|P\| |
| `issued()` | none | number of successful inserts so far; never resets |
| `name()` | none | `"heap"`, `"avl"` or `"oracle"` |

**Costs** — L = |P|; m = physical heap entries (m ≤ 2L at every public-operation boundary).
Logarithms use `log(L+2)` so empty and one-element queues are covered.

| Operation | Heap (A) | AVL (B) | Oracle |
|---|---|---|---|
| insert | expected amortised O(log(L+2)) | O(log(L+2)) + amortised pool growth | O(log(L+2)) search + O(L) shift |
| cancel | expected O(1) marking; O(log(L+2)) amortised when stale-root cleanup is charged to it | O(log(L+2)) | O(L) |
| pop_min / peek_min | expected amortised O(log(L+2)); a single call may discard many stale entries | O(log(L+2)) | O(1) |
| successor | visits O(m) nodes (expected O(m) work, given O(1) expected hash lookups) | O(log(L+2)) | O(log(L+2)) |
| size / issued | O(1) | O(1) | O(1) |

"Expected" costs rest on the stated assumption that hash lookups take O(1) expected probes.
Vector growth makes some single inserts linear; this is amortised separately.

## 4. Edge cases — all three implementations must agree exactly

| Case | Required behaviour |
|---|---|
| `pop_min` / `peek_min` / `successor` with no answer | return `false`, `out` unchanged |
| `cancel` of a popped event | `false` |
| `cancel` twice | `true`, then `false` |
| `cancel` with `h.seq >= issued()` (never issued; includes both reserved seqs) | `false`, checked first, before any lookup |
| `cancel` with a real seq but a different time | `false` |
| `cancel` with a value exactly equal to a pending key | `true` — handles are values, not secrets |
| many events at one time | returned in increasing seq |
| `successor(t)` with an event at exactly time t | that event is not returned (strictly greater) |
| `successor(INT64_MAX)` / `successor(INT64_MIN)` | `false` / the minimum event (if any) |
| insert with a time earlier than events already popped | allowed (general priority queue); only the simulator forbids it |
| `size()` after cancels | counts pending events only |
| handle from a different queue object | unsupported (identical key values can exist in two queues) |

Queues are single-threaded. Allocation failure ends the run; do not keep using a queue after one.

## 5. Structural invariants (checked by the oracle owner's checkers)

**Structure A — lazy heap** (array `a[0..m−1]`, table `T`, counters `L`, `C`)
1. Heap order: for every i ≥ 1, `!(a[i].key < a[(i−1)/2].key)`.
2. Every seq in `a` is unique and `< issued()`.
3. `T` mirrors `a` exactly: each `a[i]` has exactly one record `(seq, time, cancelled)` with the same
   time, and `T` holds no other records (no orphans). Checked by enumerating `T`'s records, not only
   by lookups and counts.
4. `L` = records not flagged cancelled = `size()`; `C` = records flagged cancelled; `L + C = m`.
5. Purge invariant (normal mode): `C ≤ L` after every public operation, hence `m ≤ 2L`.
6. Table health: capacity is a power of two ≥ 16; `occupied + tombstones ≤ capacity / 2`;
   no real key equals a reserved value.

**Structure B — AVL tree** (node pool, root index, count, free list)
1. Every child index is in range; following children never revisits a node (no cycles, no node
   reached twice).
2. Strict BST order on keys.
3. Stored heights are correct: `h(NIL) = −1`, `h(x) = 1 + max(h(left), h(right))`.
4. Balance: every node has `|h(left) − h(right)| ≤ 1` (computed from recomputed heights).
5. Every reachable seq is unique and `< issued()`.
6. Reachable node count = `size()`; every non-NIL pool slot is either reachable or on the free list,
   never both; free-list entries are distinct and valid.

**Oracle** (array `v[0..k−1]`)
1. Strictly descending: `v[i+1].key < v[i].key`; the minimum is `v[k−1]`.
2. Seqs unique and `< issued()`; `k = size()`.

## 6. Determinism rules

1. Randomness comes only from `eq::Rng` (`include/rng.h`), seeded from the command line. No
   `std::*_distribution`, no `rand()`, no time-based seeds.
2. The simulator uses integers only. Benchmark generators may use `u01()` with `log`/`sqrt`; their
   outputs are saved as integer streams for exact replay across machines.
3. stdout contains only deterministic canonical output (integers, fixed hex). Queue name, wall time,
   memory and anything machine-dependent go to stderr. Use the "C" locale.
4. No program effect may sit inside `assert(...)`: release builds use `-DNDEBUG`, which deletes it.
   Required checks are explicit `if (!ok) { report; exit(3); }`.
5. Every `now + delay` is preceded by an overflow check (`delay <= INT64_MAX − now`).

## 7. Operation stream format (`.ops`)

Text, one operation per line, `#` starts a comment, blank lines ignored. Literal semantics, no remapping:

```
I <time> <type> <data>   insert; the k-th I line (0-based) creates handle H[k]
C <k>                    cancel(H[k]); if H[k] does not exist yet → cancel returns false
F <time> <seq>           cancel(Handle{time, seq}) with these literal values (any int64 / uint64)
P                        pop_min
K                        peek_min
S <t>                    successor(t)
```

A malformed line (unknown op, wrong argument count, out-of-range number) is an input error:
message with file name and line number on stderr, exit code 2.

`bin/replay --queue heap|avl|oracle file.ops` prints exactly one line per operation:

```
I <seq>
C 1   |  C 0          F 1  |  F 0
P <time> <seq> <type> <data>   |  P -
K <time> <seq> <type> <data>   |  K -
S <time> <seq> <type> <data>   |  S -
END size=<size()> issued=<issued()>
```

Replaying one file must give byte-identical stdout on all three queues.

## 8. Exit codes (all executables)

0 success · 2 invalid command line or input file · 3 invariant / differential / simulation check
failed · 4 resource, time-overflow or sequence exhaustion.

## 9. Ownership

`include/`, `app/`, `scripts/`, `Makefile`, `README.md`, this file: Architect.
`src/structA/`: Structure A owner. `src/structB/`: Structure B owner. `oracle/`: Oracle owner.
`adversary/`: Adversary. `bench/`, `results/`, `docs/MODEL_D6.md`: Analyst.
`docs/derivations/Dk.pdf`: the owner of derivation k.
