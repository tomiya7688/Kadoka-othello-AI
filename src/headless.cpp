#include "kadoka_othello/headless.hpp"

#include "kadoka_othello/game_record.hpp"

#include <algorithm>
#include <chrono>
#include <memory>
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

AIOutput invoke_with_optional_timing(
    const HeadlessConfig& config,
    HeadlessSummary& summary,
    Player player,
    AIPackage package,
    const AIInput& input) {
    if (!config.collect_metrics) {
        return invoke_ai(package, input);
    }

    const auto start = std::chrono::steady_clock::now();
    const AIOutput output = invoke_ai(package, input);
    const auto end = std::chrono::steady_clock::now();
    const double elapsed_us = std::chrono::duration<double, std::micro>(end - start).count();

    ++summary.ai_calls;
    summary.total_ai_think_us += elapsed_us;
    if (elapsed_us > summary.max_ai_think_us) {
        summary.max_ai_think_us = elapsed_us;
    }

    HeadlessAIMetrics& player_metrics =
        player == Player::Black
            ? summary.black_ai_metrics
            : summary.white_ai_metrics;
    ++player_metrics.calls;
    player_metrics.total_think_us += elapsed_us;
    player_metrics.max_think_us =
        std::max(player_metrics.max_think_us, elapsed_us);

    if (output.metrics.nodes) {
        summary.total_nodes += *output.metrics.nodes;
        ++summary.node_reports;
        player_metrics.total_nodes += *output.metrics.nodes;
        ++player_metrics.node_reports;
    }
    if (output.metrics.simulations) {
        summary.total_simulations += *output.metrics.simulations;
        ++summary.simulation_reports;
        player_metrics.total_simulations += *output.metrics.simulations;
        ++player_metrics.simulation_reports;
    }
    if (output.metrics.depth) {
        summary.max_depth = std::max(summary.max_depth, *output.metrics.depth);
        ++summary.depth_reports;
        player_metrics.max_depth =
            std::max(player_metrics.max_depth, *output.metrics.depth);
        ++player_metrics.depth_reports;
    }
    if (output.metrics.search_effort) {
        summary.total_search_effort += *output.metrics.search_effort;
        ++summary.search_effort_reports;
        player_metrics.total_search_effort +=
            *output.metrics.search_effort;
        ++player_metrics.search_effort_reports;
    }
    return output;
}

void record_turn_metrics(
    const HeadlessConfig& config,
    HeadlessSummary& summary,
    std::size_t invalid_attempts) {
    ++summary.turns;
    if (!config.collect_metrics) return;

    if (invalid_attempts > 0) {
        ++summary.turns_with_invalid_attempts;
    }
    if (invalid_attempts > summary.max_invalid_attempts_in_turn) {
        summary.max_invalid_attempts_in_turn = invalid_attempts;
    }
    if (summary.invalid_attempt_histogram.size() <= invalid_attempts) {
        summary.invalid_attempt_histogram.resize(invalid_attempts + 1, 0);
    }
    ++summary.invalid_attempt_histogram[invalid_attempts];
}

}  // namespace

HeadlessSummary run_games(
    const HeadlessConfig& config,
    AIPackage black,
    AIPackage white,
    std::ostream* dataset_output,
    std::ostream* board_state_output,
    std::ostream* game_aux_output) {
    if (config.games == 0) {
        return {};
    }

    if (config.board_size < 4 || config.board_size % 2 != 0) {
        throw std::invalid_argument("board size must be an even number greater than or equal to 4");
    }

    if (config.max_invalid_attempts_per_turn == 0) {
        throw std::invalid_argument("max_invalid_attempts_per_turn must be greater than zero");
    }
    if ((board_state_output == nullptr) != (game_aux_output == nullptr)) {
        throw std::invalid_argument(
            "Headless Game Record requires both BoardState and GameAux outputs");
    }
    if (!config.record_game_id.empty() && config.games != 1) {
        throw std::invalid_argument(
            "explicit Headless record_game_id requires games == 1");
    }

    HeadlessSummary summary;
    summary.games = config.games;

    for (std::size_t game_index = 0; game_index < config.games; ++game_index) {
        Game game(config.board_size);
        static_cast<void>(game.add_event_listener([&summary](const GameEvent& event) {
            if (event.type == GameEventType::InvalidMove) {
                ++summary.invalid_move_attempts;
            }
        }));

        std::unique_ptr<GameRecordRecorder> recorder;
        if (board_state_output != nullptr) {
            recorder = std::make_unique<GameRecordRecorder>(
                game,
                config.record_game_id);
        }

        while (game.status() == GameStatus::Playing) {
            if (game.can_pass()) {
                if (!game.pass()) {
                    break;
                }
                continue;
            }

            const AIInput input{&game.board(), game.current_player(), {}};
            const AIPackage current = package_for_player(
                game.current_player(),
                black,
                white);

            bool played = false;
            std::size_t invalid_attempts_this_turn = 0;
            for (std::size_t attempt = 0;
                 attempt < config.max_invalid_attempts_per_turn;
                 ++attempt) {
                const AIOutput output = invoke_with_optional_timing(
                    config,
                    summary,
                    game.current_player(),
                    current,
                    input);
                if (game.play(output.move)) {
                    played = true;
                    break;
                }
                ++invalid_attempts_this_turn;
            }

            if (!played) {
                throw std::runtime_error("AI exceeded invalid move retry limit");
            }

            record_turn_metrics(config, summary, invalid_attempts_this_turn);

            if (dataset_output != nullptr && config.write_json_lines) {
                *dataset_output << snapshot_to_json(make_snapshot(game)) << '\n';
            }
        }

        if (recorder) {
            write_game_record_jsonl(
                recorder->record(),
                *board_state_output,
                *game_aux_output);
        }

        const auto result = game.result();
        if (!result) {
            throw std::runtime_error("finished headless game has no result");
        }

        summary.total_black_discs +=
            result->black_discs;
        summary.total_white_discs +=
            result->white_discs;
        summary.total_disc_difference +=
            static_cast<std::int64_t>(result->black_discs) -
            static_cast<std::int64_t>(result->white_discs);

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
    std::ostream* dataset_output,
    std::ostream* board_state_output,
    std::ostream* game_aux_output) {
    RandomAI black_ai(config.seed == 0 ? 0 : config.seed);
    RandomAI white_ai(config.seed == 0 ? 0 : config.seed + 1);
    return run_games(
        config,
        AIPackage{&black_ai},
        AIPackage{&white_ai},
        dataset_output,
        board_state_output,
        game_aux_output);
}

}  // namespace kadoka::othello
