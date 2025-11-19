#include <cstdio>
#include <cstring>
#include <vector>
#include <random>
#include <chrono>

#include "Allocator.hpp"

int main() {
    const std::size_t POOL_SIZE = 200 * 1024 * 1024; // 10 MB
    const int DURATION_MS = 3000; // 3000 ms

    // ==============================
    // TLSF Allocator Benchmark
    // ==============================
    {
        printf("\n=== TLSF Allocator ===\n");
        printf("Initializing TLSF allocator with %zu MB pool...\n", POOL_SIZE / (1024 * 1024));

        TLSF::Allocator alloc(POOL_SIZE);

        std::mt19937 rng(12345);
        std::uniform_int_distribution<std::size_t> size_dist(1, 16384); // 1B ～ 16KB
        std::uniform_int_distribution<int> action_dist(0, 99);

        std::vector<void*> allocated;
        allocated.reserve(10000);

        auto start = std::chrono::steady_clock::now();
        uint64_t ops = 0;

        printf("Running benchmark for %d ms...\n", DURATION_MS);

        while (true) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
            if (elapsed.count() >= DURATION_MS) break;

            ops++;

            if (!allocated.empty() && action_dist(rng) < 30) {
                void* p = allocated.back();
                allocated.pop_back();
                alloc.deallocate(p);
            }
            else {
                std::size_t sz = size_dist(rng);
                void* p = alloc.allocate(sz);
                if (p) {
                    memset(p, 0xAB, std::min(sz, static_cast<std::size_t>(1024)));
                    allocated.push_back(p);
                }
                else {
                    int n = std::min(10, (int)allocated.size());
                    for (int i = 0; i < n; ++i) {
                        alloc.deallocate(allocated.back());
                        allocated.pop_back();
                    }
                }
            }

            if (allocated.size() > 9000) {
                for (void* p : allocated) alloc.deallocate(p);
                allocated.clear();
            }
        }

        for (void* p : allocated) alloc.deallocate(p);

        auto end = std::chrono::steady_clock::now();
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        printf("Benchmark completed.\n");
        printf(" - Time: %lld ms\n", (long long)elapsed_ms);
        printf(" - Total ops: %llu\n", (unsigned long long)ops);
        printf(" - Avg ops/sec: %.2f\n", (double)ops / (elapsed_ms / 1000.0));
    }

    // ==============================
    // malloc / free Benchmark
    // ==============================
    {
        printf("\n=== Standard malloc/free ===\n");
        printf("Using system malloc (no pool)...\n");

        std::mt19937 rng(12345);
        std::uniform_int_distribution<std::size_t> size_dist(1, 16384); // 1B ～ 16KB
        std::uniform_int_distribution<int> action_dist(0, 99);

        std::vector<void*> allocated;
        allocated.reserve(10000);

        auto start = std::chrono::steady_clock::now();
        uint64_t ops = 0;

        printf("Running benchmark for %d ms...\n", DURATION_MS);

        while (true) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
            if (elapsed.count() >= DURATION_MS) break;

            ops++;

            if (!allocated.empty() && action_dist(rng) < 30) {
                void* p = allocated.back();
                allocated.pop_back();
                std::free(p);
            }
            else {
                std::size_t sz = size_dist(rng);
                void* p = std::malloc(sz);
                if (p) {
                    memset(p, 0xAB, std::min(sz, static_cast<std::size_t>(1024)));
                    allocated.push_back(p);
                }
            }

            if (allocated.size() > 9000) {
                for (void* p : allocated) std::free(p);
                allocated.clear();
            }
        }

        for (void* p : allocated) std::free(p);

        auto end = std::chrono::steady_clock::now();
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        printf("Benchmark completed.\n");
        printf(" - Time: %lld ms\n", (long long)elapsed_ms);
        printf(" - Total ops: %llu\n", (unsigned long long)ops);
        printf(" - Avg ops/sec: %.2f\n", (double)ops / (elapsed_ms / 1000.0));
    }

    printf("\nAll tests completed successfully.\n");
    return 0;

}
