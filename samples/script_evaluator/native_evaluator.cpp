#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>

int main(int argc, char** argv) {
    std::string input_path;
    std::string output_path;
    for (int i = 1; i + 1 < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--kadoka-eval-input") input_path = argv[++i];
        else if (arg == "--kadoka-eval-output") output_path = argv[++i];
    }
    if (input_path.empty() || output_path.empty()) return 2;

    std::ifstream input(input_path);
    std::ofstream output(output_path);
    if (!input || !output) return 3;

    std::size_t current_id = 0;
    bool active = false;
    std::unordered_map<std::string, double> features;
    std::string line;

    auto flush = [&]() {
        if (!active) return;
        const double memory_bonus = features["memory_bonus"];
        const double recall_delay = features["recall_delay"];
        const double score = memory_bonus - recall_delay;
        output << "result " << current_id << '\n';
        output << "value score " << score << '\n';
        output << "value confidence 1\n";
        output << "diag runtime=native_process\n";
        output << "end\n";
        features.clear();
        active = false;
    };

    while (std::getline(input, line)) {
        std::istringstream parser(line);
        std::string kind;
        parser >> kind;
        if (kind == "case") {
            flush();
            parser >> current_id;
            active = true;
        } else if (kind == "feature" && active) {
            std::string key;
            double value = 0.0;
            parser >> key >> value;
            features[key] = value;
        } else if (kind == "end") {
            flush();
        }
    }
    flush();
    return 0;
}
