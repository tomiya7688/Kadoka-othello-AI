#include "kadoka_othello/package_loader.hpp"

#include <stdexcept>

namespace kadoka::othello {
namespace {

std::unique_ptr<IAIAdapter> make_adapter(const std::string& id) {
    if (id == "pass_through") {
        return std::make_unique<PassThroughAdapter>();
    }
    if (id == "drop_legal_moves") {
        return std::make_unique<DropLegalMovesAdapter>();
    }
    throw std::invalid_argument("unknown AI adapter: " + id);
}

std::unique_ptr<IAIEngine> make_native_engine(
    const AIPackageManifest& manifest,
    std::uint64_t seed) {
    if (manifest.id == "kadoka.random") {
        return std::make_unique<RandomAI>(seed);
    }
    throw std::invalid_argument("unknown built-in native AI: " + manifest.id);
}

}  // namespace

LoadedAIPackage load_ai_package(
    const AIPackageManifest& manifest,
    std::uint64_t seed) {
    LoadedAIPackage loaded;
    loaded.manifest = manifest;
    loaded.adapter = make_adapter(manifest.adapter);

    switch (manifest.interface_type) {
        case AIPackageInterface::Native:
            loaded.engine = make_native_engine(manifest, seed);
            break;
        case AIPackageInterface::DynamicLibrary:
        case AIPackageInterface::ExternalProcess:
        case AIPackageInterface::Python:
        case AIPackageInterface::Network:
            throw std::runtime_error(
                "AI package interface is recognized but runtime loading is not implemented yet: " +
                package_interface_name(manifest.interface_type));
    }

    return loaded;
}

}  // namespace kadoka::othello
