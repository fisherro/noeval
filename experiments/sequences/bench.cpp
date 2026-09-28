// Microbenchmark of persistent sequence structures, to check the choice of
// sequence type in design/data-structures.md. See README.md in this
// directory.
//
// Usage: bench time      prints nanoseconds per unit, the minimum of 7 runs
//        bench callgrind prints labels for run.bash to join with the
//                        instruction counts callgrind dumps for each one

#include <immer/algorithm.hpp>
#include <immer/flex_vector.hpp>
#include <immer/flex_vector_transient.hpp>
#include <immer/vector.hpp>
#include <immer/vector_transient.hpp>

#include <valgrind/callgrind.h>

#include <malloc.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <functional>
#include <memory>
#include <new>
#include <numeric>
#include <print>
#include <random>
#include <string>
#include <utility>
#include <vector>

// Counting allocations

namespace {
    std::size_t allocation_count{0};
    std::size_t live_bytes{0};
    // Only the memory mode counts bytes, so the other modes don't pay for
    // malloc_usable_size.
    bool count_bytes{false};
}

void* operator new(std::size_t size)
{
    ++allocation_count;
    if (void* p = std::malloc(size ? size : 1)) {
        if (count_bytes) live_bytes += malloc_usable_size(p);
        return p;
    }
    throw std::bad_alloc{};
}

void operator delete(void* p) noexcept
{
    if (p and count_bytes) live_bytes -= malloc_usable_size(p);
    std::free(p);
}

