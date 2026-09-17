#include <cassert>
#include <sstream>
#include <string>
#include <vector>

#include "kadoka_othello/league.hpp"

int main() {
    using namespace kadoka::othello;

    LeagueParticipantConfig first_config;
    first_config.manifest_path = "src/packages/random/manifest.json";
    first_config.board_size = 6;
    first_config.seed = 1001;
    first_config.config_hash = "test-a";

    LeagueParticipantConfig second_config = first_config;
    second_config.seed = 1002;
    second_config.config_hash = "test-b";

    const LeagueParticipant first = load_league_participant(first_config);
    const LeagueParticipant second = load_league_participant(second_config);

    assert(first.participant_id != second.participant_id);

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

    assert(result.games.size() == 2);
    assert(result.table.size() == 2);

    assert(result.games[0].black_participant_id == first.participant_id);
    assert(result.games[0].white_participant_id == second.participant_id);
    assert(result.games[1].black_participant_id == second.participant_id);
    assert(result.games[1].white_participant_id == first.participant_id);
    assert(result.games[0].game_id != result.games[1].game_id);

    for (const auto& entry : result.table) {
        assert(entry.rating.games == 2);
        assert(entry.rating.deviation < 350.0);
        assert(entry.rating.wins + entry.rating.losses + entry.rating.draws == 2);
    }

    assert(game_log.str().find("kadoka.league_game.v1") != std::string::npos);
    assert(dataset.str().find("kadoka.league_position.v1") != std::string::npos);
    assert(dataset.str().find(result.games[0].game_id) != std::string::npos);

    return 0;
}
