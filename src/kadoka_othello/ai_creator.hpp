#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "kadoka_othello/ai.hpp"
#include "kadoka_othello/package_loader.hpp"

namespace kadoka::othello {

struct CreatorPosition {
    Board board;
    Player player{Player::Black};
    std::size_t ply{};

    explicit CreatorPosition(std::size_t board_size = 8)
        : board(board_size) {}
};

struct AICreatorRunResult {
    std::string model_id;
    AIInspection inspection;
    double elapsed_us{};
};

struct AIBenchmarkResult {
    std::string model_id;
    std::size_t iterations{};
    double total_us{};
    double average_us{};
    double min_us{};
    double max_us{};
};

[[nodiscard]] CreatorPosition load_creator_position(const std::string& path);

[[nodiscard]] AICreatorRunResult run_creator_inference(
    LoadedAIPackage& package,
    const CreatorPosition& position);

[[nodiscard]] AIBenchmarkResult benchmark_creator_ai(
    LoadedAIPackage& package,
    const CreatorPosition& position,
    std::size_t iterations);

[[nodiscard]] std::vector<AICreatorRunResult> compare_creator_ais(
    std::vector<LoadedAIPackage>& packages,
    const CreatorPosition& position);

}  // namespace kadoka::othello
