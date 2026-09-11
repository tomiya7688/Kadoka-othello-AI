#include "kadoka_othello/obake_kadoka.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace kadoka::othello {
namespace {

constexpr std::array<std::pair<int, int>, 8> kDirections{{
    {-1, -1}, {-1, 0}, {-1, 1},
    {0, -1},           {0, 1},
    {1, -1},  {1, 0},  {1, 1},
}};

bool in_bounds(const Board& board, int row, int col) noexcept {
    return row >= 0 && col >= 0 &&
           row < static_cast<int>(board.size()) &&
           col < static_cast<int>(board.size());
}

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

int count_line_interest(const Board& board, Position move) {
    int interest = 0;
    for (const auto [dr, dc] : kDirections) {
        int row = static_cast<int>(move.row) + dr;
        int col = static_cast<int>(move.col) + dc;
        if (!in_bounds(board, row, col)) continue;

        const Cell first = board.at({static_cast<std::size_t>(row), static_cast<std::size_t>(col)});
        if (first == Cell::Empty) continue;

        int run = 0;
        while (in_bounds(board, row, col)) {
            const Cell current = board.at({static_cast<std::size_t>(row), static_cast<std::size_t>(col)});
            if (current != first) break;
            ++run;
            row += dr;
            col += dc;
        }

        if (run >= 1 && in_bounds(board, row, col)) {
            const Cell terminal = board.at({static_cast<std::size_t>(row), static_cast<std::size_t>(col)});
            if (terminal != Cell::Empty && terminal != first) {
                interest += std::min(run, 3);
            }
        }
    }
    return interest;
}

int count_color_transitions(const Board& board, Position move) {
    int transitions = 0;
    for (const auto [dr, dc] : kDirections) {
        int row = static_cast<int>(move.row) + dr;
        int col = static_cast<int>(move.col) + dc;
        Cell previous = Cell::Empty;
        int steps = 0;

        while (in_bounds(board, row, col) && steps < 4) {
            const Cell current = board.at({static_cast<std::size_t>(row), static_cast<std::size_t>(col)});
            if (current == Cell::Empty) break;
            if (previous != Cell::Empty && current != previous) ++transitions;
            previous = current;
            row += dr;
            col += dc;
            ++steps;
        }
    }
    return transitions;
}

}  // namespace

ObakeKadokaAI::ObakeKadokaAI(
    std::uint64_t seed,
    ObakeKadokaConfig config)
    : config_(config),
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
    for (std::size_t i = 0; i < count; ++i) {
        inspection.candidates.push_back(AICandidate{
            candidates[i].move,
            candidates[i].score,
            total_weight > 0.0 ? candidates[i].weight / total_weight : 0.0,
        });
    }

    std::size_t inferred_rejected = 0;
    for (std::size_t i = 0; i < recent_count_; ++i) {
        if (recent_[i].valid && recent_[i].inferred_rejected) ++inferred_rejected;
    }

    inspection.diagnostics.push_back({"engine", id()});
    inspection.diagnostics.push_back({"strategy", "obake_local_evaluator_plus_randomizer"});
    inspection.diagnostics.push_back({"legal_moves_used", "false"});
    inspection.diagnostics.push_back({"memory_depth", std::to_string(config_.memory_depth)});
    inspection.diagnostics.push_back({"remembered_attempts", std::to_string(recent_count_)});
    inspection.diagnostics.push_back({"inferred_rejected_attempts", std::to_string(inferred_rejected)});
    inspection.diagnostics.push_back({"candidate_count", std::to_string(count)});
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
            const double score = evaluate_position(board, move, empty_cells);
            candidates[count++] = WeightedMove{move, score, score_to_weight(score, move)};
        }
    }
    return count;
}

double ObakeKadokaAI::evaluate_position(
    const Board& board,
    Position move,
    std::size_t empty_cells) const {
    int occupied_neighbors = 0;
    int empty_neighbors = 0;
    bool touches_black = false;
    bool touches_white = false;

    for (const auto [dr, dc] : kDirections) {
        const int row = static_cast<int>(move.row) + dr;
        const int col = static_cast<int>(move.col) + dc;
        if (!in_bounds(board, row, col)) continue;

        const Cell cell = board.at({static_cast<std::size_t>(row), static_cast<std::size_t>(col)});
        if (cell == Cell::Empty) {
            ++empty_neighbors;
        } else {
            ++occupied_neighbors;
            touches_black = touches_black || cell == Cell::Black;
            touches_white = touches_white || cell == Cell::White;
        }
    }

    double score = 0.0;
    score += static_cast<double>(occupied_neighbors) * config_.occupied_neighbor_score;
    score -= static_cast<double>(empty_neighbors) * config_.empty_neighbor_penalty;
    score += static_cast<double>(occupied_neighbors * occupied_neighbors) * config_.local_density_score * 0.1;

    if (touches_black && touches_white) score += config_.mixed_color_score;
    score += static_cast<double>(count_color_transitions(board, move)) * config_.color_transition_score;
    score += static_cast<double>(count_line_interest(board, move)) * config_.line_interest_score;

    const double empty_ratio = static_cast<double>(empty_cells) /
                               static_cast<double>(board.size() * board.size());
    if (empty_ratio > 0.55) {
        const double center = (static_cast<double>(board.size()) - 1.0) * 0.5;
        const double distance =
            std::abs(static_cast<double>(move.row) - center) +
            std::abs(static_cast<double>(move.col) - center);
        score += std::max(0.0, center * 2.0 - distance) * config_.center_early_score;
    }

    return score;
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
    config.occupied_neighbor_score = read_number(text, "occupied_neighbor_score", config.occupied_neighbor_score);
    config.empty_neighbor_penalty = read_number(text, "empty_neighbor_penalty", config.empty_neighbor_penalty);
    config.mixed_color_score = read_number(text, "mixed_color_score", config.mixed_color_score);
    config.color_transition_score = read_number(text, "color_transition_score", config.color_transition_score);
    config.line_interest_score = read_number(text, "line_interest_score", config.line_interest_score);
    config.local_density_score = read_number(text, "local_density_score", config.local_density_score);
    config.center_early_score = read_number(text, "center_early_score", config.center_early_score);
    config.recent_retry_penalty = read_number(text, "recent_retry_penalty", config.recent_retry_penalty);
    config.inferred_illegal_retry_penalty = read_number(text, "inferred_illegal_retry_penalty", config.inferred_illegal_retry_penalty);
    config.exploration_floor = read_number(text, "exploration_floor", config.exploration_floor);
    config.randomizer_temperature = read_number(text, "randomizer_temperature", config.randomizer_temperature);
    config.memory_depth = std::min<std::size_t>(read_size(text, "memory_depth", config.memory_depth), 2U);
    return config;
}

}  // namespace kadoka::othello
