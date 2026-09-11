#include "kadoka_othello/ai.hpp"

#include <stdexcept>

namespace kadoka::othello {

AdaptedAIInput PassThroughAdapter::adapt(const AIInput& input) const {
    return AdaptedAIInput{input.board, input.legal_moves};
}

AdaptedAIInput DropLegalMovesAdapter::adapt(const AIInput& input) const {
    return AdaptedAIInput{input.board, nullptr};
}

AIInspection IAIEngine::inspect(const AdaptedAIInput& input) {
    return AIInspection{think(input), {}};
}

RandomAI::RandomAI(std::uint64_t seed)
    : rng_(seed == 0 ? std::random_device{}() : seed) {}

std::string RandomAI::id() const {
    return "kadoka.random";
}

AIOutput RandomAI::think(const AdaptedAIInput& input) {
    if (input.legal_moves == nullptr || input.legal_moves->empty()) {
        throw std::invalid_argument("RandomAI requires non-empty legal moves");
    }

    std::uniform_int_distribution<std::size_t> pick(0, input.legal_moves->size() - 1);
    return AIOutput{input.legal_moves->at(pick(rng_))};
}

AIInspection RandomAI::inspect(const AdaptedAIInput& input) {
    const AIOutput output = think(input);
    AIInspection inspection;
    inspection.output = output;
    inspection.diagnostics.push_back({"engine", id()});
    inspection.diagnostics.push_back({"strategy", "uniform_random_legal_move"});
    if (input.legal_moves != nullptr) {
        inspection.diagnostics.push_back({"legal_move_count", std::to_string(input.legal_moves->size())});
    }
    return inspection;
}

AIOutput invoke_ai(
    AIPackage package,
    const AIInput& input) {
    if (package.engine == nullptr || package.adapter == nullptr) {
        throw std::invalid_argument("AI package requires engine and adapter");
    }

    return package.engine->think(package.adapter->adapt(input));
}

AIInspection inspect_ai(
    AIPackage package,
    const AIInput& input) {
    if (package.engine == nullptr || package.adapter == nullptr) {
        throw std::invalid_argument("AI package requires engine and adapter");
    }

    return package.engine->inspect(package.adapter->adapt(input));
}

}  // namespace kadoka::othello
