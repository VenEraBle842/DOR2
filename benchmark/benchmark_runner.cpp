#include "benchmark/benchmark_runner.hpp"

#include <cstdlib>
#include <iostream>

int main() {
    const auto result = BenchmarkRunner::runAndSave("benchmark_results.csv", 1000, 200, 100);
    if (!result) {
        std::cerr << formatError(result.error()) << '\n';
        return EXIT_FAILURE;
    }
    for (const BenchmarkResult& row : *result) {
        if (!row.verified) {
            std::cerr << "Benchmark verification failed; inspect benchmark_results.csv\n";
            return EXIT_FAILURE;
        }
    }
    std::cout << "Wrote " << result->size()
              << " verified benchmark rows to benchmark_results.csv\n";
    return EXIT_SUCCESS;
}
