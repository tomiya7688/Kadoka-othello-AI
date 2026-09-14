#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include "kadoka_othello/script_evaluator.hpp"

using namespace kadoka::othello;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout
            << "usage: kadoka_script_evaluator_probe <evaluator.json> [--repeat N] [key=value ...]\n";
        return 0;
    }

    try {
        ScriptEvaluator evaluator(load_script_evaluator_config(argv[1]));
        ScriptEvaluatorCase item;
        item.id = 0;

        std::size_t repeat = 1;
        int argument = 2;
        if (argument < argc && std::string(argv[argument]) == "--repeat") {
            if (argument + 1 >= argc) {
                throw std::invalid_argument("--repeat requires a positive integer");
            }
            repeat = static_cast<std::size_t>(std::stoull(argv[argument + 1]));
            if (repeat == 0) {
                throw std::invalid_argument("--repeat must be greater than zero");
            }
            argument += 2;
        }

        for (int i = argument; i < argc; ++i) {
            const std::string token = argv[i];
            const std::size_t equals = token.find('=');
            if (equals == std::string::npos) {
                throw std::invalid_argument("feature must be key=value: " + token);
            }
            item.features.push_back({
                token.substr(0, equals),
                std::stod(token.substr(equals + 1)),
            });
        }

        std::vector<ScriptEvaluatorResult> results;
        const auto start = std::chrono::steady_clock::now();
        for (std::size_t i = 0; i < repeat; ++i) {
            results = evaluator.evaluate_batch({item});
        }
        const auto end = std::chrono::steady_clock::now();
        const double total_us =
            std::chrono::duration<double, std::micro>(end - start).count();

        for (const auto& value : results.front().values) {
            std::cout << value.key << '=' << value.value << '\n';
        }
        for (const auto& diag : results.front().diagnostics) {
            std::cout << "diag." << diag.first << '=' << diag.second << '\n';
        }
        std::cout << "benchmark.repeat=" << repeat << '\n';
        std::cout << "benchmark.total_us=" << total_us << '\n';
        std::cout << "benchmark.average_us=" << (total_us / static_cast<double>(repeat)) << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "script evaluator error: " << error.what() << '\n';
        return 1;
    }
}
