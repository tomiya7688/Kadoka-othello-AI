#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "kadoka_othello/ai_creator.hpp"
#include "kadoka_othello/conversion.hpp"
#include "kadoka_othello/package.hpp"
#include "kadoka_othello/package_import.hpp"
#include "kadoka_othello/package_loader.hpp"

using namespace kadoka::othello;

namespace {

void print_usage() {
    std::cout
        << "Kadoka Othello AI Creator\n"
        << "  analyze <manifest.json> [position.txt]\n"
        << "  batch <manifest.json> <position-list.txt>\n"
        << "  compare <position.txt> <manifest-a.json> <manifest-b.json> [...]\n"
        << "  benchmark <manifest.json> <iterations> [position.txt]\n"
        << "  import <python|external_process> <source> <destination-dir> <id> <name>\n"
        << "  formats\n";
}

CreatorPosition make_initial_position(std::size_t board_size) {
    CreatorPosition position(board_size);
    position.player = Player::Black;
    position.ply = 0;
    return position;
}

LoadedAIPackage load_manifest_package(const std::string& manifest_path, std::uint64_t seed) {
    return load_ai_package(load_ai_manifest(manifest_path), seed);
}

void print_inspection(const AICreatorRunResult& result) {
    std::cout << "model: " << result.model_id << '\n';
    std::cout << "move: " << result.inspection.output.move.row
              << ',' << result.inspection.output.move.col << '\n';
    std::cout << "elapsed_us: " << result.elapsed_us << '\n';
    for (const auto& candidate : result.inspection.candidates) {
        std::cout << "candidate " << candidate.move.row << ',' << candidate.move.col
                  << " value=" << candidate.value
                  << " policy=" << candidate.policy << '\n';
    }
    for (const auto& diagnostic : result.inspection.diagnostics) {
        std::cout << "diag " << diagnostic.key << '=' << diagnostic.value << '\n';
    }
}

int command_analyze(int argc, char** argv) {
    if (argc < 3) { print_usage(); return 2; }
    CreatorPosition position = argc > 3 ? load_creator_position(argv[3]) : make_initial_position(8);
    LoadedAIPackage package = load_manifest_package(argv[2], 12345);
    print_inspection(run_creator_inference(package, position));
    return 0;
}

int command_batch(int argc, char** argv) {
    if (argc < 4) { print_usage(); return 2; }
    LoadedAIPackage package = load_manifest_package(argv[2], 12345);
    const auto paths = load_position_list(argv[3]);
    const auto items = run_batch_analysis(package, paths);
    for (const auto& item : items) {
        std::cout << "position: " << item.position_path << '\n';
        print_inspection(item.result);
    }
    return 0;
}

int command_compare(int argc, char** argv) {
    if (argc < 5) { print_usage(); return 2; }
    CreatorPosition position = load_creator_position(argv[2]);
    std::vector<LoadedAIPackage> packages;
    for (int i = 3; i < argc; ++i) {
        packages.push_back(load_manifest_package(argv[i], static_cast<std::uint64_t>(12345 + i)));
    }
    const auto results = compare_creator_ais(packages, position);
    for (const auto& result : results) print_inspection(result);
    return 0;
}

int command_benchmark(int argc, char** argv) {
    if (argc < 4) { print_usage(); return 2; }
    const std::size_t iterations = static_cast<std::size_t>(std::strtoull(argv[3], nullptr, 10));
    CreatorPosition position = argc > 4 ? load_creator_position(argv[4]) : make_initial_position(8);
    LoadedAIPackage package = load_manifest_package(argv[2], 12345);
    const auto result = benchmark_creator_ai(package, position, iterations);
    std::cout << "model: " << result.model_id << '\n'
              << "iterations: " << result.iterations << '\n'
              << "total_us: " << result.total_us << '\n'
              << "average_us: " << result.average_us << '\n'
              << "min_us: " << result.min_us << '\n'
              << "max_us: " << result.max_us << '\n';
    return 0;
}

int command_import(int argc, char** argv) {
    if (argc < 7) { print_usage(); return 2; }
    AIPackageImportRequest request;
    const std::string type = argv[2];
    if (type == "python") request.interface_type = AIPackageInterface::Python;
    else if (type == "external_process") request.interface_type = AIPackageInterface::ExternalProcess;
    else throw std::invalid_argument("import type must be external_process or python");
    request.source_path = argv[3];
    request.destination_directory = argv[4];
    request.id = argv[5];
    request.name = argv[6];
    const std::string manifest = import_ai_package(request);
    std::cout << "imported manifest: " << manifest << '\n';
    return 0;
}

int command_formats() {
    for (const auto& mapping : builtin_format_mappings()) {
        std::cout << mapping.format_id << " (" << mapping.display_name << ")"
                  << " import=" << (mapping.supports_import ? "yes" : "no")
                  << " export=" << (mapping.supports_export ? "yes" : "no") << '\n';
        for (const auto& field : mapping.fields) {
            std::cout << "  " << field.kadoka_field << " -> " << field.external_field << '\n';
        }
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 2) { print_usage(); return 0; }
        const std::string command = argv[1];
        if (command == "analyze") return command_analyze(argc, argv);
        if (command == "batch") return command_batch(argc, argv);
        if (command == "compare") return command_compare(argc, argv);
        if (command == "benchmark") return command_benchmark(argc, argv);
        if (command == "import") return command_import(argc, argv);
        if (command == "formats") return command_formats();
        print_usage();
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "AI Creator error: " << error.what() << '\n';
        return 1;
    }
}
