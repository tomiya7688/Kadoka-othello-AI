#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "kadoka_othello/league.hpp"

namespace {

void print_usage() {
    std::cout
        << "Kadoka Othello AI League\n"
        << "  kadoka_ai_league <board-size> <games-per-color> <seed> "
        << "<game-log.jsonl|-> <position-dataset.jsonl|-> <manifest.json> [...]\n";
}

}  // namespace

int main(int argc, char** argv) {
    using namespace kadoka::othello;

    if (argc < 8) {
        print_usage();
        return 2;
    }

    try {
        const std::size_t board_size = static_cast<std::size_t>(
            std::strtoull(argv[1], nullptr, 10));
        LeagueRunConfig run_config;
        run_config.games_per_color = static_cast<std::size_t>(
            std::strtoull(argv[2], nullptr, 10));
        run_config.seed = std::strtoull(argv[3], nullptr, 10);
        run_config.collect_metrics = true;

        std::ofstream game_log_file;
        std::ofstream dataset_file;
        std::ostream* game_log = nullptr;
        std::ostream* dataset = nullptr;

        if (std::string(argv[4]) != "-") {
            game_log_file.open(argv[4], std::ios::out | std::ios::trunc);
            if (!game_log_file) {
                throw std::runtime_error("failed to open league game log");
            }
            game_log = &game_log_file;
        }
        if (std::string(argv[5]) != "-") {
            dataset_file.open(argv[5], std::ios::out | std::ios::trunc);
            if (!dataset_file) {
                throw std::runtime_error("failed to open league position dataset");
            }
            dataset = &dataset_file;
        }

        std::vector<LeagueParticipant> participants;
        participants.reserve(static_cast<std::size_t>(argc - 6));
        for (int i = 6; i < argc; ++i) {
            LeagueParticipantConfig participant;
            participant.manifest_path = argv[i];
            participant.board_size = board_size;
            participant.seed = run_config.seed + static_cast<std::uint64_t>(i - 5);
            participants.push_back(load_league_participant(std::move(participant)));
        }

        const LeagueRunResult result = run_round_robin_league(
            participants,
            run_config,
            game_log,
            dataset);

        std::cout << "games=" << result.games.size() << '\n';
        std::cout << std::fixed << std::setprecision(2);
        for (std::size_t rank = 0; rank < result.table.size(); ++rank) {
            const auto& entry = result.table[rank];
            std::cout << "rank=" << (rank + 1)
                      << " participant=" << entry.participant.participant_id
                      << " rating=" << entry.rating.rating
                      << " rd=" << entry.rating.deviation
                      << " games=" << entry.rating.games
                      << " wins=" << entry.rating.wins
                      << " losses=" << entry.rating.losses
                      << " draws=" << entry.rating.draws
                      << '\n';
        }
    } catch (const std::exception& error) {
        std::cerr << "AI League error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
