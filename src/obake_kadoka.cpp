#include "kadoka_othello/obake_kadoka.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace kadoka::othello {
namespace {

double read_number(const std::string& text, const std::string& key, double fallback) {
    const std::string token = "\"" + key + "\"";
    const std::size_t key_pos = text.find(token);
    if (key_pos == std::string::npos) return fallback;
    const std::size_t colon = text.find(':', key_pos + token.size());
    if (colon == std::string::npos) return fallback;
    std::size_t start = colon + 1;
    while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start]))) ++start;
    std::size_t end = start;
    while (end < text.size() &&
           (std::isdigit(static_cast<unsigned char>(text[end])) ||
            text[end] == '-' || text[end] == '+' || text[end] == '.' ||
            text[end] == 'e' || text[end] == 'E')) {
        ++end;
    }
    return end == start ? fallback : std::stod(text.substr(start, end - start));
}

std::size_t read_size(const std::string& text, const std::string& key, std::size_t fallback) {
    const double value = read_number(text, key, static_cast<double>(fallback));
    return value < 0.0 ? fallback : static_cast<std::size_t>(value);
}

std::size_t count_empty_cells(const Board& board) {
    std::size_t empty = 0;
    for (std::size_t row = 0; row < board.size(); ++row) {
        for (std::size_t col = 0; col < board.size(); ++col) {
            if (board.at({row, col}) == Cell::Empty) ++empty;
        }
    }
    return empty;
}

void add_feature_diagnostics(
    std::vector<AIDiagnostic>& diagnostics,
    const ObakeKadokaFeatureVector& features,
    const ObakeKadokaEvaluatorWeights& weights) {
    diagnostics.push_back({"feature.occupied_neighbors", std::to_string(features.occupied_neighbors)});
    diagnostics.push_back({"feature.empty_neighbors", std::to_string(features.empty_neighbors)});
    diagnostics.push_back({"feature.mixed_color", std::to_string(features.mixed_color)});
    diagnostics.push_back({"feature.color_transitions", std::to_string(features.color_transitions)});
    diagnostics.push_back({"feature.line_interest", std::to_string(features.line_interest)});
    diagnostics.push_back({"feature.local_density", std::to_string(features.local_density)});
    diagnostics.push_back({"feature.early_center", std::to_string(features.early_center)});

    diagnostics.push_back({"contrib.occupied_neighbors", std::to_string(features.occupied_neighbors * weights.occupied_neighbors)});
    diagnostics.push_back({"contrib.empty_neighbors", std::to_string(features.empty_neighbors * weights.empty_neighbors)});
    diagnostics.push_back({"contrib.mixed_color", std::to_string(features.mixed_color * weights.mixed_color)});
    diagnostics.push_back({"contrib.color_transitions", std::to_string(features.color_transitions * weights.color_transitions)});
    diagnostics.push_back({"contrib.line_interest", std::to_string(features.line_interest * weights.line_interest)});
    diagnostics.push_back({"contrib.local_density", std::to_string(features.local_density * weights.local_density)});
    diagnostics.push_back({"contrib.early_center", std::to_string(features.early_center * weights.early_center)});
}

}  // namespace

ObakeKadokaAI::ObakeKadokaAI(
    std::uint64_t seed,
    ObakeKadokaConfig config)
    : config_(config),
      evaluator_(config.evaluator_weights),
      rng_(seed == 0 ? std::random_device{}() : seed) {
    config_.memory_depth = std::min<std::size_t>(config_.memory_depth, recent_.size());
    config_.randomizer_temperature = std::max(config_.randomizer_temperature, 0.05);
    config_.exploration_floor = std::max(config_.exploration_floor, 0.0);
}

std::string ObakeKadokaAI::id() const {
    return "kadoka.obake_kadoka";
}

AIOutput ObakeKadokaAI::think(const AdaptedAIInput& input) {
    if (input.board == nullptr) throw std::invalid_argument("ObakeKadokaAI requires board input");

    const std::uint64_t board_hash = hash_board(*input.board);
    infer_previous_attempt_result(board_hash);

    std::array<WeightedMove, 100> candidates{};
    const std::size_t empty_cells = count_empty_cells(*input.board);
    const std::size_t count = collect_candidates(*input.board, candidates, empty_cells);
    if (count == 0) throw std::runtime_error("ObakeKadokaAI found no empty square");

    const Position move = choose_weighted(candidates, count);
    remember(move, board_hash);
    return AIOutput{move};
}

