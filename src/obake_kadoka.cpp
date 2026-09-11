#include "kadoka_othello/obake_kadoka.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>

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
    if (end == start) return fallback;
    return std::stod(text.substr(start, end - start));
}

std::size_t read_size(const std::string& text, const std::string& key, std::size_t fallback) {
    const double value = read_number(text, key, static_cast<double>(fallback));
    return value < 0.0 ? fallback : static_cast<std::size_t>(value);
}

}  // namespace

ObakeKadokaAI::ObakeKadokaAI(
    std::uint64_t seed,
    ObakeKadokaConfig config)
    : config_(config),
      rng_(seed == 0 ? std::random_device{}() : seed) {
    config_.memory_depth = std::min<std::size_t>(config_.memory_depth, recent_.size());
}

std::string ObakeKadokaAI::id() const {
    return "kadoka.obake_kadoka";
}

AIOutput ObakeKadokaAI::think(const AdaptedAIInput& input) {
    if (input.board == nullptr) {
        throw std::invalid_argument("ObakeKadokaAI requires board input");
    }

    std::array<WeightedMove, 100> candidates{};
    const std::size_t count = collect_candidates(*input.board, candidates);
    if (count == 0) {
        throw std::runtime_error("ObakeKadokaAI found no empty square");
    }

    const Position move = choose_weighted(candidates, count);
    remember(move);
    return AIOutput{move};
}

AIInspection ObakeKadokaAI::inspect(const AdaptedAIInput& input) {
    if (input.board == nullptr) {
        throw std::invalid_argument("ObakeKadokaAI requires board input");
    }

    std::array<WeightedMove, 100> candidates{};
    const std::size_t count = collect_candidates(*input.board, candidates);
    if (count == 0) {
        throw std::runtime_error("ObakeKadokaAI found no empty square");
    }

    double total_weight = 0.0;
    for (std::size_t i = 0; i < count; ++i) total_weight += candidates[i].weight;

    const Position selected = choose_weighted(candidates, count);
    remember(selected);

    AIInspection inspection;
    inspection.output = AIOutput{selected};
    inspection.candidates.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        inspection.candidates.push_back(AICandidate{
            candidates[i].move,
            candidates[i].weight,
            total_weight > 0.0 ? candidates[i].weight / total_weight : 0.0,
        });
    }
    inspection.diagnostics.push_back({"engine", id()});
    inspection.diagnostics.push_back({"strategy", "weighted_empty_square_randomizer"});
    inspection.diagnostics.push_back({"legal_moves_used", "false"});
    inspection.diagnostics.push_back({"memory_depth", std::to_string(config_.memory_depth)});
    inspection.diagnostics.push_back({"candidate_count", std::to_string(count)});
    return inspection;
}

std::size_t ObakeKadokaAI::collect_candidates(
    const Board& board,
    std::array<WeightedMove, 100>& candidates) const {
    if (board.size() * board.size() > candidates.size()) {
        throw std::invalid_argument("ObakeKadokaAI supports boards up to 10x10");
    }

    std::size_t count = 0;
    for (std::size_t row = 0; row < board.size(); ++row) {
        for (std::size_t col = 0; col < board.size(); ++col) {
            const Position move{row, col};
            if (board.at(move) != Cell::Empty) continue;
            candidates[count++] = WeightedMove{move, score_position(board, move)};
        }
    }
    return count;
}

double ObakeKadokaAI::score_position(const Board& board, Position move) const {
    const std::size_t last = board.size() - 1;
    const bool corner =
        (move.row == 0 || move.row == last) &&
        (move.col == 0 || move.col == last);
    const bool edge = move.row == 0 || move.row == last || move.col == 0 || move.col == last;

    double score = config_.exploration_floor;
    if (corner) score += config_.corner_weight;
    else if (edge) score += config_.edge_weight;

    int occupied_neighbors = 0;
    int directional_lines = 0;
    for (const auto [dr, dc] : kDirections) {
        const int row = static_cast<int>(move.row) + dr;
        const int col = static_cast<int>(move.col) + dc;
        if (!in_bounds(board, row, col)) continue;
        const Cell neighbor = board.at({static_cast<std::size_t>(row), static_cast<std::size_t>(col)});
        if (neighbor == Cell::Empty) continue;
        ++occupied_neighbors;

        int row2 = row + dr;
        int col2 = col + dc;
        while (in_bounds(board, row2, col2)) {
            const Cell next = board.at({static_cast<std::size_t>(row2), static_cast<std::size_t>(col2)});
            if (next == Cell::Empty) break;
            if (next != neighbor) {
                ++directional_lines;
                break;
            }
            row2 += dr;
            col2 += dc;
        }
    }

    score += static_cast<double>(occupied_neighbors) * config_.near_piece_weight;
    score += static_cast<double>(directional_lines) * config_.center_weight;

    const double center = (static_cast<double>(board.size()) - 1.0) * 0.5;
    const double dr = std::abs(static_cast<double>(move.row) - center);
    const double dc = std::abs(static_cast<double>(move.col) - center);
    const double center_bonus = std::max(0.0, center - (dr + dc) * 0.5);
    score += center_bonus * config_.center_weight;

    if (is_recent(move)) score *= config_.recent_retry_penalty;
    return std::max(score, 0.000001);
}

bool ObakeKadokaAI::is_recent(Position move) const noexcept {
    for (std::size_t i = 0; i < recent_count_; ++i) {
        if (recent_[i] == move) return true;
    }
    return false;
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

void ObakeKadokaAI::remember(Position move) noexcept {
    if (config_.memory_depth == 0) return;
    if (recent_count_ < config_.memory_depth) {
        recent_[recent_count_++] = move;
        return;
    }
    recent_[recent_cursor_] = move;
    recent_cursor_ = (recent_cursor_ + 1) % config_.memory_depth;
}

ObakeKadokaConfig load_obake_kadoka_config(const std::string& path) {
    if (path.empty()) return {};
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("failed to open Obake Kadoka model config: " + path);
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    const std::string text = buffer.str();

    ObakeKadokaConfig config;
    config.corner_weight = read_number(text, "corner_weight", config.corner_weight);
    config.edge_weight = read_number(text, "edge_weight", config.edge_weight);
    config.near_piece_weight = read_number(text, "near_piece_weight", config.near_piece_weight);
    config.center_weight = read_number(text, "center_weight", config.center_weight);
    config.recent_retry_penalty = read_number(text, "recent_retry_penalty", config.recent_retry_penalty);
    config.exploration_floor = read_number(text, "exploration_floor", config.exploration_floor);
    config.memory_depth = std::min<std::size_t>(
        read_size(text, "memory_depth", config.memory_depth), 2U);
    return config;
}

}  // namespace kadoka::othello
