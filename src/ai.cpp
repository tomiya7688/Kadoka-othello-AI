#include "kadoka_othello/ai.hpp"

#include <limits>
#include <stdexcept>

#include "kadoka_othello/rules.hpp"

namespace kadoka::othello {
namespace {

std::uint64_t parse_u64_metric(
    std::string_view key,
    std::string_view value) {
    std::size_t consumed = 0;
    const unsigned long long parsed =
        std::stoull(std::string(value), &consumed, 10);
    if (consumed != value.size()) {
        throw std::invalid_argument(
            "AI metric " + std::string(key) +
            " must be an unsigned integer");
    }
    return static_cast<std::uint64_t>(parsed);
}

double parse_double_metric(
    std::string_view key,
    std::string_view value) {
    std::size_t consumed = 0;
    const double parsed =
        std::stod(std::string(value), &consumed);
    if (consumed != value.size()) {
        throw std::invalid_argument(
            "AI metric " + std::string(key) +
            " must be numeric");
    }
    return parsed;
}

}  // namespace

void apply_standard_ai_metric(
    AIOutput& output,
    std::string_view key,
    std::string_view value) {
    if (key == "nodes") {
        output.metrics.nodes = parse_u64_metric(key, value);
    } else if (key == "simulations") {
        output.metrics.simulations = parse_u64_metric(key, value);
    } else if (key == "depth") {
        const std::uint64_t depth = parse_u64_metric(key, value);
        if (depth > std::numeric_limits<std::size_t>::max()) {
            throw std::out_of_range("AI metric depth is too large");
        }
        output.metrics.depth = static_cast<std::size_t>(depth);
    } else if (key == "search_effort") {
        output.metrics.search_effort =
            parse_double_metric(key, value);
    }
}

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
