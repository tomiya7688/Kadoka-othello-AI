#include "kadoka_othello/obake_kadoka.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
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
    if (end == start) return fallback;
    return std::stod(text.substr(start, end - start));
}

std::size_t read_size(const std::string& text, const std::string& key, std::size_t fallback) {
    const double value = read_number(text, key, static_cast<double>(fallback));
    return value < 0.0 ? fallback : static_cast<std::size_t>(value);
}

bool is_corner(std::size_t size, Position move) noexcept {
    const std::size_t last = size - 1;
    return (move.row == 0 || move.row == last) &&
           (move.col == 0 || move.col == last);
}

bool is_edge(std::size_t size, Position move) noexcept {
    const std::size_t last = size - 1;
    return move.row == 0 || move.row == last || move.col == 0 || move.col == last;
}

bool is_x_square(std::size_t size, Position move, Position& corner) noexcept {
    const std::size_t last = size - 1;
    if (move.row == 1 && move.col == 1) { corner = {0, 0}; return true; }
    if (move.row == 1 && move.col + 2 == size) { corner = {0, last}; return true; }
    if (move.row + 2 == size && move.col == 1) { corner = {last, 0}; return true; }
    if (move.row + 2 == size && move.col + 2 == size) { corner = {last, last}; return true; }
    return false;
}

bool is_c_square(std::size_t size, Position move, Position& corner) noexcept {
    const std::size_t last = size - 1;
    if (move.row == 0 && move.col == 1) { corner = {0, 0}; return true; }
    if (move.row == 1 && move.col == 0) { corner = {0, 0}; return true; }
    if (move.row == 0 && move.col + 2 == size) { corner = {0, last}; return true; }
    if (move.row == 1 && move.col == last) { corner = {0, last}; return true; }
    if (move.row == last && move.col == 1) { corner = {last, 0}; return true; }
    if (move.row + 2 == size && move.col == 0) { corner = {last, 0}; return true; }
    if (move.row == last && move.col + 2 == size) { corner = {last, last}; return true; }
    if (move.row + 2 == size && move.col == last) { corner = {last, last}; return true; }
    return false;
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

int line_potential(const Board& board, Position move) {
    int potential = 0;

    for (const auto [dr, dc] : kDirections) {
        int row = static_cast<int>(move.row) + dr;
        int col = static_cast<int>(move.col) + dc;
        if (!in_bounds(board, row, col)) continue;

        const Cell first = board.at({static_cast<std::size_t>(row), static_cast<std::size_t>(col)});
        if (first == Cell::Empty) continue;

        int length = 0;
        while (in_bounds(board, row, col)) {
            const Cell current = board.at({static_cast<std::size_t>(row), static_cast<std::size_t>(col)});
            if (current != first) break;
            ++length;
            row += dr;
            col += dc;
        }

        if (length == 0 || !in_bounds(board, row, col)) continue;
        const Cell terminal = board.at({static_cast<std::size_t>(row), static_cast<std::size_t>(col)});
        if (terminal != Cell::Empty && terminal != first) ++potential;
    }

    return potential;
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
            candidates[i].score,
            total_weight > 0.0 ? candidates[i].weight / total_weight : 0.0,
        });
    }
    inspection.diagnostics.push_back({"engine", id()});
    inspection.diagnostics.push_back({"strategy", "obake_square_evaluation_plus_weighted_randomizer"});
    inspection.diagnostics.push_back({"legal_moves_used", "false"});
    inspection.diagnostics.push_back({"memory_depth", std::to_string(config_.memory_depth)});
    inspection.diagnostics.push_back({"candidate_count", std::to_string(count)});
    inspection.diagnostics.push_back({"randomizer_temperature", std::to_string(config_.randomizer_temperature)});
    return inspection;
}

std::size_t ObakeKadokaAI::collect_candidates(
    const Board& board,
    std::array<WeightedMove, 100>& candidates) const {
    if (board.size() * board.size() > candidates.size()) {
        throw std::invalid_argument("ObakeKadokaAI supports boards up to 10x10");
    }

    const std::size_t empty_cells = count_empty_cells(board);
    std::size_t count = 0;
    for (std::size_t row = 0; row < board.size(); ++row) {
        for (std::size_t col = 0; col < board.size(); ++col) {
            const Position move{row, col};
            if (board.at(move) != Cell::Empty) continue;

            const double score = evaluate_position(board, move, empty_cells);
            candidates[count++] = WeightedMove{
                move,
                score,
                score_to_weight(score, is_recent(move)),
            };
        }
    }
    return count;
}

double ObakeKadokaAI::evaluate_position(
    const Board& board,
    Position move,
    std::size_t empty_cells) const {
    const std::size_t size = board.size();
    double score = 0.0;

    if (is_corner(size, move)) score += config_.corner_score;
    else if (is_edge(size, move)) score += config_.edge_score;

    Position related_corner{};
    if (is_x_square(size, move, related_corner) && board.at(related_corner) == Cell::Empty) {
        score -= config_.x_square_penalty;
    }
    if (is_c_square(size, move, related_corner) && board.at(related_corner) == Cell::Empty) {
        score -= config_.c_square_penalty;
    }

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

    score += static_cast<double>(occupied_neighbors) * config_.occupied_neighbor_score;
    score -= static_cast<double>(empty_neighbors) * config_.empty_neighbor_penalty;
    if (touches_black && touches_white) score += config_.mixed_color_score;

    score += static_cast<double>(line_potential(board, move)) * config_.line_potential_score;

    const double empty_ratio = static_cast<double>(empty_cells) /
                               static_cast<double>(size * size);
    if (empty_ratio > 0.55) {
        const double center = (static_cast<double>(size) - 1.0) * 0.5;
        const double distance =
            std::abs(static_cast<double>(move.row) - center) +
            std::abs(static_cast<double>(move.col) - center);
        const double normalized = std::max(0.0, center * 2.0 - distance);
        score += normalized * config_.center_early_score;
    }

    return score;
}

double ObakeKadokaAI::score_to_weight(double score, bool recent) const noexcept {
    const double scaled = std::clamp(
        score / config_.randomizer_temperature,
        -12.0,
        12.0);
    double weight = std::exp(scaled) + config_.exploration_floor;
    if (recent) weight *= config_.recent_retry_penalty;
    return std::max(weight, 0.000001);
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
    config.corner_score = read_number(text, "corner_score", config.corner_score);
    config.edge_score = read_number(text, "edge_score", config.edge_score);
    config.x_square_penalty = read_number(text, "x_square_penalty", config.x_square_penalty);
    config.c_square_penalty = read_number(text, "c_square_penalty", config.c_square_penalty);
    config.occupied_neighbor_score = read_number(text, "occupied_neighbor_score", config.occupied_neighbor_score);
    config.empty_neighbor_penalty = read_number(text, "empty_neighbor_penalty", config.empty_neighbor_penalty);
    config.mixed_color_score = read_number(text, "mixed_color_score", config.mixed_color_score);
    config.line_potential_score = read_number(text, "line_potential_score", config.line_potential_score);
    config.center_early_score = read_number(text, "center_early_score", config.center_early_score);
    config.recent_retry_penalty = read_number(text, "recent_retry_penalty", config.recent_retry_penalty);
    config.exploration_floor = read_number(text, "exploration_floor", config.exploration_floor);
    config.randomizer_temperature = read_number(text, "randomizer_temperature", config.randomizer_temperature);
    config.memory_depth = std::min<std::size_t>(
        read_size(text, "memory_depth", config.memory_depth), 2U);
    return config;
}

}  // namespace kadoka::othello
