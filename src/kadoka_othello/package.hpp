#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace kadoka::othello {

enum class AIPackageInterface {
    Native,
    DynamicLibrary,
    ExternalProcess,
    Python,
    Network,
};

struct AIPackageManifest {
    std::string id;
    std::string name;
    std::string version;
    std::size_t protocol_version{1};
    std::vector<std::size_t> board_sizes;
    AIPackageInterface interface_type{AIPackageInterface::Native};
    std::string entry;
    std::string model;
    std::string metadata;
    std::vector<std::string> capabilities;

    // External/script runtime settings. Persistent stdin/stdout sessions are
    // the default; legacy_oneshot keeps the old temp-file compatibility path.
    std::string transport{"persistent"};
    std::string executable;
    std::size_t timeout_ms{5000};

    std::string source_directory;
};

[[nodiscard]] AIPackageManifest load_ai_manifest(const std::string& path);
[[nodiscard]] std::string package_interface_name(AIPackageInterface type);

}  // namespace kadoka::othello
