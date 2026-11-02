// include/event_queue.h — the one interface every event queue in this project implements.
// Owner: Architect. Changing this file needs a PR approved by both structure owners and the
// oracle owner. The full contract (edge cases, invariants, costs) is in INTERFACE.md.
//
// Implementations: Structure A = lazy binary heap (src/structA), Structure B = AVL tree
// (src/structB), oracle = sorted array (oracle/).
#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace eq {

using Time = std::int64_t;   // simulated time in integer microseconds
using Seq  = std::uint64_t;  // insertion number: 0, 1, 2, ... never reused

// Seqs 0 .. kSeqLimit-1 are usable. The two largest uint64 values are reserved: Structure A's
// hash table uses them to mark EMPTY (UINT64_MAX) and TOMBSTONE (UINT64_MAX - 1) slots.
inline constexpr Seq kSeqLimit = std::numeric_limits<Seq>::max() - 1;

// Key = (time, seq), compared lexicographically. seq is unique, so no two keys are equal:
// a strict total order. Equal times are ordered by seq = insertion order (FIFO).
struct Key {
    Time time;
    Seq  seq;
};
inline bool operator<(const Key& a, const Key& b) noexcept {
    return a.time < b.time || (a.time == b.time && a.seq < b.seq);
}
inline bool operator==(const Key& a, const Key& b) noexcept {
    return a.time == b.time && a.seq == b.seq;
}
inline bool operator!=(const Key& a, const Key& b) noexcept { return !(a == b); }
inline bool operator>(const Key& a, const Key& b) noexcept { return b < a; }

// A Handle names one inserted event. It is a copy of that event's key, so the AVL tree can
// cancel by an ordinary keyed delete. It is a value, not a secret: a forged value equal to a
// pending key names that event. Handles are only meaningful for the queue that issued them.
using Handle = Key;

// What a queue stores. type/data are opaque to the queue; only key takes part in ordering.
struct Event {
    Key           key;
    std::uint32_t type;
    std::uint32_t data;
};
static_assert(sizeof(Event) == 24, "Unsupported Event layout: update the memory figures in REPORT.md");

// Operation counters for the Analyst. Only updated when built with -DEQ_COUNTING=1, so timing
// builds pay nothing. Never put a side effect inside EQ_COUNT's arguments.
struct Counters {
    std::uint64_t comparisons      = 0;
    std::uint64_t moves            = 0;
    std::uint64_t rotations_single = 0;
    std::uint64_t rotations_double = 0;
    std::uint64_t purges           = 0;
    std::uint64_t purged_entries   = 0;
    std::uint64_t hash_probes      = 0;
};
#ifndef EQ_COUNTING
#define EQ_COUNTING 0
#endif
#define EQ_COUNT(field, n) do { if (EQ_COUNTING) this->counters_.field += (n); } while (0)

class EventQueue {
public:
    EventQueue() = default;
    virtual ~EventQueue() = default;
    // One queue object = one simulation state: no copying or moving (v1).
    EventQueue(const EventQueue&) = delete;
    EventQueue& operator=(const EventQueue&) = delete;
    EventQueue(EventQueue&&) = delete;
    EventQueue& operator=(EventQueue&&) = delete;

    // Schedule an event at time t (any int64 value is accepted; only the simulator restricts
    // itself to t >= now). Assigns the next seq. The seq counter advances only after the
    // structure has stored the event.
    Handle insert(Time t, std::uint32_t type, std::uint32_t data) {
        if (next_seq_ >= kSeqLimit) throw std::overflow_error("event_queue: sequence numbers exhausted");
        const Event e{Key{t, next_seq_}, type, data};
        do_insert(e);   // allocation failure is fatal to the run
        ++next_seq_;
        return e.key;
    }

    // true exactly once, while the named event is pending; then it is never returned again.
    // false (and no observable change) if it was already popped, already cancelled, or never
    // issued — including a right seq with a wrong time. Implementations must start with
    // `if (h.seq >= issued()) return false;`.
    virtual bool cancel(const Handle& h) = 0;

    // Remove the pending event with the smallest key into out. false iff none; out untouched.
    virtual bool pop_min(Event& out) = 0;

    // Copy the pending event with the smallest key. false iff none; out untouched.
    // Not const: the lazy heap may physically discard cancelled entries while looking.
    virtual bool peek_min(Event& out) = 0;

    // Copy the pending event with the smallest key among those with key.time > t (strictly).
    // false iff none; out untouched. Compare times directly — never compute t + 1 (overflow).
    virtual bool successor(Time t, Event& out) = 0;

    // Number of pending events (never counts cancelled entries a structure still holds).
    virtual std::size_t size() const = 0;
    virtual const char* name() const = 0;   // "heap", "avl" or "oracle"

    bool empty() const { return size() == 0; }
    Seq issued() const { return next_seq_; }  // successful inserts so far; never resets
    const Counters& counters() const { return counters_; }
    void reset_counters() { counters_ = Counters{}; }

protected:
    virtual void do_insert(const Event& e) = 0;   // e.key.seq is fresh, unique and < kSeqLimit
    Counters counters_;

private:
    Seq next_seq_ = 0;
};

}  // namespace eq
