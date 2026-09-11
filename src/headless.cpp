#include "kadoka_othello/headless.hpp"

#include <ostream>
#include <stdexcept>

namespace kadoka::othello {

namespace {

AIPackage package_for_player(
    Player player,
    AIPackage black,
    AIPackage white) {
    return player == Player::Black ? black : white;
}

}  // namespace

HeadlessSummary run_games(
    const HeadlessConfig& config,
    AIPackage black,
    AIPackage white,
    std::ostream* dataset_output) {
    if (config.games == 0) {
        return {};
    }

    if (config.board_size < 4 || config.board_size % 2 != 0) {
        throw std::invalid_argument("board size must be an even number greater than or equal to 4");
    }

    if (config.max_invalid_attempts_per_turn == 0) {
        throw std::invalid_argument("max_invalid_attempts_per_turn must be greater than zero");
    }

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

            const AIInput input{&game.board(), &moves};
            const AIPackage current = package_for_player(
                game.current_player(),
                black,
                white);

            bool played = false;
            for (std::size_t attempt = 0;
                 attempt < config.max_invalid_attempts_per_turn;
                 ++attempt) {
                const AIOutput output = invoke_ai(current, input);
                if (game.play(output.move)) {
                    played = true;
                    break;
                }
                ++summary.invalid_move_attempts;
            }

            if (!played) {
                throw std::runtime_error("AI exceeded invalid move retry limit");
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

HeadlessSummary run_random_games(
    const HeadlessConfig& config,
    std::ostream* dataset_output) {
    RandomAI black_ai(config.seed == 0 ? 0 : config.seed);
    RandomAI white_ai(config.seed == 0 ? 0 : config.seed + 1);
    PassThroughAdapter adapter;

    return run_games(
        config,
        AIPackage{&black_ai, &adapter},
        AIPackage{&white_ai, &adapter},
        dataset_output);
}

}  // namespace kadoka::othello
