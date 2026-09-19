#pragma once

#include <string>

#include "kadoka_othello/core_state.hpp"
#include "kadoka_othello/game.hpp"

namespace kadoka::othello {

// Transitional Headless/Dataset snapshot. It intentionally contains only the
// canonical Core state. Record/event enrichment is handled separately.
using GameSnapshot = CoreState;

[[nodiscard]] GameSnapshot make_snapshot(
    const Game& game,
    CoreTimeState time = {});
[[nodiscard]] std::string snapshot_to_json(const GameSnapshot& snapshot);

}  // namespace kadoka::othello