AIInspection ObakeKadokaAI::inspect(const AdaptedAIInput& input) {
    if (input.board == nullptr) throw std::invalid_argument("ObakeKadokaAI requires board input");

    const std::uint64_t board_hash = hash_board(*input.board);
    infer_previous_attempt_result(board_hash);

    std::array<WeightedMove, 100> candidates{};
    const std::size_t empty_cells = count_empty_cells(*input.board);
    const std::size_t count = collect_candidates(*input.board, candidates, empty_cells);
    if (count == 0) throw std::runtime_error("ObakeKadokaAI found no empty square");

    double total_weight = 0.0;
    for (std::size_t i = 0; i < count; ++i) total_weight += candidates[i].weight;

    const Position selected = choose_weighted(candidates, count);
    remember(selected, board_hash);

    AIInspection inspection;
    inspection.output = AIOutput{selected};
    inspection.candidates.reserve(count);

    const WeightedMove* selected_candidate = nullptr;
    for (std::size_t i = 0; i < count; ++i) {
        inspection.candidates.push_back(AICandidate{
            candidates[i].move,
            candidates[i].evaluation.score,
            total_weight > 0.0 ? candidates[i].weight / total_weight : 0.0,
        });
        if (candidates[i].move == selected) selected_candidate = &candidates[i];
    }

    std::size_t inferred_rejected = 0;
    for (std::size_t i = 0; i < recent_count_; ++i) {
        if (recent_[i].valid && recent_[i].inferred_rejected) ++inferred_rejected;
    }

    inspection.diagnostics.push_back({"engine", id()});
    inspection.diagnostics.push_back({"strategy", "obake_tunable_evaluator_plus_randomizer"});
    inspection.diagnostics.push_back({"legal_moves_used", "false"});
    inspection.diagnostics.push_back({"memory_depth", std::to_string(config_.memory_depth)});
    inspection.diagnostics.push_back({"remembered_attempts", std::to_string(recent_count_)});
    inspection.diagnostics.push_back({"inferred_rejected_attempts", std::to_string(inferred_rejected)});
    inspection.diagnostics.push_back({"candidate_count", std::to_string(count)});
    inspection.diagnostics.push_back({"randomizer_temperature", std::to_string(config_.randomizer_temperature)});

    if (selected_candidate != nullptr) {
        inspection.diagnostics.push_back({"selected_score", std::to_string(selected_candidate->evaluation.score)});
        add_feature_diagnostics(
            inspection.diagnostics,
            selected_candidate->evaluation.features,
            evaluator_.weights());
    }

    return inspection;
}

std::size_t ObakeKadokaAI::collect_candidates(
    const Board& board,
    std::array<WeightedMove, 100>& candidates,
    std::size_t empty_cells) const {
    if (board.size() * board.size() > candidates.size()) {
        throw std::invalid_argument("ObakeKadokaAI supports boards up to 10x10");
    }

    std::size_t count = 0;
    for (std::size_t row = 0; row < board.size(); ++row) {
        for (std::size_t col = 0; col < board.size(); ++col) {
            const Position move{row, col};
            if (board.at(move) != Cell::Empty) continue;

            const ObakeKadokaEvaluation evaluation = evaluator_.evaluate(board, move, empty_cells);
            candidates[count++] = WeightedMove{
                move,
                evaluation,
                score_to_weight(evaluation.score, move),
            };
        }
    }
    return count;
}

double ObakeKadokaAI::score_to_weight(double score, Position move) const noexcept {
    const double scaled = std::clamp(score / config_.randomizer_temperature, -12.0, 12.0);
    double weight = std::exp(scaled) + config_.exploration_floor;

    const AttemptMemory* memory = find_recent(move);
    if (memory != nullptr) {
        weight *= memory->inferred_rejected
            ? config_.inferred_illegal_retry_penalty
            : config_.recent_retry_penalty;
    }
    return std::max(weight, 0.000001);
}

