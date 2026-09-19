#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

#include "kadoka_othello/core_state.hpp"
#include "kadoka_othello/rules.hpp"

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
    using namespace kadoka::othello;

    const Mode mode = parse_mode(argc, argv);
    std::size_t request_count = 0;
    std::string line;

    while (std::getline(std::cin, line)) {
        if (line.rfind("request ", 0) != 0) continue;

        std::size_t request_id = 0;
        {
            std::istringstream parser(line);
            std::string kind;
            parser >> kind >> request_id;
        }

        std::string state_json;
        while (std::getline(std::cin, line) && line != "end") {
            if (line.rfind("state ", 0) == 0) {
                state_json = line.substr(6);
            }
        }
        if (state_json.empty()) return 18;

        const CoreState state = parse_core_state_json(state_json);
        const Board board = board_from_core_state(state);
        const auto legal_moves = rules::legal_moves(board, state.side_to_move);

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
            std::cout << "move " << legal_moves.front().row << ' '
                      << legal_moves.front().col << '\n';
        }
        std::cout << "diag request_count=" << request_count << '\n';
        std::cout << "end\n" << std::flush;
    }

    return 0;
}
