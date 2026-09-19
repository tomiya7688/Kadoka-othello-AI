#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

#include "kadoka_othello/headless.hpp"
#include "kadoka_othello/package.hpp"
#include "kadoka_othello/package_loader.hpp"

int main(int argc, char** argv) {
    using namespace kadoka::othello;

    HeadlessConfig config;
    std::string output_path;
    std::string black_manifest_path;
    std::string white_manifest_path;
    std::string board_state_path;
    std::string game_aux_path;

    if (argc >= 2) config.games = static_cast<std::size_t>(std::strtoull(argv[1], nullptr, 10));
    if (argc >= 3) config.board_size = static_cast<std::size_t>(std::strtoull(argv[2], nullptr, 10));
    if (argc >= 4 && std::string(argv[3]) != "-") output_path = argv[3];
    if (argc >= 5) config.seed = std::strtoull(argv[4], nullptr, 10);
    if (argc >= 6 && std::string(argv[5]) != "-") black_manifest_path = argv[5];
    if (argc >= 7 && std::string(argv[6]) != "-") white_manifest_path = argv[6];
    if (argc >= 8 && std::string(argv[7]) != "-") board_state_path = argv[7];
    if (argc >= 9 && std::string(argv[8]) != "-") game_aux_path = argv[8];

    if (board_state_path.empty() != game_aux_path.empty()) {
        std::cerr << "BoardState and GameAux record paths must be supplied together\n";
        return 2;
    }

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

    std::ofstream board_state_output;
    std::ofstream game_aux_output;
    std::ostream* board_state_stream = nullptr;
    std::ostream* game_aux_stream = nullptr;
    if (!board_state_path.empty()) {
        board_state_output.open(board_state_path, std::ios::out | std::ios::trunc);
        game_aux_output.open(game_aux_path, std::ios::out | std::ios::trunc);
        if (!board_state_output || !game_aux_output) {
            std::cerr << "failed to open Game Record output\n";
            return 1;
        }
        board_state_stream = &board_state_output;
        game_aux_stream = &game_aux_output;
    }

    try {
        HeadlessSummary summary;
        if (black_manifest_path.empty() && white_manifest_path.empty()) {
            summary = run_random_games(
                config,
                dataset_stream,
                board_state_stream,
                game_aux_stream);
        } else {
            const std::string default_random_manifest = "src/packages/random/manifest.json";
            if (black_manifest_path.empty()) black_manifest_path = default_random_manifest;
            if (white_manifest_path.empty()) white_manifest_path = default_random_manifest;

            LoadedAIPackage black = load_ai_package(
                load_ai_manifest(black_manifest_path),
                config.seed == 0 ? 1 : config.seed);
            LoadedAIPackage white = load_ai_package(
                load_ai_manifest(white_manifest_path),
                config.seed == 0 ? 2 : config.seed + 1);

            summary = run_games(
                config,
                black.view(),
                white.view(),
                dataset_stream,
                board_state_stream,
                game_aux_stream);
        }

        std::cout << "games=" << summary.games
                  << " black_wins=" << summary.black_wins
                  << " white_wins=" << summary.white_wins
                  << " draws=" << summary.draws
                  << " invalid_attempts=" << summary.invalid_move_attempts << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }

    return 0;
}
