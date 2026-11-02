# Project 9 — Event scheduler: heap against balanced tree

CS 253 / MA 253 Data Structures and Algorithms Laboratory, IIT Indore · Team **O(n²)**

A network simulator keeps a future event set of timestamped events (ACK arrivals, retransmission
timeouts, watchdog timers), most of which are cancelled before they fire. We implement that event set
twice — **Structure A**, a binary min-heap with lazy cancellation, and **Structure B**, an AVL tree with
true deletion — check both against a sorted-array **oracle**, attack them with adversarial inputs, and
measure them under Jones's hold model and a cancellation sweep.

## One command

```bash
make all
```

builds everything, runs every test, then (once added) the full-scale demo and every benchmark, strictly
in that order. Requirements: Linux or WSL, `g++` with C++17, GNU make, Python 3 + matplotlib (plots only).

Other targets: `make build`, `make test`, `make sanitize` (AddressSanitizer + UBSan build in
`build/sanitize`), `make BUILD=count test` (operation-counting build), `make clean`.

## Team and roles (fixed at the first checkpoint)

| Role | Member | Owns | Derivation |
|---|---|---|---|
| Architect | Aditya Singh (250003007) | `include/`, `app/`, `scripts/`, `Makefile`, `INTERFACE.md`, README | D3 |
| Structure A owner | *open* | `src/structA/` — lazy binary heap + hash table | D1 |
| Structure B owner | *open* | `src/structB/` — AVL tree | D2 |
| Oracle owner | *open* | `oracle/` — oracle, replay, differential test, shrinker, checkers | D4 |
| Adversary | *open* | `adversary/` — distributions, attack generators, edge-case suites | D5 |
| Analyst | *open* | `bench/`, `results/`, `docs/MODEL_D6.md` | D6 |

## Repository layout

```
include/            shared interface (event_queue.h), randomness (rng.h)      Architect
src/structA/        Structure A: lazy heap                                    Structure A owner
src/structB/        Structure B: AVL tree                                     Structure B owner
oracle/             oracle, replay, differential test, shrinker, checkers    Oracle owner
adversary/          distributions, generators, .ops suites                    Adversary
app/                retransmission-timer simulator                            Architect
bench/              hold-model and cancellation-sweep benchmarks              Analyst
results/            raw CSV, profiler output, figures, plot.py               Analyst
tests/              unit and contract tests
docs/               MODEL_D6.md, derivations/D1.pdf … D6.pdf
INTERFACE.md        the contract and invariants (merged before any structure)
REPORT.md           design, figures, predicted vs measured (later)
TOOLS.md            every tool / AI used, on what, and what we changed
```

## How we work

- `main` is protected: every change arrives through a pull request approved by another member.
- Branches are named `<role>/<topic>`, e.g. `heap/purge`, `avl/delete`, `architect/simulator`.
- Each member commits their own module from their own account, in their own directory.
- The submission is the commit tagged `final`.

## Status

- [x] PR #1 — interface, randomness, contract test, build system, skeleton
- [ ] Structure A · Structure B · oracle + replay + checkers
- [ ] Differential test, adversarial suites
- [ ] Simulator (`app/`), identical-output check
- [ ] Benchmarks, figures, REPORT.md, derivations
