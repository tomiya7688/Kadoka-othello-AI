#include "kadoka_othello/obake_maru.hpp"

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
            text[end] == 'e' || text[end] == 'E')) ++end;
    return end == start ? fallback : std::stod(text.substr(start, end - start));
}

}  // namespace

ObakeMaruAI::ObakeMaruAI(
    std::uint64_t seed,
    ObakeMaruConfig config)
    : config_(config),
      rng_(seed == 0 ? std::random_device{}() : seed) {
    config_.randomizer_temperature = std::max(config_.randomizer_temperature, 0.05);
    config_.exploration_floor = std::max(config_.exploration_floor, 0.0);
}

std::string ObakeMaruAI::id() const {
    return "kadoka.obake_maru";
}

AIOutput ObakeMaruAI::think(const AdaptedAIInput& input) {
    if (input.board == nullptr) throw std::invalid_argument("ObakeMaruAI requires board input");

    const std::uint64_t hash = hash_board(*input.board);
    infer_last_result(hash);

    std::array<Candidate, 100> candidates{};
    const std::size_t count = collect_candidates(*input.board, candidates);
    if (count == 0) throw std::runtime_error("ObakeMaruAI found no empty square");

    const Position move = choose(candidates, count);
    remember(move, hash);
    return AIOutput{move};
}

AIInspection ObakeMaruAI::inspect(const AdaptedAIInput& input) {
    if (input.board == nullptr) throw std::invalid_argument("ObakeMaruAI requires board input");

    const std::uint64_t hash = hash_board(*input.board);
    infer_last_result(hash);

    std::array<Candidate, 100> candidates{};
    const std::size_t count = collect_candidates(*input.board, candidates);
    if (count == 0) throw std::runtime_error("ObakeMaruAI found no empty square");

    double total = 0.0;
    for (std::size_t i = 0; i < count; ++i) total += candidates[i].weight;

    const Position move = choose(candidates, count);
    remember(move, hash);

    AIInspection inspection;
    inspection.output = AIOutput{move};
    inspection.candidates.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        inspection.candidates.push_back(AICandidate{
            candidates[i].move,
            candidates[i].score,
            total > 0.0 ? candidates[i].weight / total : 0.0,
        });
    }
    inspection.diagnostics.push_back({"engine", id()});
    inspection.diagnostics.push_back({"strategy", "simple_local_interest_plus_high_randomness"});
    inspection.diagnostics.push_back({"legal_moves_used", "false"});
    inspection.diagnostics.push_back({"memory_depth", "1"});
    inspection.diagnostics.push_back({"last_rejected", last_rejected_ ? "true" : "false"});
    return inspection;
}

std::size_t ObakeMaruAI::collect_candidates(
    const Board& board,
    std::array<Candidate, 100>& candidates) const {
    if (board.size() * board.size() > candidates.size()) {
        throw std::invalid_argument("ObakeMaruAI supports boards up to 10x10");
    }

    std::size_t count = 0;
    for (std::size_t row = 0; row < board.size(); ++row) {
        for (std::size_t col = 0; col < board.size(); ++col) {
            const Position move{row, col};
            if (board.at(move) != Cell::Empty) continue;
            const double score = evaluate_position(board, move);
            candidates[count++] = Candidate{move, score, to_weight(score, move)};
        }
    }
    return count;
}

double ObakeMaruAI::evaluate_position(const Board& board, Position move) const {
    int occupied = 0;
    int black = 0;
    int white = 0;

    for (const auto [dr, dc] : kDirections) {
        const int row = static_cast<int>(move.row) + dr;
        const int col = static_cast<int>(move.col) + dc;
        if (!in_bounds(board, row, col)) continue;
        const Cell cell = board.at({static_cast<std::size_t>(row), static_cast<std::size_t>(col)});
        if (cell == Cell::Black) { ++occupied; ++black; }
        if (cell == Cell::White) { ++occupied; ++white; }
    }

    double score = static_cast<double>(occupied) * config_.occupied_neighbor_weight;
    score += static_cast<double>(occupied * occupied) * config_.local_cluster_weight * 0.1;
    if (black > 0 && white > 0) score += config_.mixed_color_weight;

    const double center = (static_cast<double>(board.size()) - 1.0) * 0.5;
    const double distance =
        std::abs(static_cast<double>(move.row) - center) +
        std::abs(static_cast<double>(move.col) - center);
    score += std::max(0.0, center * 2.0 - distance) * config_.center_bias_weight;
    return score;
}

double ObakeMaruAI::to_weight(double score, Position move) const noexcept {
    const double scaled = std::clamp(score / config_.randomizer_temperature, -10.0, 10.0);
    double weight = std::exp(scaled) + config_.exploration_floor;
    if (has_memory_ && last_rejected_ && last_move_ == move) {
        weight *= config_.rejected_retry_penalty;
    }
    return std::max(weight, 0.000001);
}

std::uint64_t ObakeMaruAI::hash_board(const Board& board) const noexcept {
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

void ObakeMaruAI::infer_last_result(std::uint64_t current_hash) noexcept {
    if (!has_memory_) return;
    last_rejected_ = last_board_hash_ == current_hash;
}

Position ObakeMaruAI::choose(
    const std::array<Candidate, 100>& candidates,
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

void ObakeMaruAI::remember(Position move, std::uint64_t board_hash) noexcept {
    last_move_ = move;
    last_board_hash_ = board_hash;
    has_memory_ = true;
    last_rejected_ = false;
}

ObakeMaruConfig load_obake_maru_config(const std::string& path) {
    if (path.empty()) return {};
    std::ifstream input(path);
    if (!input) throw std::runtime_error("failed to open Obake Maru model config: " + path);

    std::ostringstream buffer;
    buffer << input.rdbuf();
    const std::string text = buffer.str();

    ObakeMaruConfig config;
    config.occupied_neighbor_weight = read_number(text, "occupied_neighbor_weight", config.occupied_neighbor_weight);
    config.local_cluster_weight = read_number(text, "local_cluster_weight", config.local_cluster_weight);
    config.mixed_color_weight = read_number(text, "mixed_color_weight", config.mixed_color_weight);
    config.center_bias_weight = read_number(text, "center_bias_weight", config.center_bias_weight);
    config.exploration_floor = read_number(text, "exploration_floor", config.exploration_floor);
    config.randomizer_temperature = read_number(text, "randomizer_temperature", config.randomizer_temperature);
    config.rejected_retry_penalty = read_number(text, "rejected_retry_penalty", config.rejected_retry_penalty);
    return config;
}

}  // namespace kadoka::othello
