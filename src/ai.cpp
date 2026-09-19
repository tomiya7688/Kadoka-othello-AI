#include "kadoka_othello/ai.hpp"

#include <stdexcept>

#include "kadoka_othello/rules.hpp"

namespace kadoka::othello {

AIInspection IAIEngine::inspect(const AIInput& input) {
    AIInspection inspection;
    inspection.output = think(input);
    return inspection;
}

RandomAI::RandomAI(std::uint64_t seed)
    : rng_(seed == 0 ? std::random_device{}() : seed) {}

std::string RandomAI::id() const {
    return "kadoka.random";
}

AIOutput RandomAI::think(const AIInput& input) {
    if (input.board == nullptr) {
        throw std::invalid_argument("RandomAI requires board input");
    }
    const std::vector<Position> legal_moves =
        rules::legal_moves(*input.board, input.side_to_move);
    if (legal_moves.empty()) {
        throw std::invalid_argument("RandomAI requires at least one legal move");
    }

    std::uniform_int_distribution<std::size_t> pick(0, legal_moves.size() - 1);
    return AIOutput{legal_moves[pick(rng_)]};
}

AIInspection RandomAI::inspect(const AIInput& input) {
    if (input.board == nullptr) {
        throw std::invalid_argument("RandomAI requires board input");
    }
    const std::vector<Position> legal_moves =
        rules::legal_moves(*input.board, input.side_to_move);
    if (legal_moves.empty()) {
        throw std::invalid_argument("RandomAI requires at least one legal move");
    }

    std::uniform_int_distribution<std::size_t> pick(0, legal_moves.size() - 1);
    AIInspection inspection;
    inspection.output = AIOutput{legal_moves[pick(rng_)]};
    inspection.diagnostics.push_back({"engine", id()});
    inspection.diagnostics.push_back({"strategy", "uniform_random_legal_move"});
    inspection.diagnostics.push_back({"legal_move_count", std::to_string(legal_moves.size())});
    const double policy = 1.0 / static_cast<double>(legal_moves.size());
    for (const Position move : legal_moves) {
        inspection.candidates.push_back({move, 0.0, policy});
    }
    return inspection;
}

AIOutput invoke_ai(
    AIPackage package,
    const AIInput& input) {
    if (package.engine == nullptr) {
        throw std::invalid_argument("AI package requires engine");
    }
    return package.engine->think(input);
}

AIInspection inspect_ai(
    AIPackage package,
    const AIInput& input) {
    if (package.engine == nullptr) {
        throw std::invalid_argument("AI package requires engine");
    }
    return package.engine->inspect(input);
}

}  // namespace kadoka::othello