void operator delete(void* p, std::size_t) noexcept { operator delete(p); }
void* operator new[](std::size_t size) { return operator new(size); }
void operator delete[](void* p) noexcept { operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { operator delete(p); }

namespace {

template<typename T>
void keep(const T& value)
{
    asm volatile("" : : "g"(&value) : "memory");
}

// Elements

// An element stands for a Noeval value_ptr: a pointer to a separately
// allocated, reference-counted object. Copying one touches the object's
// count, as copying a value_ptr does. The counts aren't atomic (see
// README.md).
struct payload {
    long refs{0};
    long number{0};
};

class element {
public:
    element() = default;
    explicit element(payload* p): p_(p) { ++p_->refs; }
    element(const element& that): p_(that.p_) { if (p_) ++p_->refs; }
    element(element&& that) noexcept: p_(std::exchange(that.p_, nullptr)) {}
    element& operator=(element that) noexcept
    {
        std::swap(p_, that.p_);
        return *this;
    }
    // The pool keeps a reference to every payload, so none is freed here,
    // but the test a real release makes is still made.
    ~element() { if (p_ and 0 == --p_->refs) std::abort(); }
    long number() const { return p_->number; }
    std::uintptr_t address() const { return reinterpret_cast<std::uintptr_t>(p_); }
private:
    payload* p_{nullptr};
};

long number_of(const element& e) { return e.number(); }
std::uintptr_t address_of(const element& e) { return e.address(); }

// The elements the sequences hold. The payloads are allocated one by one and
// then shuffled, so neighbouring elements of a sequence point to scattered
// objects, as they would in a real heap.
struct pool {
    std::vector<payload*> payloads;
    std::vector<element> elements;
    std::vector<std::uint8_t> bytes;

    explicit pool(std::size_t n)
    {
        std::mt19937_64 rng{1};
        for (std::size_t i{0}; i < n; ++i) {
            payloads.push_back(new payload{0, static_cast<long>(i)});
        }
        std::shuffle(payloads.begin(), payloads.end(), rng);
        for (auto* p: payloads) elements.emplace_back(p);
        for (std::size_t i{0}; i < n; ++i) bytes.push_back(rng() & 0xff);
    }
};

constexpr std::size_t pool_size{1 << 20};

// Cons lists

// A cons list whose links are std::shared_ptr, each with its own control
// block, as Noeval's are (value::make uses new, not make_shared).
template<typename T>
struct shared_links {
    struct node;
    using ptr = std::shared_ptr<node>;
    struct node {
        T car;
        ptr cdr;
    };
    static ptr make(T car, ptr cdr)
    {
        return ptr(new node{std::move(car), std::move(cdr)});
    }
    static const T& car(const ptr& p) { return p->car; }
    static const ptr& cdr(const ptr& p) { return p->cdr; }
    // Destroy a list iteratively, since a node's destructor would recurse
    // down a list of a million.
    static void release(ptr p)
    {
        while (p and 1 == p.use_count()) {
            ptr next{std::move(p->cdr)};
            p = std::move(next);
        }
    }
};

// Allocates blocks of one size, keeping freed ones on a list for reuse, as
// immer's free lists do.
template<std::size_t Size>
struct free_list {
    static inline void* head{nullptr};
    static void* allocate()
    {
        if (not head) return ::operator new(Size);
        void* p{head};
        head = *static_cast<void**>(p);
        return p;
    }
    static void deallocate(void* p)
    {
        *static_cast<void**>(p) = head;
        head = p;
    }
};

// A cons list with intrusive, non-atomic reference counts, whose nodes come
// from malloc or, if Pooled, from a free list.
template<typename T, bool Pooled = false>
struct intrusive_links {
    struct node {
        long refs;
        T car;
        node* cdr;
    };
    class ptr {
    public:
        ptr() = default;
        explicit ptr(node* n): n_(n) {}
        ptr(const ptr& that): n_(that.n_) { if (n_) ++n_->refs; }
        ptr(ptr&& that) noexcept: n_(std::exchange(that.n_, nullptr)) {}
        ptr& operator=(ptr that) noexcept
        {
            std::swap(n_, that.n_);
            return *this;
        }
        ~ptr()
        {
            node* n{n_};
            while (n and 0 == --n->refs) {
                node* next{n->cdr};
                destroy(n);
                n = next;
            }
        }
        explicit operator bool() const { return n_; }
        node* get() const { return n_; }
        node* release() { return std::exchange(n_, nullptr); }
    private:
        node* n_{nullptr};
    };
    static node* create(T car, node* cdr)
    {
        if constexpr (Pooled) {
            return new (free_list<sizeof(node)>::allocate()) node{1, std::move(car), cdr};
        } else {
            return new node{1, std::move(car), cdr};
        }
    }
    static void destroy(node* n)
    {
        if constexpr (Pooled) {
            n->~node();
            free_list<sizeof(node)>::deallocate(n);
        } else {
            delete n;
        }
    }
    static ptr make(T car, ptr cdr) { return ptr(create(std::move(car), cdr.release())); }
    static const T& car(const ptr& p) { return p.get()->car; }
    // A view of the cdr that shares the node's reference: copying it takes a
    // reference of its own.
    static ptr cdr(const ptr& p)
    {
        node* d{p.get()->cdr};
        if (d) ++d->refs;
        return ptr(d);
    }
    static void release(ptr) {}
};

template<typename T, typename Links>
struct cons_list {
    using ptr = typename Links::ptr;
    struct seq {
        ptr head;
        seq() = default;
        explicit seq(ptr h): head(std::move(h)) {}
        seq(const seq&) = default;
        seq(seq&&) = default;
        seq& operator=(seq that) noexcept
        {
            std::swap(head, that.head);
            return *this;
        }
        ~seq() { Links::release(std::move(head)); }
    };

    static constexpr bool linear_rest{false};
    static constexpr bool linear_front{false};
    static constexpr bool linear_index{true};
    static constexpr bool linear_shared_update{true};
    static constexpr bool linear_unique_update{true};
    static constexpr bool linear_persistent_growth{false};
    static constexpr bool grows_at_front{true};

    // Build from an array: cons from the end, one allocation per element.
    static seq build(const T* first, std::size_t n)
    {
        ptr p;
        for (std::size_t i{n}; i > 0; --i) p = Links::make(first[i - 1], std::move(p));
        return seq(std::move(p));
    }
    // Build incrementally, in order, not knowing the length in advance:
    // cons onto the front and reverse at the end, as Lisp code does.
    static seq build_incrementally(const T* first, std::size_t n)
    {
        seq reversed;
        for (std::size_t i{0}; i < n; ++i) reversed = push_front(reversed, first[i]);
        seq result;
        for (ptr p{reversed.head}; p; p = Links::cdr(p)) {
            result = push_front(result, Links::car(p));
        }
        return result;
    }
    static seq push_front(const seq& s, T e) { return seq(Links::make(std::move(e), s.head)); }
    static seq grow(const seq& s, T e) { return push_front(s, std::move(e)); }
    static bool is_empty(const seq& s) { return not s.head; }
    static const T& first(const seq& s) { return Links::car(s.head); }
    static seq rest(const seq& s) { return seq(Links::cdr(s.head)); }
    static std::size_t size(const seq& s)
    {
        std::size_t count{0};
        for (auto* n{s.head.get()}; n; n = raw_next(n)) ++count;
        return count;
    }
    static const T& nth(const seq& s, std::size_t i)
    {
        auto* n{s.head.get()};
        for (; i > 0; --i) n = raw_next(n);
        return n->car;
    }
    // Copy the nodes before the index, and share the rest.
    static seq set(const seq& s, std::size_t i, T e)
    {
        std::vector<T> prefix;
        ptr p{s.head};
        for (; i > 0; --i) {
            prefix.push_back(Links::car(p));
            p = Links::cdr(p);
        }
        ptr result{Links::make(std::move(e), Links::cdr(p))};
        for (std::size_t j{prefix.size()}; j > 0; --j) {
            result = Links::make(prefix[j - 1], std::move(result));
        }
        return seq(std::move(result));
    }
    static seq set_unique(seq&& s, std::size_t i, T e) { return set(s, i, std::move(e)); }
    // Copy a, and share b.
    static seq concat(const seq& a, const seq& b)
    {
        std::vector<T> items;
        for (ptr p{a.head}; p; p = Links::cdr(p)) items.push_back(Links::car(p));
        ptr result{b.head};
        for (std::size_t j{items.size()}; j > 0; --j) {
            result = Links::make(items[j - 1], std::move(result));
        }
        return seq(std::move(result));
    }
    static seq slice(const seq& s, std::size_t from, std::size_t to)
    {
        ptr p{s.head};
        for (std::size_t i{0}; i < from; ++i) p = Links::cdr(p);
        std::vector<T> items;
        for (std::size_t i{from}; i < to; ++i) {
            items.push_back(Links::car(p));
            p = Links::cdr(p);
        }
        return build(items.data(), items.size());
    }
    template<typename F>
    static void for_each(const seq& s, F f)
    {
        for (auto* n{s.head.get()}; n; n = raw_next(n)) f(n->car);
    }
    template<typename F>
    static void for_each_from(const seq& s, std::size_t start, F f)
    {
        auto* n{s.head.get()};
        for (; start > 0; --start) n = raw_next(n);
        for (; n; n = raw_next(n)) f(n->car);
    }
private:
    template<typename N>
    static N* raw_next(N* n)
    {
        if constexpr (std::is_pointer_v<decltype(n->cdr)>) {
            return n->cdr;
        } else {
            return n->cdr.get();
        }
    }
};

template<typename T> using cons_shared = cons_list<T, shared_links<T>>;
template<typename T> using cons_intrusive = cons_list<T, intrusive_links<T>>;
template<typename T> using cons_pooled = cons_list<T, intrusive_links<T, true>>;

// A flat array in a single allocation, shared by reference count, updated in
// place when nothing else refers to it and copied otherwise. Growing it in
// place doubles its capacity. rest and slice are views into it.
template<typename T>
struct flat_array {
    struct block {
        long refs;
        std::size_t size;
        std::size_t capacity;
        T* items() { return reinterpret_cast<T*>(this + 1); }
    };
    static_assert(alignof(T) <= alignof(block));

    static block* allocate(std::size_t capacity)
    {
        void* p{::operator new(sizeof(block) + capacity * sizeof(T))};
        return new (p) block{1, 0, capacity};
    }
    static void release(block* b)
    {
        std::destroy_n(b->items(), b->size);
        ::operator delete(b);
    }

    struct seq {
        block* b{nullptr};
        std::size_t offset{0};
        std::size_t length{0};
        seq() = default;
        seq(block* blk, std::size_t off, std::size_t len): b(blk), offset(off), length(len) {}
        seq(const seq& that): b(that.b), offset(that.offset), length(that.length)
        {
            if (b) ++b->refs;
        }
        seq(seq&& that) noexcept:
            b(std::exchange(that.b, nullptr)), offset(that.offset), length(that.length) {}
        seq& operator=(seq that) noexcept
        {
            std::swap(b, that.b);
            offset = that.offset;
            length = that.length;
            return *this;
        }
        ~seq() { if (b and 0 == --b->refs) release(b); }
        bool unique_and_whole() const
        {
            return b and 1 == b->refs and 0 == offset and length == b->size;
        }
        T* data() const { return b ? b->items() + offset : nullptr; }
    };

    static constexpr bool linear_rest{false};
    static constexpr bool linear_front{true};
    static constexpr bool linear_index{false};
    static constexpr bool linear_shared_update{true};
    static constexpr bool linear_unique_update{false};
    static constexpr bool linear_persistent_growth{true};
    static constexpr bool grows_at_front{false};

    // A new array holding a's elements and then b's, with room for capacity.
    static seq copy(const T* a, std::size_t a_length, const T* b, std::size_t b_length,
        std::size_t capacity)
    {
        block* blk{allocate(capacity)};
        std::uninitialized_copy_n(a, a_length, blk->items());
        std::uninitialized_copy_n(b, b_length, blk->items() + a_length);
        blk->size = a_length + b_length;
        return seq(blk, 0, blk->size);
    }
    // Make room for more elements at the end of a unique array.
    static void reserve(seq& s, std::size_t needed)
    {
        if (needed <= s.b->capacity) return;
        block* blk{allocate(std::max(needed, 2 * s.b->capacity))};
        std::uninitialized_move_n(s.b->items(), s.b->size, blk->items());
        blk->size = s.b->size;
        release(s.b);
        s.b = blk;
    }
    static seq build(const T* first, std::size_t n) { return copy(first, n, nullptr, 0, n); }
    static seq push_back(seq&& s, T e)
    {
        if (not s.unique_and_whole()) {
            seq t{copy(s.data(), s.length, nullptr, 0, std::max<std::size_t>(4, 2 * s.length))};
            return push_back(std::move(t), std::move(e));
        }
        reserve(s, s.length + 1);
        new (s.b->items() + s.length) T(std::move(e));
        ++s.b->size;
        ++s.length;
        return std::move(s);
    }
    static seq build_incrementally(const T* first, std::size_t n)
    {
        seq s;
        for (std::size_t i{0}; i < n; ++i) s = push_back(std::move(s), first[i]);
        return s;
    }
    static seq push_front(const seq& s, T e) { return copy(&e, 1, s.data(), s.length, s.length + 1); }
    static seq grow(const seq& s, T e)
    {
        seq t{copy(s.data(), s.length, nullptr, 0, s.length + 1)};
        return push_back(std::move(t), std::move(e));
    }
    static bool is_empty(const seq& s) { return 0 == s.length; }
    static const T& first(const seq& s) { return s.data()[0]; }
    static seq rest(const seq& s)
    {
        ++s.b->refs;
        return seq(s.b, s.offset + 1, s.length - 1);
    }
    static std::size_t size(const seq& s) { return s.length; }
    static const T& nth(const seq& s, std::size_t i) { return s.data()[i]; }
    static seq set(const seq& s, std::size_t i, T e)
    {
        seq t{copy(s.data(), s.length, nullptr, 0, s.length)};
        t.data()[i] = std::move(e);
        return t;
    }
    static seq set_unique(seq&& s, std::size_t i, T e)
    {
        if (s.unique_and_whole()) {
            s.data()[i] = std::move(e);
            return std::move(s);
        }
        return set(s, i, std::move(e));
    }
    static seq concat(const seq& a, const seq& b)
    {
        return copy(a.data(), a.length, b.data(), b.length, a.length + b.length);
    }
    static seq append_in_place(seq&& a, const seq& b)
    {
        if (not a.unique_and_whole()) {
            return copy(a.data(), a.length, b.data(), b.length, 2 * (a.length + b.length));
        }
        reserve(a, a.length + b.length);
        std::uninitialized_copy_n(b.data(), b.length, a.b->items() + a.length);
        a.b->size += b.length;
        a.length += b.length;
        return std::move(a);
    }
    static seq slice(const seq& s, std::size_t from, std::size_t to)
    {
        ++s.b->refs;
        return seq(s.b, s.offset + from, to - from);
    }
    template<typename F>
    static void for_each(const seq& s, F f)
    {
        for (std::size_t i{0}; i < s.length; ++i) f(s.data()[i]);
    }
    template<typename F>
    static void for_each_from(const seq& s, std::size_t start, F f)
    {
        for (std::size_t i{start}; i < s.length; ++i) f(s.data()[i]);
    }
};

// immer's vectors, with non-atomic counts. By default their nodes come from
// malloc, like the other structures'; immer's default policy keeps free
// lists.
using malloc_policy = immer::memory_policy<immer::heap_policy<immer::cpp_heap>,
    immer::unsafe_refcount_policy, immer::no_lock_policy>;
using free_list_policy = immer::memory_policy<immer::unsafe_free_list_heap_policy<immer::cpp_heap>,
    immer::unsafe_refcount_policy, immer::no_lock_policy>;

template<typename V>
struct immer_common {
    using seq = V;
    using T = typename V::value_type;
    static seq build(const T* first, std::size_t n) { return seq(first, first + n); }
    static seq build_incrementally(const T* first, std::size_t n)
    {
        seq s;
        for (std::size_t i{0}; i < n; ++i) s = std::move(s).push_back(first[i]);
        return s;
    }
    static seq grow(const seq& s, T e) { return s.push_back(std::move(e)); }
    static bool is_empty(const seq& s) { return s.empty(); }
    static const T& first(const seq& s) { return s.front(); }
    static std::size_t size(const seq& s) { return s.size(); }
    static const T& nth(const seq& s, std::size_t i) { return s[i]; }
    static seq set(const seq& s, std::size_t i, T e) { return s.set(i, std::move(e)); }
    static seq set_unique(seq&& s, std::size_t i, T e) { return std::move(s).set(i, std::move(e)); }
    template<typename F>
    static void for_each(const seq& s, F f)
    {
        immer::for_each_chunk(s, [&](const T* b, const T* e) { for (; b != e; ++b) f(*b); });
    }
    template<typename F>
    static void for_each_from(const seq& s, std::size_t start, F f)
    {
        std::for_each(s.begin() + start, s.end(), f);
    }
};

// The persistent trie, as Clojure's vector.
template<typename T>
struct trie : immer_common<immer::vector<T, malloc_policy>> {
    using base = immer_common<immer::vector<T, malloc_policy>>;
    using typename base::seq;

    static constexpr bool linear_rest{true};
    static constexpr bool linear_front{true};
    static constexpr bool linear_index{false};
    static constexpr bool linear_shared_update{false};
    static constexpr bool linear_unique_update{false};
    static constexpr bool linear_persistent_growth{false};
    static constexpr bool grows_at_front{false};

    static seq copy_range(const seq& s, std::size_t from, std::size_t to)
    {
        auto t{seq{}.transient()};
        std::for_each(s.begin() + from, s.begin() + to, [&](const T& e) { t.push_back(e); });
        return t.persistent();
    }
    static seq push_front(const seq& s, T e)
    {
        auto t{seq{}.transient()};
        t.push_back(std::move(e));
        base::for_each(s, [&](const T& x) { t.push_back(x); });
        return t.persistent();
    }
    static seq rest(const seq& s) { return copy_range(s, 1, s.size()); }
    static seq concat(const seq& a, const seq& b)
    {
        auto t{a.transient()};
        base::for_each(b, [&](const T& x) { t.push_back(x); });
        return t.persistent();
    }
    static seq append_in_place(seq&& a, const seq& b)
    {
        auto t{std::move(a).transient()};
        base::for_each(b, [&](const T& x) { t.push_back(x); });
        return t.persistent();
    }
    static seq slice(const seq& s, std::size_t from, std::size_t to) { return copy_range(s, from, to); }
};

// The RRB tree.
template<typename T, typename Policy>
struct rrb_tree : immer_common<immer::flex_vector<T, Policy>> {
    using base = immer_common<immer::flex_vector<T, Policy>>;
    using typename base::seq;

    static constexpr bool linear_rest{false};
    static constexpr bool linear_front{false};
    static constexpr bool linear_index{false};
    static constexpr bool linear_shared_update{false};
    static constexpr bool linear_unique_update{false};
    static constexpr bool linear_persistent_growth{false};
    static constexpr bool grows_at_front{false};

    static seq push_front(const seq& s, T e) { return s.push_front(std::move(e)); }
    static seq rest(const seq& s) { return s.drop(1); }
    static seq concat(const seq& a, const seq& b) { return a + b; }
    static seq append_in_place(seq&& a, const seq& b) { return std::move(a) + b; }
    static seq slice(const seq& s, std::size_t from, std::size_t to) { return s.drop(from).take(to - from); }
};

template<typename T> using rrb = rrb_tree<T, malloc_policy>;
template<typename T> using rrb_pooled = rrb_tree<T, free_list_policy>;

// Measuring

bool callgrind_mode{false};

// Runs prepare, untimed, and then run on what it returns, and reports run's
// cost divided by units. The state is freed after the measurement, so what
// run leaves in it isn't counted, but freeing anything run creates and drops
// is.
template<typename Prepare, typename Run>
void measure(const std::string& structure, const std::string& workload,
    std::size_t n, double units, Prepare prepare, Run run)
{
    auto label{std::format("{}|{}|{}", structure, workload, n)};
    if (callgrind_mode) {
        auto state{prepare()};
        auto before{allocation_count};
        CALLGRIND_ZERO_STATS;
        run(state);
        CALLGRIND_DUMP_STATS_AT(label.c_str());
        auto allocations{allocation_count - before};
        std::println("{}\t{}\t{}", label, units, allocations / units);
    } else {
        double best{1e300};
        for (int i{0}; i < 7; ++i) {
            auto state{prepare()};
            auto start{std::chrono::steady_clock::now()};
            run(state);
            auto stop{std::chrono::steady_clock::now()};
            best = std::min(best, std::chrono::duration<double, std::nano>(stop - start).count());
        }
        std::println("{}\t{:.2f}", label, best / units);
    }
    std::fflush(stdout);
}

const std::vector<std::size_t> sizes{1, 2, 3, 4, 8, 16, 32, 33, 1000, 1000000};

std::size_t repetitions(std::size_t n) { return std::max<std::size_t>(1, 65536 / n); }

// The lengths of the lists in src/lib.noeval and tests/*.noeval (see
// forms.noeval), as (length, count).
const std::vector<std::pair<std::size_t, std::size_t>> form_lengths{
    {1, 550}, {2, 3029}, {3, 7046}, {4, 634}, {5, 163}, {6, 57}, {7, 29},
    {8, 12}, {9, 1}, {10, 4}, {11, 2}, {13, 1},
};

template<template<typename> class S>
void run_element_workloads(const std::string& name, const pool& p)
{
    using A = S<element>;
    using seq = typename A::seq;
    const element* source{p.elements.data()};
    auto big{[](bool linear, std::size_t n) { return linear and n > 1000; }};

    for (auto n: sizes) {
        auto k{repetitions(n)};
        auto nothing{[] { return 0; }};

        measure(name, "build", n, double(k) * n, nothing, [&](int) {
            for (std::size_t r{0}; r < k; ++r) keep(A::build(source + r % 16, n));
        });
        measure(name, "build incrementally", n, double(k) * n, nothing, [&](int) {
            for (std::size_t r{0}; r < k; ++r) keep(A::build_incrementally(source + r % 16, n));
        });
        if (not big(A::linear_front, n)) {
            measure(name, "prepend", n, double(k) * n, nothing, [&](int) {
                for (std::size_t r{0}; r < k; ++r) {
                    seq s;
                    for (std::size_t i{0}; i < n; ++i) s = A::push_front(s, source[i]);
                    keep(s);
                }
            });
        }
        if (not big(A::linear_persistent_growth, n)) {
            // Grow at the structure's natural end while the previous version
            // is still referenced, so nothing can be updated in place.
            measure(name, "persistent growth", n, double(k) * n, nothing, [&](int) {
                for (std::size_t r{0}; r < k; ++r) {
                    seq s;
                    for (std::size_t i{0}; i < n; ++i) {
                        seq previous{s};
                        s = A::grow(previous, source[i]);
                    }
                    keep(s);
                }
            });
        }

        auto built{[&] { return A::build(source, n); }};
        if (not big(A::linear_rest, n)) {
            // A recursive walk with first and rest. The whole sequence stays
            // referenced by its owner, and each step's is held while taking
            // its rest.
            measure(name, "walk with rest", n, double(k) * n, built, [&](const seq& whole) {
                for (std::size_t r{0}; r < k; ++r) {
                    long sum{0};
                    seq s{whole};
                    while (not A::is_empty(s)) {
                        sum += number_of(A::first(s));
                        s = A::rest(s);
                    }
                    keep(sum);
                }
            });
        }
        measure(name, "reduce", n, double(k) * n, built, [&](const seq& s) {
            for (std::size_t r{0}; r < k; ++r) {
                long sum{0};
                A::for_each(s, [&](const element& e) { sum += number_of(e); });
                keep(sum);
            }
        });
        // Reduce without reading the elements' objects, which measures the
        // structure's own locality.
        measure(name, "reduce spine", n, double(k) * n, built, [&](const seq& s) {
            for (std::size_t r{0}; r < k; ++r) {
                std::uintptr_t sum{0};
                A::for_each(s, [&](const element& e) { sum += address_of(e); });
                keep(sum);
            }
        });
        measure(name, "length", n, double(k), built, [&](const seq& s) {
            for (std::size_t r{0}; r < k; ++r) keep(A::size(s));
        });

        std::mt19937_64 rng{2};
        std::vector<std::size_t> indices(10000);
        for (auto& i: indices) i = rng() % n;
        if (not big(A::linear_index, n)) {
            measure(name, "index", n, indices.size(), built, [&](const seq& s) {
                long sum{0};
                for (auto i: indices) sum += number_of(A::nth(s, i));
                keep(sum);
            });
        }
        constexpr std::size_t updates{1000};
        if (not big(A::linear_shared_update, n)) {
            measure(name, "update shared", n, updates, built, [&](const seq& s) {
                for (std::size_t j{0}; j < updates; ++j) keep(A::set(s, indices[j], source[j]));
            });
        }
        if (not big(A::linear_unique_update, n)) {
            measure(name, "update unique", n, updates, built, [&](seq& s) {
                seq t{std::move(s)};
                for (std::size_t j{0}; j < updates; ++j) {
                    t = A::set_unique(std::move(t), indices[j], source[j]);
                }
                // Hand the sequence back, so freeing it isn't counted.
                s = std::move(t);
            });
        }
        auto two{[&] { return std::pair{A::build(source, n), A::build(source + n % 7, n)}; }};
        measure(name, "concatenate", n, double(k), two, [&](const std::pair<seq, seq>& ab) {
            for (std::size_t r{0}; r < k; ++r) keep(A::concat(ab.first, ab.second));
        });
        if (n >= 4) {
            measure(name, "slice", n, double(k), built, [&](const seq& s) {
                for (std::size_t r{0}; r < k; ++r) keep(A::slice(s, n / 4, n / 4 + n / 2));
            });
        }
    }

    // Code-shaped work: forms with the lengths of the library's and tests'
    // lists. The parser builds them, the evaluator takes each apart (its
    // first element, then its operands in order), and a macro rebuilds it
    // (a new head on the old operands).
    std::vector<std::size_t> lengths;
    for (auto [length, count]: form_lengths) lengths.insert(lengths.end(), count, length);
    std::shuffle(lengths.begin(), lengths.end(), std::mt19937_64{3});
    auto forms{lengths.size()};
    auto parsed{[&] {
        std::vector<seq> v;
        std::size_t at{0};
        for (auto length: lengths) {
            v.push_back(A::build(source + at, length));
            at += length;
        }
        return v;
    }};
    measure(name, "code: parse", 0, forms, [] { return 0; }, [&](int) {
        std::vector<seq> v;
        v.reserve(forms);
        std::size_t at{0};
        for (auto length: lengths) {
            v.push_back(A::build(source + at, length));
            at += length;
        }
        keep(v);
    });
    measure(name, "code: evaluate", 0, forms, parsed, [&](const std::vector<seq>& v) {
        long sum{0};
        for (const auto& form: v) {
            sum += number_of(A::first(form));
            A::for_each_from(form, 1, [&](const element& e) { sum += number_of(e); });
        }
        keep(sum);
    });
    measure(name, "code: expand", 0, forms, parsed, [&](const std::vector<seq>& v) {
        std::vector<seq> expanded;
        expanded.reserve(forms);
        for (const auto& form: v) expanded.push_back(A::push_front(A::rest(form), source[0]));
        keep(expanded);
    });
}

template<template<typename> class S>
void run_text_workloads(const std::string& name, const pool& p)
{
    using A = S<std::uint8_t>;
    using seq = typename A::seq;
    const std::uint8_t* source{p.bytes.data()};

    // Concatenate many short strings into a long one, then take short slices
    // of it at random and sum their bytes.
    std::mt19937_64 rng{4};
    std::vector<std::size_t> chunk_lengths(5000);
    for (auto& l: chunk_lengths) l = 1 + rng() % 40;
    auto total{std::accumulate(chunk_lengths.begin(), chunk_lengths.end(), std::size_t{0})};
    auto chunks{[&] {
        std::vector<seq> v;
        std::size_t at{0};
        for (auto l: chunk_lengths) {
            v.push_back(A::build(source + at, l));
            at += l;
        }
        return v;
    }};
    measure(name, "text: concatenate", 0, total, chunks, [&](const std::vector<seq>& v) {
        if constexpr (A::grows_at_front) {
            // A cons list is built from the end, so each step copies only
            // the short string.
            seq acc;
            for (std::size_t i{v.size()}; i > 0; --i) acc = A::concat(v[i - 1], acc);
            keep(acc);
        } else {
            seq acc;
            for (const auto& c: v) acc = A::append_in_place(std::move(acc), c);
            keep(acc);
        }
    });
    constexpr std::size_t slices{200};
    std::vector<std::pair<std::size_t, std::size_t>> ranges;
    for (std::size_t i{0}; i < slices; ++i) {
        auto length{10 + rng() % 91};
        auto from{rng() % (total - length)};
        ranges.emplace_back(from, from + length);
    }
    measure(name, "text: slice", 0, slices, [&] { return A::build(source, total); }, [&](const seq& s) {
        long sum{0};
        for (auto [from, to]: ranges) {
            A::for_each(A::slice(s, from, to), [&](std::uint8_t b) { sum += b; });
        }
        keep(sum);
    });
}

// What Noeval allocates for each value: a 160-byte object and, separately,
// its shared_ptr control block. A sequence operation that returns a new
// sequence (rest on anything but a cons list, for example) would cost this
// too, when the sequence is wrapped in a value.
void run_baseline()
{
    struct fake_value {
        char bytes[160];
    };
    std::size_t k{65536};
    measure("baseline", "value allocation", 0, k, [] { return 0; }, [&](int) {
        for (std::size_t r{0}; r < k; ++r) keep(std::shared_ptr<fake_value>(new fake_value));
    });
}

// The bytes the structure itself allocates per element (not the elements'
// objects), as malloc_usable_size counts them. Run in a fresh process, before
// anything is freed, since immer keeps freed nodes on a free list.
template<template<typename> class S>
void run_memory(const std::string& name, const pool& p)
{
    // Every sequence built is kept until the end, so none of the memory
    // measured comes from a free list.
    std::vector<std::vector<typename S<element>::seq>> kept;
    for (std::size_t n: {1, 2, 3, 4, 8, 32, 33, 1000, 1000000}) {
        auto k{repetitions(n)};
        auto& v{kept.emplace_back()};
        v.reserve(k);
        auto before{live_bytes};
        for (std::size_t r{0}; r < k; ++r) v.push_back(S<element>::build(p.elements.data(), n));
        std::println("{}|memory|{}\t{:.2f}", name, n, double(live_bytes - before) / (k * n));
    }
}

template<template<typename> class S>
void run_structure(const std::string& mode, const std::string& name, const pool& p)
{
    if ("memory" == mode) {
        run_memory<S>(name, p);
    } else {
        run_element_workloads<S>(name, p);
        run_text_workloads<S>(name, p);
    }
}

} // namespace

int main(int argc, char** argv)
{
    std::string mode{argc > 1 ? argv[1] : "time"};
    std::string only{argc > 2 ? argv[2] : ""};
    callgrind_mode = "callgrind" == mode;
    count_bytes = "memory" == mode;
    // The pool is never freed: its payloads outlive every sequence.
    const pool& p{*new pool{pool_size}};

    auto want{[&](const std::string& name) { return only.empty() or only == name; }};
    if (want("baseline") and "memory" != mode) run_baseline();
    if (want("cons (shared_ptr)")) run_structure<cons_shared>(mode, "cons (shared_ptr)", p);
    if (want("cons (intrusive)")) run_structure<cons_intrusive>(mode, "cons (intrusive)", p);
    if (want("cons (free list)")) run_structure<cons_pooled>(mode, "cons (free list)", p);
    if (want("flat array")) run_structure<flat_array>(mode, "flat array", p);
    if (want("trie")) run_structure<trie>(mode, "trie", p);
    if (want("rrb")) run_structure<rrb>(mode, "rrb", p);
    if (want("rrb (free list)")) run_structure<rrb_pooled>(mode, "rrb (free list)", p);
}
