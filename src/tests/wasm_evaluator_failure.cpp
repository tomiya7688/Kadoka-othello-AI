#include <cstddef>
#include <fstream>
#include <string>

// A real WASI command that emits one chosen failure through the batch protocol.
int main(int argc, char** argv) {
    std::string input_path;
    std::string output_path;
    for (int i = 1; i + 1 < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--kadoka-eval-input")
            input_path = argv[++i];
        else if (arg == "--kadoka-eval-output")
            output_path = argv[++i];
    }
    std::ifstream input(input_path);
    if (!input || output_path.empty())
        return 2;
    std::size_t id = 0;
    int mode = 0;
    std::string token;
    while (input >> token) {
        if (token == "case")
            input >> id;
        else if (token == "failure_mode")
            input >> mode;
    }
    if (mode == 1)
        return 17;
    if (mode == 2)
        return 0; // Success exit, but no output file.
    std::ofstream output(output_path);
    if (!output)
        return 3;
    if (mode == 3) {
        output << "not_a_result\n";
    } else {
        output << "result " << (mode == 4 ? id + 1 : id) << '\n';
        output << "value score " << (mode == 5 ? "nan" : "1") << '\n';
        if (mode != 6)
            output << "end\n";
        if (mode == 7)
            output << "result " << id << "\nvalue score 1\nend\n";
    }
    return output ? 0 : 4;
}
