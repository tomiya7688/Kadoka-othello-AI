#include "kadoka_othello/ai_creator.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>


namespace kadoka::othello {
namespace {

Player parse_player(const std::string& value) {
    if (value == "black" || value == "Black" || value == "B") return Player::Black;
    if (value == "white" || value == "White" || value == "W") return Player::White;
    throw std::invalid_argument("player must be black or white");
}

Cell parse_cell(char value) {
    if (value == '.' || value == '0') return Cell::Empty;
    if (value == 'B' || value == 'b' || value == '1') return Cell::Black;
    if (value == 'W' || value == 'w' || value == '2') return Cell::White;
    throw std::invalid_argument("unsupported board cell character");
}

}  // namespace

CreatorPosition load_creator_position(const std::string& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("failed to open position file: " + path);

    std::size_t size = 0;
    std::string player_text;
    std::size_t ply = 0;
    input >> size >> player_text >> ply;
    if (!input || size < 4 || size % 2 != 0) throw std::runtime_error("invalid position header");

    CreatorPosition position(size);
    position.player = parse_player(player_text);
    position.ply = ply;

    for (std::size_t row = 0; row < size; ++row) {
        std::string line;
        input >> line;
        if (line.size() != size) throw std::runtime_error("invalid board row length");
        for (std::size_t col = 0; col < size; ++col) {
            position.board.set({row, col}, parse_cell(line[col]));
        }
    }
    return position;
}

std::vector<std::string> load_position_list(const std::string& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("failed to open position list: " + path);
    std::vector<std::string> paths;
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty() || line[0] == '#') continue;
        paths.push_back(line);
    }
    return paths;
}

AICreatorRunResult run_creator_inference(
    LoadedAIPackage& package,
    const CreatorPosition& position) {
    const AIInput input{&position.board, position.player, {}};
    const auto start = std::chrono::steady_clock::now();
    AIInspection inspection = inspect_ai(package.view(), input);
    const auto end = std::chrono::steady_clock::now();
    const double elapsed_us = std::chrono::duration<double, std::micro>(end - start).count();
    return AICreatorRunResult{package.manifest.id, std::move(inspection), elapsed_us};
}

AIBenchmarkResult benchmark_creator_ai(
    LoadedAIPackage& package,
    const CreatorPosition& position,
    std::size_t iterations) {
    if (iterations == 0) throw std::invalid_argument("benchmark iterations must be greater than zero");
    const AIInput input{&position.board, position.player, {}};
    double total = 0.0;
    double minimum = std::numeric_limits<double>::max();
    double maximum = 0.0;
    for (std::size_t i = 0; i < iterations; ++i) {
        const auto start = std::chrono::steady_clock::now();
        static_cast<void>(inspect_ai(package.view(), input));
        const auto end = std::chrono::steady_clock::now();
        const double elapsed = std::chrono::duration<double, std::micro>(end - start).count();
        total += elapsed;
        minimum = std::min(minimum, elapsed);
        maximum = std::max(maximum, elapsed);
    }
    return AIBenchmarkResult{package.manifest.id, iterations, total,
        total / static_cast<double>(iterations), minimum, maximum};
}

std::vector<AICreatorRunResult> compare_creator_ais(
    std::vector<LoadedAIPackage>& packages,
    const CreatorPosition& position) {
    std::vector<AICreatorRunResult> results;
    results.reserve(packages.size());
    for (auto& package : packages) results.push_back(run_creator_inference(package, position));
    return results;
}

std::vector<BatchAnalysisItem> run_batch_analysis(
    LoadedAIPackage& package,
    const std::vector<std::string>& position_paths) {
    std::vector<BatchAnalysisItem> items;
    items.reserve(position_paths.size());
    for (const auto& path : position_paths) {
        CreatorPosition position = load_creator_position(path);
        AICreatorRunResult result = run_creator_inference(package, position);
        items.emplace_back(path, std::move(position), std::move(result));
    }
    return items;
}

}  // namespace kadoka::othello
