#include "kadoka_othello/state.hpp"

namespace kadoka::othello {

GameSnapshot make_snapshot(
    const Game& game,
    CoreTimeState time) {
    return make_core_state(CoreStateView{
        &game.board(),
        game.current_player(),
        time,
    });
}

std::string snapshot_to_json(const GameSnapshot& snapshot) {
    return core_state_to_json(snapshot);
}

}  // namespace kadoka::othello
