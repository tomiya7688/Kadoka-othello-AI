#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include "kadoka_othello/headless.hpp"
#include "kadoka_othello/package.hpp"
#include "kadoka_othello/package_loader.hpp"

namespace {

void print_histogram(const kadoka::othello::HeadlessSummary& summary) {
    std::cout << "invalid_histogram=";
    bool first = true;
    for (std::size_t invalid = 0; invalid < summary.invalid_attempt_histogram.size(); ++invalid) {
        const std::size_t turns = summary.invalid_attempt_histogram[invalid];
        if (turns == 0) continue;
        if (!first) std::cout << ',';
        std::cout << invalid << ':' << turns;
        first = false;
    }
    if (first) std::cout << "none";
    std::cout << '\n';
}

}  // namespace

int main(int argc, char** argv) {
    using namespace kadoka::othello;

    if (argc < 5 || argc > 6) {
        std::cerr
            << "usage: kadoka_headless_benchmark <games> <board-size> "
            << "<black-manifest> <white-manifest> [seed]\n";
        return 2;
    }

    try {
        HeadlessConfig config;
        config.games = static_cast<std::size_t>(std::strtoull(argv[1], nullptr, 10));
        config.board_size = static_cast<std::size_t>(std::strtoull(argv[2], nullptr, 10));
        config.seed = argc == 6 ? std::strtoull(argv[5], nullptr, 10) : 12345;
        config.write_json_lines = false;
        config.collect_metrics = true;

        if (config.games == 0) {
            throw std::invalid_argument("games must be greater than zero");
        }

        LoadedAIPackage black = load_ai_package(
            load_ai_manifest(argv[3]),
            config.seed == 0 ? 1 : config.seed);
        LoadedAIPackage white = load_ai_package(
            load_ai_manifest(argv[4]),
            config.seed == 0 ? 2 : config.seed + 1);

        const auto start = std::chrono::steady_clock::now();
        const HeadlessSummary summary = run_games(
            config,
            black.view(),
            white.view(),
            nullptr);
        const auto end = std::chrono::steady_clock::now();

        const double elapsed_us = std::chrono::duration<double, std::micro>(end - start).count();
        const double games_per_sec = elapsed_us > 0.0
            ? static_cast<double>(summary.games) * 1000000.0 / elapsed_us
            : 0.0;
        const double average_think_us = summary.ai_calls > 0
            ? summary.total_ai_think_us / static_cast<double>(summary.ai_calls)
            : 0.0;
        const double average_invalid_per_turn = summary.turns > 0
            ? static_cast<double>(summary.invalid_move_attempts) /
                  static_cast<double>(summary.turns)
            : 0.0;

        std::cout << "black=" << black.manifest.id << '\n';
        std::cout << "white=" << white.manifest.id << '\n';
        std::cout << "board_size=" << config.board_size << '\n';
        std::cout << "games=" << summary.games << '\n';
        std::cout << "elapsed_us=" << elapsed_us << '\n';
        std::cout << "games_per_sec=" << games_per_sec << '\n';
        std::cout << "turns=" << summary.turns << '\n';
        std::cout << "ai_calls=" << summary.ai_calls << '\n';
        std::cout << "average_think_us=" << average_think_us << '\n';
        std::cout << "max_think_us=" << summary.max_ai_think_us << '\n';
        std::cout << "invalid_attempts=" << summary.invalid_move_attempts << '\n';
        std::cout << "turns_with_invalid_attempts=" << summary.turns_with_invalid_attempts << '\n';
        std::cout << "average_invalid_per_turn=" << average_invalid_per_turn << '\n';
        std::cout << "max_invalid_per_turn=" << summary.max_invalid_attempts_in_turn << '\n';
        print_histogram(summary);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }

    return 0;
}
