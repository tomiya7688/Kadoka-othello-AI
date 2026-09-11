#include <fstream>
#include <iostream>
#include <string>

#include "kadoka_othello/model_data.hpp"
#include "kadoka_othello/package.hpp"
#include "kadoka_othello/package_loader.hpp"

using namespace kadoka::othello;

int main(int argc, char** argv) {
    const std::string manifest_path = argc > 1 ? argv[1] : "src/packages/random/manifest.json";
    const std::string output_path = argc > 2 ? argv[2] : "ai_creator_sample.jsonl";
    const std::size_t board_size = argc > 3 ? static_cast<std::size_t>(std::stoul(argv[3])) : 8U;

    try {
        const AIPackageManifest manifest = load_ai_manifest(manifest_path);
        LoadedAIPackage package = load_ai_package(manifest, 12345);

        Game game(board_size);
        const auto legal_moves = game.legal_moves();
        const AIInput input{&game.board(), &legal_moves};
        const AIInspection inspection = inspect_ai(package.view(), input);

        std::ofstream output(output_path, std::ios::app);
        if (!output) {
            std::cerr << "failed to open output file: " << output_path << '\n';
            return 2;
        }

        KadokaJsonlCodec codec;
        ModelRecord record = make_model_record(
            game,
            manifest.id,
            "mock-game-0001",
            inspection);
        codec.write(output, record);

        std::cout << "AI package: " << manifest.name << " (" << manifest.id << ")\n";
        std::cout << "interface: " << package_interface_name(manifest.interface_type) << '\n';
        std::cout << "adapter: " << manifest.adapter << '\n';
        std::cout << "selected move: row=" << inspection.output.move.row
                  << " col=" << inspection.output.move.col << '\n';
        std::cout << "diagnostics:\n";
        for (const auto& item : inspection.diagnostics) {
            std::cout << "  " << item.key << " = " << item.value << '\n';
        }
        std::cout << "record codec: " << codec.id() << '\n';
        std::cout << "output: " << output_path << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "AI creator error: " << error.what() << '\n';
        return 1;
    }
}
