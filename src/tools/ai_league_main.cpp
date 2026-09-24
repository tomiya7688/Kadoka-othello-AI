#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "kadoka_othello/league.hpp"
#include "kadoka_othello/league_registry.hpp"
#include "kadoka_othello/league_scheduler.hpp"

namespace {

using namespace kadoka::othello;

void print_usage() {
    std::cout
        << "Kadoka Othello AI League\n"
        << "Legacy round-robin:\n"
        << "  kadoka_ai_league <board-size> <games-per-color> <seed> "
        << "<game-log.jsonl|-> <position-dataset.jsonl|-> "
        << "<manifest.json> [...]\n\n"
        << "Persistent registry:\n"
        << "  kadoka_ai_league registry-add <registry.jsonl> "
        << "<standard|champion|candidate|hall_of_fame> "
        << "<board-size> <seed> <config-hash|-> <checkpoint-id|-> "
        << "<manifest.json>\n"
        << "  kadoka_ai_league registry-table <registry.jsonl>\n"
        << "  kadoka_ai_league registry-run <registry.jsonl> "
        << "<round_robin|random|rating_band|candidate_champion|candidate_hof> "
        << "<board-size> <games-per-color> <seed> "
        << "<game-log.jsonl|-> <board-state.jsonl|-> <game-aux.jsonl|-> "
        << "[pair-count] [max-rating-gap]\n";
}

LeagueRegistry load_registry_file(
    const std::string& path,
    bool allow_missing) {
    if (allow_missing &&
        !std::filesystem::exists(path)) {
        return {};
    }

    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error(
            "failed to open league registry: " + path);
    }
    return read_league_registry_jsonl(input);
}

void save_registry_file(
    const LeagueRegistry& registry,
    const std::string& path) {
    const std::filesystem::path target(path);
    const std::filesystem::path temporary =
        target.string() + ".tmp";

    {
        std::ofstream output(
            temporary,
            std::ios::out | std::ios::trunc);
        if (!output) {
            throw std::runtime_error(
                "failed to create league registry temp file");
        }
        write_league_registry_jsonl(registry, output);
    }

    std::error_code error;
    std::filesystem::remove(target, error);
    error.clear();
    std::filesystem::rename(temporary, target, error);
    if (error) {
        std::filesystem::remove(temporary);
        throw std::runtime_error(
            "failed to replace league registry: " +
            error.message());
    }
}

void print_table(
    const std::vector<LeagueTableEntry>& table) {
    std::cout << std::fixed << std::setprecision(2);
    for (std::size_t rank = 0;
         rank < table.size();
         ++rank) {
        const auto& entry = table[rank];
        std::cout << "rank=" << (rank + 1)
                  << " participant="
                  << entry.participant.participant_id
                  << " rating=" << entry.rating.rating
                  << " rd=" << entry.rating.deviation
                  << " games=" << entry.rating.games
                  << " wins=" << entry.rating.wins
                  << " losses=" << entry.rating.losses
                  << " draws=" << entry.rating.draws
                  << '\n';
    }
}

int command_registry_add(int argc, char** argv) {
    if (argc != 9) {
        print_usage();
        return 2;
    }

    const std::string registry_path = argv[2];
    LeagueRegistry registry =
        load_registry_file(registry_path, true);

    LeagueParticipantConfig config;
    config.board_size = static_cast<std::size_t>(
        std::strtoull(argv[4], nullptr, 10));
    config.seed = std::strtoull(argv[5], nullptr, 10);
    if (std::string(argv[6]) != "-") {
        config.config_hash = argv[6];
    }
    config.manifest_path = argv[8];

    LeagueRegistryEntry entry;
    entry.participant =
        load_league_participant(std::move(config));
    entry.role =
        parse_league_participant_role(argv[3]);
    if (std::string(argv[7]) != "-") {
        entry.checkpoint_id = argv[7];
    }

    const std::string participant_id =
        entry.participant.participant_id;
    registry.register_entry(std::move(entry));
    save_registry_file(registry, registry_path);

    std::cout << "registered participant="
              << participant_id
              << " registry=" << registry_path << '\n';
    return 0;
}

int command_registry_table(int argc, char** argv) {
    if (argc != 3) {
        print_usage();
        return 2;
    }

    const LeagueRegistry registry =
        load_registry_file(argv[2], false);
    std::cout << std::fixed << std::setprecision(2);
    for (const auto& entry : registry.entries()) {
        std::cout
            << "participant="
            << entry.participant.participant_id
            << " role="
            << league_participant_role_name(entry.role)
            << " checkpoint=" << entry.checkpoint_id
            << " board_size="
            << entry.participant.config.board_size
            << " rating=" << entry.rating.rating
            << " rd=" << entry.rating.deviation
            << " games=" << entry.rating.games
            << '\n';
    }
    return 0;
}

