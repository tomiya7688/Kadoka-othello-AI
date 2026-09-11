#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "kadoka_othello/ai_creator.hpp"
#include "kadoka_othello/conversion.hpp"
#include "kadoka_othello/model_data.hpp"
#include "kadoka_othello/package.hpp"
#include "kadoka_othello/package_loader.hpp"
#include "kadoka_othello/rules.hpp"

using namespace kadoka::othello;

namespace {

CreatorPosition make_initial_position(std::size_t board_size) {
    return CreatorPosition(board_size);
}

ModelRecord make_creator_record(
    const CreatorPosition& position,
    const AICreatorRunResult& result,
    const std::string& game_id) {
    ModelRecord record;
    record.model_id = result.model_id;
    record.game_id = game_id;
    record.ply = position.ply;
    record.board_size = position.board.size();
    record.player = position.player;
    record.legal_moves = rules::legal_moves(position.board, position.player);
    record.selected_move = result.inspection.output.move;
    record.diagnostics = result.inspection.diagnostics;
    record.diagnostics.push_back({"creator_elapsed_us", std::to_string(result.elapsed_us)});
    for (const auto& candidate : result.inspection.candidates) {
        record.candidates.push_back({candidate.move, candidate.value, candidate.policy});
    }

    for (std::size_t row = 0; row < record.board_size; ++row) {
        for (std::size_t col = 0; col < record.board_size; ++col) {
            record.board.push_back(position.board.at({row, col}));
        }
    }
    return record;
}

void print_inspection(const AICreatorRunResult& result) {
    std::cout << "model: " << result.model_id << '\n';
    std::cout << "selected move: row=" << result.inspection.output.move.row
              << " col=" << result.inspection.output.move.col << '\n';
    std::cout << "elapsed_us: " << std::fixed << std::setprecision(3)
              << result.elapsed_us << '\n';

    std::cout << "candidates:\n";
    for (const auto& candidate : result.inspection.candidates) {
        std::cout << "  (" << candidate.move.row << ',' << candidate.move.col << ")"
                  << " value=" << candidate.value
                  << " policy=" << candidate.policy << '\n';
    }

    std::cout << "diagnostics:\n";
    for (const auto& item : result.inspection.diagnostics) {
        std::cout << "  " << item.key << " = " << item.value << '\n';
    }
}

LoadedAIPackage load_manifest_package(const std::string& manifest_path, std::uint64_t seed) {
    return load_ai_package(load_ai_manifest(manifest_path), seed);
}

void print_usage() {
    std::cout
        << "Kadoka Othello AI Creator\n"
        << "\n"
        << "analyze <manifest> [position.txt] [output.jsonl]\n"
        << "compare <position.txt> <manifest1> <manifest2> [...]\n"
        << "benchmark <manifest> <iterations> [position.txt]\n"
        << "formats\n"
        << "\n"
        << "position.txt format:\n"
        << "  <board_size> <black|white> <ply>\n"
        << "  followed by board_size rows using B/W/.\n";
}

int command_analyze(int argc, char** argv) {
    if (argc < 3) {
        print_usage();
        return 2;
    }

    const std::string manifest_path = argv[2];
    const bool has_position = argc > 3;
    const std::string position_path = has_position ? argv[3] : "";
    const std::string output_path = argc > 4 ? argv[4] : "ai_creator_output.jsonl";

    CreatorPosition position = has_position
        ? load_creator_position(position_path)
        : make_initial_position(8);

    LoadedAIPackage package = load_manifest_package(manifest_path, 12345);
    AICreatorRunResult result = run_creator_inference(package, position);
    print_inspection(result);

    std::ofstream output(output_path, std::ios::app);
    if (!output) {
        std::cerr << "failed to open output: " << output_path << '\n';
        return 3;
    }
    KadokaJsonlCodec codec;
    codec.write(output, make_creator_record(position, result, "ai-creator"));
    std::cout << "record codec: " << codec.id() << '\n';
    std::cout << "output: " << output_path << '\n';
    return 0;
}

int command_compare(int argc, char** argv) {
    if (argc < 5) {
        print_usage();
        return 2;
    }

    CreatorPosition position = load_creator_position(argv[2]);
    std::vector<LoadedAIPackage> packages;
    for (int i = 3; i < argc; ++i) {
        packages.push_back(load_manifest_package(argv[i], 12345 + static_cast<std::uint64_t>(i)));
    }

    const auto results = compare_creator_ais(packages, position);
    for (std::size_t i = 0; i < results.size(); ++i) {
        if (i != 0) std::cout << "\n---\n";
        print_inspection(results[i]);
    }
    return 0;
}

int command_benchmark(int argc, char** argv) {
    if (argc < 4) {
        print_usage();
        return 2;
    }

    const std::string manifest_path = argv[2];
    const std::size_t iterations = static_cast<std::size_t>(std::stoull(argv[3]));
    CreatorPosition position = argc > 4
        ? load_creator_position(argv[4])
        : make_initial_position(8);

    LoadedAIPackage package = load_manifest_package(manifest_path, 12345);
    const auto result = benchmark_creator_ai(package, position, iterations);
    std::cout << "model: " << result.model_id << '\n';
    std::cout << "iterations: " << result.iterations << '\n';
    std::cout << "total_us: " << result.total_us << '\n';
    std::cout << "average_us: " << result.average_us << '\n';
    std::cout << "min_us: " << result.min_us << '\n';
    std::cout << "max_us: " << result.max_us << '\n';
    return 0;
}

int command_formats() {
    for (const auto& mapping : builtin_format_mappings()) {
        std::cout << mapping.id << " (" << (mapping.lossy ? "lossy" : "lossless") << ")\n";
        for (const auto& field : mapping.fields) {
            std::cout << "  " << field.kadoka_field << " -> " << field.external_field << '\n';
        }
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 2) {
            print_usage();
            return 0;
        }

        const std::string command = argv[1];
        if (command == "analyze") return command_analyze(argc, argv);
        if (command == "compare") return command_compare(argc, argv);
        if (command == "benchmark") return command_benchmark(argc, argv);
        if (command == "formats") return command_formats();

        print_usage();
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "AI Creator error: " << error.what() << '\n';
        return 1;
    }
}
