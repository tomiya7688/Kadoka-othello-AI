#pragma once

#include <memory>

#include "kadoka_othello/ai.hpp"
#include "kadoka_othello/package.hpp"

namespace kadoka::othello {

struct LoadedAIPackage {
    AIPackageManifest manifest;
    std::unique_ptr<IAIEngine> engine;
    std::unique_ptr<IAIAdapter> adapter;

    [[nodiscard]] AIPackage view() const noexcept {
        return AIPackage{engine.get(), adapter.get()};
    }
};

[[nodiscard]] LoadedAIPackage load_ai_package(
    const AIPackageManifest& manifest,
    std::uint64_t seed = 0);

}  // namespace kadoka::othello