std::uint64_t ObakeKadokaAI::hash_board(const Board& board) const noexcept {
    std::uint64_t hash = 1469598103934665603ULL;
    hash ^= static_cast<std::uint64_t>(board.size());
    hash *= 1099511628211ULL;
    for (std::size_t row = 0; row < board.size(); ++row) {
        for (std::size_t col = 0; col < board.size(); ++col) {
            hash ^= static_cast<std::uint64_t>(board.at({row, col})) + 1ULL;
            hash *= 1099511628211ULL;
        }
    }
    return hash;
}

void ObakeKadokaAI::infer_previous_attempt_result(std::uint64_t current_hash) noexcept {
    if (recent_count_ == 0 || config_.memory_depth == 0) return;
    const std::size_t last_index = recent_count_ < config_.memory_depth
        ? recent_count_ - 1
        : (recent_cursor_ + config_.memory_depth - 1) % config_.memory_depth;
    AttemptMemory& last = recent_[last_index];
    if (last.valid) last.inferred_rejected = last.board_hash == current_hash;
}

const ObakeKadokaAI::AttemptMemory* ObakeKadokaAI::find_recent(Position move) const noexcept {
    for (std::size_t i = 0; i < recent_count_; ++i) {
        if (recent_[i].valid && recent_[i].move == move) return &recent_[i];
    }
    return nullptr;
}

Position ObakeKadokaAI::choose_weighted(
    const std::array<WeightedMove, 100>& candidates,
    std::size_t count) {
    double total = 0.0;
    for (std::size_t i = 0; i < count; ++i) total += candidates[i].weight;

    std::uniform_real_distribution<double> pick(0.0, total);
    double cursor = pick(rng_);
    for (std::size_t i = 0; i < count; ++i) {
        cursor -= candidates[i].weight;
        if (cursor <= 0.0) return candidates[i].move;
    }
    return candidates[count - 1].move;
}

void ObakeKadokaAI::remember(Position move, std::uint64_t board_hash) noexcept {
    if (config_.memory_depth == 0) return;
    AttemptMemory memory{move, board_hash, false, true};
    if (recent_count_ < config_.memory_depth) {
        recent_[recent_count_++] = memory;
        return;
    }
    recent_[recent_cursor_] = memory;
    recent_cursor_ = (recent_cursor_ + 1) % config_.memory_depth;
}

ObakeKadokaConfig load_obake_kadoka_config(const std::string& path) {
    if (path.empty()) return {};
    std::ifstream input(path);
    if (!input) throw std::runtime_error("failed to open Obake Kadoka model config: " + path);

    std::ostringstream buffer;
    buffer << input.rdbuf();
    const std::string text = buffer.str();

    ObakeKadokaConfig config;
    config.evaluator_weights.occupied_neighbors = read_number(text, "occupied_neighbors", config.evaluator_weights.occupied_neighbors);
    config.evaluator_weights.empty_neighbors = read_number(text, "empty_neighbors", config.evaluator_weights.empty_neighbors);
    config.evaluator_weights.mixed_color = read_number(text, "mixed_color", config.evaluator_weights.mixed_color);
    config.evaluator_weights.color_transitions = read_number(text, "color_transitions", config.evaluator_weights.color_transitions);
    config.evaluator_weights.line_interest = read_number(text, "line_interest", config.evaluator_weights.line_interest);
    config.evaluator_weights.local_density = read_number(text, "local_density", config.evaluator_weights.local_density);
    config.evaluator_weights.early_center = read_number(text, "early_center", config.evaluator_weights.early_center);
    config.recent_retry_penalty = read_number(text, "recent_retry_penalty", config.recent_retry_penalty);
    config.inferred_illegal_retry_penalty = read_number(text, "inferred_illegal_retry_penalty", config.inferred_illegal_retry_penalty);
    config.exploration_floor = read_number(text, "exploration_floor", config.exploration_floor);
    config.randomizer_temperature = read_number(text, "randomizer_temperature", config.randomizer_temperature);
    config.memory_depth = std::min<std::size_t>(read_size(text, "memory_depth", config.memory_depth), 2U);
    return config;
}

}  // namespace kadoka::othello
