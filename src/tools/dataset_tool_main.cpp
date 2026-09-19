#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "kadoka_othello/dataset_pool.hpp"

using namespace kadoka::othello;

namespace {

std::string read_all(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("failed to open file: " + path);
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

DatasetRegistry load_registry(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("failed to open Dataset Registry: " + path);
    }
    return read_dataset_registry_jsonl(input);
}

DatasetRecipe load_recipe(const std::string& path) {
    return parse_dataset_recipe_json(read_all(path));
}

void print_usage() {
    std::cout
        << "Kadoka Dataset Tool\n"
        << "  validate <registry.jsonl>\n"
        << "  plan <registry.jsonl> <recipe.json>\n"
        << "  attach-recipe <metadata.json> <recipe.json> <output.json>\n";
}

int command_validate(int argc, char** argv) {
    if (argc != 3) {
        print_usage();
        return 2;
    }
    const DatasetRegistry registry = load_registry(argv[2]);
    std::cout << "Dataset Registry: OK entries="
              << registry.entries().size() << '\n';
    return 0;
}

int command_plan(int argc, char** argv) {
    if (argc != 4) {
        print_usage();
        return 2;
    }
    const DatasetRegistry registry = load_registry(argv[2]);
    const DatasetRecipe recipe = load_recipe(argv[3]);
    const ResolvedDatasetRecipe resolved = registry.resolve(recipe);

    std::cout << "recipe=" << resolved.recipe_id
              << " model=" << resolved.model_id
              << " usage=" << dataset_usage_name(resolved.usage)
              << " unique_games=" << resolved.unique_game_count << '\n';
    for (const auto& slice : resolved.datasets) {
        std::cout << "dataset=" << slice.dataset_id
                  << " ratio=" << std::setprecision(17) << slice.ratio
                  << " games=" << slice.game_ids.size() << '\n';
    }
    return 0;
}

int command_attach_recipe(int argc, char** argv) {
    if (argc != 5) {
        print_usage();
        return 2;
    }
    const std::string metadata = read_all(argv[2]);
    const DatasetRecipe recipe = load_recipe(argv[3]);
    const std::string updated =
        metadata_with_training_recipe(metadata, recipe);

    std::ofstream output(argv[4], std::ios::out | std::ios::trunc);
    if (!output) {
        throw std::runtime_error(
            "failed to create model metadata output: " +
            std::string(argv[4]));
    }
    output << updated;
    std::cout << "attached recipe=" << recipe.recipe_id
              << " model=" << recipe.model_id
              << " output=" << argv[4] << '\n';
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
        if (command == "validate") return command_validate(argc, argv);
        if (command == "plan") return command_plan(argc, argv);
        if (command == "attach-recipe") {
            return command_attach_recipe(argc, argv);
        }

        print_usage();
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "Dataset Tool error: " << error.what() << '\n';
        return 1;
    }
}
