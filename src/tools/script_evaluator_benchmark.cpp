#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "kadoka_othello/script_evaluator.hpp"

namespace {

void run_one(const std::string& config_path, std::size_t repeat) {
    using namespace kadoka::othello;

    ScriptEvaluator evaluator(load_script_evaluator_config(config_path));
    const std::vector<ScriptEvaluatorCase> cases{
        ScriptEvaluatorCase{
            0,
            {
                {"memory_bonus", 2.0},
                {"recall_delay", 0.4},
            },
        },
    };

    const auto start = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < repeat; ++i) {
        const auto results = evaluator.evaluate_batch(cases);
        if (results.size() != 1 || results.front().id != 0) {
            throw std::runtime_error("benchmark evaluator returned invalid result");
        }
    }
    const auto end = std::chrono::steady_clock::now();
    const double total_us = std::chrono::duration<double, std::micro>(end - start).count();
    const double average_us = repeat == 0 ? 0.0 : total_us / static_cast<double>(repeat);

    std::cout << "runtime=" << script_evaluator_runtime_name(evaluator.config().runtime)
              << " repeat=" << repeat
              << " total_us=" << total_us
              << " average_us=" << average_us << '\n';
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 5) {
        std::cerr << "usage: kadoka_script_evaluator_benchmark <repeat> <python-config> <native-process-config> <native-in-process-config>\n";
        return 2;
    }

    try {
        const std::size_t repeat = static_cast<std::size_t>(std::strtoull(argv[1], nullptr, 10));
        if (repeat == 0) throw std::invalid_argument("repeat must be greater than zero");
        run_one(argv[2], repeat);
        run_one(argv[3], repeat);
        run_one(argv[4], repeat);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }

    return 0;
}
