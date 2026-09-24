#include <cstdint>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "kadoka_othello/league.hpp"
#include "kadoka_othello/league_registry.hpp"
#include "kadoka_othello/league_scheduler.hpp"
#include "test_support.hpp"

using namespace kadoka::othello;

namespace {

LeagueRegistryEntry make_entry(
    const std::string& manifest_path,
    std::uint64_t seed,
    std::string config_hash,
    LeagueParticipantRole role,
    std::string checkpoint_id = {}) {
    LeagueParticipantConfig config;
    config.manifest_path = manifest_path;
    config.board_size = 6;
    config.seed = seed;
    config.config_hash = std::move(config_hash);

    LeagueRegistryEntry entry;
    entry.participant =
        load_league_participant(std::move(config));
    entry.role = role;
    entry.checkpoint_id = std::move(checkpoint_id);
    return entry;
}

void test_round_robin_compatibility(
    const std::string& manifest_path) {
    LeagueParticipantConfig first_config;
    first_config.manifest_path = manifest_path;
    first_config.board_size = 6;
    first_config.seed = 1001;
    first_config.config_hash = "test-a";

    LeagueParticipantConfig second_config = first_config;
    second_config.seed = 1002;
    second_config.config_hash = "test-b";

    const LeagueParticipant first =
        load_league_participant(first_config);
    const LeagueParticipant second =
        load_league_participant(second_config);

    KADOKA_REQUIRE(
        first.participant_id != second.participant_id);

    LeagueRunConfig run_config;
    run_config.games_per_color = 1;
    run_config.seed = 777;
    run_config.collect_metrics = true;

    std::ostringstream game_log;
    std::ostringstream dataset;
    const LeagueRunResult result = run_round_robin_league(
        std::vector<LeagueParticipant>{first, second},
        run_config,
        &game_log,
        &dataset);

    KADOKA_REQUIRE(result.games.size() == 2);
    KADOKA_REQUIRE(result.table.size() == 2);
    KADOKA_REQUIRE(
        result.games[0].black_participant_id ==
        first.participant_id);
    KADOKA_REQUIRE(
        result.games[0].white_participant_id ==
        second.participant_id);
    KADOKA_REQUIRE(
        result.games[1].black_participant_id ==
        second.participant_id);
    KADOKA_REQUIRE(
        result.games[1].white_participant_id ==
        first.participant_id);
    KADOKA_REQUIRE(
        result.games[0].game_id != result.games[1].game_id);
    KADOKA_REQUIRE(
        result.games[0].game_id.size() == 26);
    KADOKA_REQUIRE(
        result.games[0].reproducibility_key.rfind(
            "run-", 0) == 0);

    for (const auto& entry : result.table) {
        KADOKA_REQUIRE(entry.rating.games == 2);
        KADOKA_REQUIRE(entry.rating.deviation < 350.0);
        KADOKA_REQUIRE(
            entry.rating.wins +
            entry.rating.losses +
            entry.rating.draws == 2);
    }

    KADOKA_REQUIRE(
        game_log.str().find("kadoka.league_game.v1") !=
        std::string::npos);
    KADOKA_REQUIRE(
        game_log.str().find("\"total_nodes\"") !=
        std::string::npos);
    KADOKA_REQUIRE(
        dataset.str().find("kadoka.league_position.v1") !=
        std::string::npos);
}

void test_registry_schedulers_and_persistence(
    const std::string& manifest_path) {
    LeagueRegistry registry;
    registry.register_entry(make_entry(
        manifest_path,
        2001,
        "champion-v1",
        LeagueParticipantRole::Champion));
    registry.register_entry(make_entry(
        manifest_path,
        2002,
        "candidate-v2",
        LeagueParticipantRole::Candidate));
    registry.register_entry(make_entry(
        manifest_path,
        2003,
        "checkpoint-2025",
        LeagueParticipantRole::HallOfFame,
        "generation-2025"));
    registry.register_entry(make_entry(
        manifest_path,
        2004,
        "standard-control",
        LeagueParticipantRole::Standard));

    KADOKA_REQUIRE(registry.entries().size() == 4);

    registry.entries()[0].rating.rating = 1500.0;
    registry.entries()[1].rating.rating = 1510.0;
    registry.entries()[2].rating.rating = 1800.0;
    registry.entries()[3].rating.rating = 1200.0;

    const auto round_robin =
        schedule_round_robin(registry, 6);
    KADOKA_REQUIRE(round_robin.size() == 6);

    const auto random_schedule =
        schedule_random_matches(registry, 6, 5, 1234);
    KADOKA_REQUIRE(random_schedule.size() == 5);
    for (const auto pair : random_schedule) {
        KADOKA_REQUIRE(
            pair.first_entry != pair.second_entry);
    }

    const auto rating_schedule =
        schedule_rating_band(
            registry,
            6,
            3,
            20.0,
            9876);
    KADOKA_REQUIRE(rating_schedule.size() == 3);
    for (const auto pair : rating_schedule) {
        const double lhs =
            registry.entries()[pair.first_entry].rating.rating;
        const double rhs =
            registry.entries()[pair.second_entry].rating.rating;
        KADOKA_REQUIRE(
            lhs - rhs <= 20.0 && rhs - lhs <= 20.0);
    }

    const auto candidate_champion =
        schedule_candidate_champion(registry, 6);
    KADOKA_REQUIRE(candidate_champion.size() == 1);
    const auto candidate_hof =
        schedule_candidate_hall_of_fame(registry, 6);
    KADOKA_REQUIRE(candidate_hof.size() == 1);

    LeagueRunConfig config;
    config.games_per_color = 1;
    config.seed = 424242;
    config.collect_metrics = true;

    std::ostringstream game_log;
    std::ostringstream board_state;
    std::ostringstream game_aux;
    LeagueRunOutputs outputs;
    outputs.game_log = &game_log;
    outputs.board_state_output = &board_state;
    outputs.game_aux_output = &game_aux;

    const LeagueRunResult result = run_registry_schedule(
        registry,
        6,
        candidate_champion,
        config,
        outputs);

    KADOKA_REQUIRE(result.games.size() == 2);
    KADOKA_REQUIRE(result.table.size() == 2);

    for (const auto& game : result.games) {
        KADOKA_REQUIRE(game.game_id.size() == 26);
        KADOKA_REQUIRE(
            board_state.str().find(game.game_id) !=
            std::string::npos);
        KADOKA_REQUIRE(
            game_aux.str().find(game.game_id) !=
            std::string::npos);
        KADOKA_REQUIRE(
            game.reproducibility_key.rfind("run-", 0) == 0);
    }

    const LeagueRegistryEntry* champion = nullptr;
    const LeagueRegistryEntry* candidate = nullptr;
    for (const auto& entry : registry.entries()) {
        if (entry.role == LeagueParticipantRole::Champion) {
            champion = &entry;
        } else if (
            entry.role == LeagueParticipantRole::Candidate) {
            candidate = &entry;
        }
    }
    KADOKA_REQUIRE(champion != nullptr);
    KADOKA_REQUIRE(candidate != nullptr);
    KADOKA_REQUIRE(champion->rating.games == 2);
    KADOKA_REQUIRE(candidate->rating.games == 2);
    KADOKA_REQUIRE(champion->rating.deviation < 350.0);
    KADOKA_REQUIRE(candidate->rating.deviation < 350.0);

    std::ostringstream persisted;
    write_league_registry_jsonl(registry, persisted);
    KADOKA_REQUIRE(
        persisted.str().find(
            "\"role\":\"hall_of_fame\"") !=
        std::string::npos);
    KADOKA_REQUIRE(
        persisted.str().find(
            "\"checkpoint_id\":\"generation-2025\"") !=
        std::string::npos);

    std::istringstream input(persisted.str());
    const LeagueRegistry restored =
        read_league_registry_jsonl(input);
    KADOKA_REQUIRE(restored.entries().size() == 4);

    const auto& restored_candidate = restored.entries()[1];
    KADOKA_REQUIRE(
        restored_candidate.role ==
        LeagueParticipantRole::Candidate);
    KADOKA_REQUIRE(restored_candidate.rating.games == 2);
}

}  // namespace

int main(int argc, char** argv) {
    KADOKA_REQUIRE(argc == 2);
    const std::string manifest_path = argv[1];

    test_round_robin_compatibility(manifest_path);
    test_registry_schedulers_and_persistence(manifest_path);
    return 0;
}