std::vector<LeagueRegistryPairing> make_registry_schedule(
    const LeagueRegistry& registry,
    const std::string& scheduler,
    std::size_t board_size,
    std::uint64_t seed,
    int argc,
    char** argv) {
    if (scheduler == "round_robin") {
        if (argc != 10) {
            throw std::invalid_argument(
                "round_robin scheduler takes no extra arguments");
        }
        return schedule_round_robin(registry, board_size);
    }
    if (scheduler == "random") {
        if (argc != 11) {
            throw std::invalid_argument(
                "random scheduler requires pair-count");
        }
        return schedule_random_matches(
            registry,
            board_size,
            static_cast<std::size_t>(
                std::strtoull(argv[10], nullptr, 10)),
            seed);
    }
    if (scheduler == "rating_band") {
        if (argc != 12) {
            throw std::invalid_argument(
                "rating_band scheduler requires pair-count and max-rating-gap");
        }
        return schedule_rating_band(
            registry,
            board_size,
            static_cast<std::size_t>(
                std::strtoull(argv[10], nullptr, 10)),
            std::strtod(argv[11], nullptr),
            seed);
    }
    if (scheduler == "candidate_champion") {
        if (argc != 10) {
            throw std::invalid_argument(
                "candidate_champion scheduler takes no extra arguments");
        }
        return schedule_candidate_champion(
            registry,
            board_size);
    }
    if (scheduler == "candidate_hof") {
        if (argc != 10) {
            throw std::invalid_argument(
                "candidate_hof scheduler takes no extra arguments");
        }
        return schedule_candidate_hall_of_fame(
            registry,
            board_size);
    }
    throw std::invalid_argument(
        "unknown league scheduler: " + scheduler);
}

int command_registry_run(int argc, char** argv) {
    if (argc < 10 || argc > 12) {
        print_usage();
        return 2;
    }

    const std::string registry_path = argv[2];
    const std::string scheduler = argv[3];
    const std::size_t board_size =
        static_cast<std::size_t>(
            std::strtoull(argv[4], nullptr, 10));

    LeagueRunConfig config;
    config.games_per_color =
        static_cast<std::size_t>(
            std::strtoull(argv[5], nullptr, 10));
    config.seed = std::strtoull(argv[6], nullptr, 10);
    config.collect_metrics = true;

    LeagueRegistry registry =
        load_registry_file(registry_path, false);
    const auto pairings = make_registry_schedule(
        registry,
        scheduler,
        board_size,
        config.seed,
        argc,
        argv);

    std::ofstream game_log_file;
    std::ofstream board_state_file;
    std::ofstream game_aux_file;
    LeagueRunOutputs outputs;

    if (std::string(argv[7]) != "-") {
        game_log_file.open(
            argv[7],
            std::ios::out | std::ios::trunc);
        if (!game_log_file) {
            throw std::runtime_error(
                "failed to open league game log");
        }
        outputs.game_log = &game_log_file;
    }

    const bool board_enabled =
        std::string(argv[8]) != "-";
    const bool aux_enabled =
        std::string(argv[9]) != "-";
    if (board_enabled != aux_enabled) {
        throw std::invalid_argument(
            "BoardState and GameAux outputs must be enabled together");
    }
    if (board_enabled) {
        board_state_file.open(
            argv[8],
            std::ios::out | std::ios::trunc);
        game_aux_file.open(
            argv[9],
            std::ios::out | std::ios::trunc);
        if (!board_state_file || !game_aux_file) {
            throw std::runtime_error(
                "failed to open league Game Record output");
        }
        outputs.board_state_output =
            &board_state_file;
        outputs.game_aux_output =
            &game_aux_file;
    }

    const LeagueRunResult result =
        run_registry_schedule(
            registry,
            board_size,
            pairings,
            config,
            outputs);
    save_registry_file(registry, registry_path);

    std::cout << "scheduler=" << scheduler
              << " pairings=" << pairings.size()
              << " games=" << result.games.size()
              << '\n';
    print_table(result.table);
    return 0;
}

int legacy_round_robin(int argc, char** argv) {
    if (argc < 8) {
        print_usage();
        return 2;
    }

    const std::size_t board_size =
        static_cast<std::size_t>(
            std::strtoull(argv[1], nullptr, 10));
    LeagueRunConfig run_config;
    run_config.games_per_color =
        static_cast<std::size_t>(
            std::strtoull(argv[2], nullptr, 10));
    run_config.seed =
        std::strtoull(argv[3], nullptr, 10);
    run_config.collect_metrics = true;

    std::ofstream game_log_file;
    std::ofstream dataset_file;
    std::ostream* game_log = nullptr;
    std::ostream* dataset = nullptr;

    if (std::string(argv[4]) != "-") {
        game_log_file.open(
            argv[4],
            std::ios::out | std::ios::trunc);
        if (!game_log_file) {
            throw std::runtime_error(
                "failed to open league game log");
        }
        game_log = &game_log_file;
    }
    if (std::string(argv[5]) != "-") {
        dataset_file.open(
            argv[5],
            std::ios::out | std::ios::trunc);
        if (!dataset_file) {
            throw std::runtime_error(
                "failed to open league position dataset");
        }
        dataset = &dataset_file;
    }

    std::vector<LeagueParticipant> participants;
    participants.reserve(
        static_cast<std::size_t>(argc - 6));
    for (int i = 6; i < argc; ++i) {
        LeagueParticipantConfig participant;
        participant.manifest_path = argv[i];
        participant.board_size = board_size;
        participant.seed =
            run_config.seed +
            static_cast<std::uint64_t>(i - 5);
        participants.push_back(
            load_league_participant(
                std::move(participant)));
    }

    const LeagueRunResult result =
        run_round_robin_league(
            participants,
            run_config,
            game_log,
            dataset);

    std::cout << "games="
              << result.games.size() << '\n';
    print_table(result.table);
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 2) {
            print_usage();
            return 0;
        }

        const std::string command = argv[1];
        if (command == "registry-add") {
            return command_registry_add(argc, argv);
        }
        if (command == "registry-table") {
            return command_registry_table(argc, argv);
        }
        if (command == "registry-run") {
            return command_registry_run(argc, argv);
        }
        return legacy_round_robin(argc, argv);
    } catch (const std::exception& error) {
        std::cerr << "AI League error: "
                  << error.what() << '\n';
        return 1;
    }
}
