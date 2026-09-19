#include <sstream>
#include <string>
#include <vector>

#include "kadoka_othello/league.hpp"
#include "test_support.hpp"

int main(int argc, char** argv) {
    using namespace kadoka::othello;

    KADOKA_REQUIRE(argc == 2);
    const std::string random_manifest = argv[1];

    LeagueParticipantConfig first_config;
    first_config.manifest_path = random_manifest;
    first_config.board_size = 6;
    first_config.seed = 1001;
    first_config.config_hash = "test-a";

    LeagueParticipantConfig second_config = first_config;
    second_config.seed = 1002;
    second_config.config_hash = "test-b";

    const LeagueParticipant first = load_league_participant(first_config);
    const LeagueParticipant second = load_league_participant(second_config);

    KADOKA_REQUIRE(first.participant_id != second.participant_id);

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

    KADOKA_REQUIRE(result.games[0].black_participant_id == first.participant_id);
    KADOKA_REQUIRE(result.games[0].white_participant_id == second.participant_id);
    KADOKA_REQUIRE(result.games[1].black_participant_id == second.participant_id);
    KADOKA_REQUIRE(result.games[1].white_participant_id == first.participant_id);
    KADOKA_REQUIRE(result.games[0].game_id != result.games[1].game_id);

    for (const auto& entry : result.table) {
        KADOKA_REQUIRE(entry.rating.games == 2);
        KADOKA_REQUIRE(entry.rating.deviation < 350.0);
        KADOKA_REQUIRE(entry.rating.wins + entry.rating.losses + entry.rating.draws == 2);
    }

    KADOKA_REQUIRE(game_log.str().find("kadoka.league_game.v1") != std::string::npos);
    KADOKA_REQUIRE(dataset.str().find("kadoka.league_position.v1") != std::string::npos);
    KADOKA_REQUIRE(dataset.str().find(result.games[0].game_id) != std::string::npos);

    return 0;
}
