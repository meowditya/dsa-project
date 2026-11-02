// tests/interface_contract_test.cpp — checks the interface itself (PR #1), using a deliberately
// naive reference queue that scans a vector. This is NOT one of the structures under study; it
// only shows that the header compiles and that the contract in INTERFACE.md §4 is expressible.
// Each structure owner later runs the same edge cases against their own implementation.
#include "event_queue.h"
#include "rng.h"

#include <cstdio>
#include <cstdlib>
#include <limits>
#include <type_traits>
#include <vector>

using namespace eq;

namespace {

int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", what); ++failures; }
}

class ScanQueue final : public EventQueue {
public:
    bool cancel(const Handle& h) override {
        if (h.seq >= issued()) return false;
        for (std::size_t i = 0; i < v_.size(); ++i) {
            if (v_[i].key == h) { v_.erase(v_.begin() + static_cast<long>(i)); return true; }
        }
        return false;
    }
    bool pop_min(Event& out) override {
        const long i = argmin(std::numeric_limits<Time>::min(), false);
        if (i < 0) return false;
        out = v_[static_cast<std::size_t>(i)];
        v_.erase(v_.begin() + i);
        return true;
    }
    bool peek_min(Event& out) override {
        const long i = argmin(std::numeric_limits<Time>::min(), false);
        if (i < 0) return false;
        out = v_[static_cast<std::size_t>(i)];
        return true;
    }
    bool successor(Time t, Event& out) override {
        const long i = argmin(t, true);
        if (i < 0) return false;
        out = v_[static_cast<std::size_t>(i)];
        return true;
    }
    std::size_t size() const override { return v_.size(); }
    const char* name() const override { return "scan"; }

protected:
    void do_insert(const Event& e) override { v_.push_back(e); }

private:
    // index of the smallest key, optionally only among events with time > t; -1 if none
    long argmin(Time t, bool only_after) const {
        long best = -1;
        for (std::size_t i = 0; i < v_.size(); ++i) {
            if (only_after && !(v_[i].key.time > t)) continue;
            if (best < 0 || v_[i].key < v_[static_cast<std::size_t>(best)].key) best = static_cast<long>(i);
        }
        return best;
    }
    std::vector<Event> v_;
};

bool same(const Event& a, const Event& b) {
    return a.key == b.key && a.type == b.type && a.data == b.data;
}

}  // namespace

int main() {
    static_assert(!std::is_copy_constructible<ScanQueue>::value, "queues must not be copyable");
    static_assert(!std::is_move_constructible<ScanQueue>::value, "queues must not be movable");

    ScanQueue q;
    EventQueue& Q = q;                      // always use the queue through the interface
    const Event sentinel{Key{-7, 7}, 7, 7};
    Event out = sentinel;

    // empty queue: every query fails and leaves out untouched
    check(!Q.pop_min(out) && same(out, sentinel), "pop on empty leaves out unchanged");
    check(!Q.peek_min(out) && same(out, sentinel), "peek on empty leaves out unchanged");
    check(!Q.successor(0, out) && same(out, sentinel), "successor on empty leaves out unchanged");
    check(Q.empty() && Q.issued() == 0, "fresh queue is empty");

    // seqs are 0,1,2,... in insertion order; ties on time leave FIFO
    const Handle a = Q.insert(5, 1, 100);
    const Handle b = Q.insert(5, 1, 200);
    const Handle c = Q.insert(3, 2, 300);
    check(a.seq == 0 && b.seq == 1 && c.seq == 2 && Q.issued() == 3, "seq assignment");
    check(Q.peek_min(out) && out.data == 300 && Q.size() == 3, "peek returns min without removing");

    // successor is strictly greater, ties broken by seq
    check(Q.successor(3, out) && out.data == 100, "successor(3) -> (5,0)");
    out = sentinel;
    check(!Q.successor(5, out) && same(out, sentinel), "successor(5) is strict -> none");
    check(!Q.successor(std::numeric_limits<Time>::max(), out), "successor(INT64_MAX) -> none");
    check(Q.successor(std::numeric_limits<Time>::min(), out) && out.data == 300, "successor(INT64_MIN) -> min");

    // cancel semantics
    check(!Q.cancel(Handle{5, 99}), "never-issued seq -> false");
    check(!Q.cancel(Handle{0, std::numeric_limits<Seq>::max()}), "reserved EMPTY seq -> false");
    check(!Q.cancel(Handle{0, std::numeric_limits<Seq>::max() - 1}), "reserved TOMB seq -> false");
    check(!Q.cancel(Handle{4, a.seq}), "right seq, wrong time -> false");
    check(Q.cancel(a) && !Q.cancel(a), "cancel twice -> true then false");
    check(Q.size() == 2 && Q.issued() == 3, "size counts pending only; issued never decreases");

    // pop order and cancel-after-pop
    check(Q.pop_min(out) && out.data == 300, "pop min (3,2)");
    check(!Q.cancel(c), "cancel after pop -> false");
    check(Q.pop_min(out) && out.data == 200, "pop (5,1)");
    check(Q.empty() && !Q.pop_min(out), "drained");

    // past insertions are allowed for the general queue
    Q.insert(-10, 0, 1);
    check(Q.peek_min(out) && out.key.time == -10, "insert in the past allowed");

    // rng: reproducible, in range, unbiased mapping helper behaves at the edges
    Rng r1(42), r2(42);
    bool same_stream = true, in_range = true;
    for (int i = 0; i < 100000; ++i) {
        const std::uint64_t x = r1.below(1000000), y = r2.below(1000000);
        same_stream = same_stream && x == y;
        in_range = in_range && x < 1000000;
    }
    check(same_stream, "same seed -> same stream");
    check(in_range, "below(n) < n");
    Rng r3(1);
    check(r3.below(1) == 0, "below(1) == 0");
    bool threw = false;
    try { r3.below(0); } catch (const std::invalid_argument&) { threw = true; }
    check(threw, "below(0) throws");
    const std::uint64_t huge = (std::uint64_t{1} << 63) + 1;
    bool huge_ok = true;
    for (int i = 0; i < 10000; ++i) huge_ok = huge_ok && r3.below(huge) < huge;
    check(huge_ok, "below(2^63+1) in range");
    for (int i = 0; i < 1000; ++i) { const double u = r3.u01(); if (!(u >= 0.0 && u < 1.0)) { check(false, "u01 in [0,1)"); break; } }

    // the C++ standard fixes this value for a default-seeded mt19937_64
    std::mt19937_64 g;
    for (int i = 1; i < 10000; ++i) g();
    check(g() == 9981545732273789042ULL, "mt19937_64 conformance (10000th output, default seed)");

    if (failures) { std::fprintf(stderr, "%d check(s) failed\n", failures); return 3; }
    std::printf("interface contract test: all checks passed\n");
    return 0;
}
