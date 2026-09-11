#include "kadoka_othello/headless.hpp"

#include <ostream>
#include <random>
#include <stdexcept>

namespace kadoka::othello {

HeadlessSummary run_random_games(
    const HeadlessConfig& config,
    std::ostream* dataset_output) {
    if (config.games == 0) {
        return {};
    }

    if (config.board_size < 4 || config.board_size % 2 != 0) {
        throw std::invalid_argument("board size must be an even number greater than or equal to 4");
    }

    std::mt19937_64 rng(config.seed == 0 ? std::random_device{}() : config.seed);
    HeadlessSummary summary;
    summary.games = config.games;

    for (std::size_t game_index = 0; game_index < config.games; ++game_index) {
        Game game(config.board_size);

        while (game.status() == GameStatus::Playing) {
            const auto moves = game.legal_moves();
            if (moves.empty()) {
                if (!game.pass()) {
                    break;
                }
                continue;
            }

            std::uniform_int_distribution<std::size_t> pick(0, moves.size() - 1);
            if (!game.play(moves[pick(rng)])) {
                throw std::runtime_error("headless runner selected an invalid move");
            }

            if (dataset_output != nullptr && config.write_json_lines) {
                *dataset_output << snapshot_to_json(make_snapshot(game)) << '\n';
            }
        }

        const auto result = game.result();
        if (!result) {
            throw std::runtime_error("finished headless game has no result");
        }

        if (result->winner == Winner::Black) {
            ++summary.black_wins;
        } else if (result->winner == Winner::White) {
            ++summary.white_wins;
        } else {
            ++summary.draws;
        }
    }

    return summary;
}

}  // namespace kadoka::othello
