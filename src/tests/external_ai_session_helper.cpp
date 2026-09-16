#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

enum class Mode {
    Normal,
    Illegal,
    Malformed,
    Exit,
    Timeout,
};

Mode parse_mode(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--mode=illegal") return Mode::Illegal;
        if (arg == "--mode=malformed") return Mode::Malformed;
        if (arg == "--mode=exit") return Mode::Exit;
        if (arg == "--mode=timeout") return Mode::Timeout;
    }
    return Mode::Normal;
}

}  // namespace

int main(int argc, char** argv) {
    const Mode mode = parse_mode(argc, argv);
    std::size_t request_count = 0;
    std::string line;

    while (std::getline(std::cin, line)) {
        if (line == "quit") return 0;
        if (line.rfind("request ", 0) != 0) continue;

        std::size_t request_id = 0;
        {
            std::istringstream parser(line);
            std::string kind;
            parser >> kind >> request_id;
        }

        std::vector<std::pair<std::size_t, std::size_t>> legal_moves;
        while (std::getline(std::cin, line) && line != "end") {
            if (line.rfind("legal ", 0) == 0) {
                std::istringstream parser(line);
                std::string kind;
                std::size_t row = 0;
                std::size_t col = 0;
                parser >> kind >> row >> col;
                legal_moves.emplace_back(row, col);
            }
        }

        ++request_count;
        if (mode == Mode::Exit) return 17;
        if (mode == Mode::Timeout) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }

        std::cout << "result " << request_id << '\n';
        if (mode == Mode::Malformed) {
            std::cout << "this_is_not_valid\nend\n" << std::flush;
            continue;
        }

        if (mode == Mode::Illegal || legal_moves.empty()) {
            std::cout << "move 0 0\n";
        } else {
            std::cout << "move " << legal_moves.front().first << ' ' << legal_moves.front().second << '\n';
        }
        std::cout << "diag request_count=" << request_count << '\n';
        std::cout << "end\n" << std::flush;
    }

    return 0;
}
