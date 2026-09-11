#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

#include "kadoka_othello/headless.hpp"

int main(int argc, char** argv) {
    kadoka::othello::HeadlessConfig config;
    std::string output_path;

    if (argc >= 2) config.games = static_cast<std::size_t>(std::strtoull(argv[1], nullptr, 10));
    if (argc >= 3) config.board_size = static_cast<std::size_t>(std::strtoull(argv[2], nullptr, 10));
    if (argc >= 4) output_path = argv[3];
    if (argc >= 5) config.seed = std::strtoull(argv[4], nullptr, 10);

    std::ofstream output;
    std::ostream* dataset_stream = nullptr;
    if (!output_path.empty()) {
        output.open(output_path, std::ios::out | std::ios::trunc);
        if (!output) {
            std::cerr << "failed to open dataset output: " << output_path << '\n';
            return 1;
        }
        dataset_stream = &output;
    }

    try {
        const auto summary = kadoka::othello::run_random_games(config, dataset_stream);
        std::cout << "games=" << summary.games
                  << " black_wins=" << summary.black_wins
                  << " white_wins=" << summary.white_wins
                  << " draws=" << summary.draws << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }

    return 0;
}
