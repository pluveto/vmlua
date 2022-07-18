#pragma once
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>

#include "emitter.h"
#include "lexer.h"
#include "parser.h"
#include "vm.h"
namespace lb::vmlua {
class driver {
private:
    std::ifstream _file;

public:
    driver(std::string const& path) { _file.open(path); }
    ~driver() {
        // no need to close for RAII, but symmetrical aesthetics
        _file.close();
    }
    void run() {
        auto debug_flag = std::getenv("VM_LUA_DEBUG");
        bool debug{false};

        if (debug_flag != NULL && debug_flag[0] == '1') {
            debug = true;
        }

        lexer lexer(_file);
        std::vector<token_t> tokens;
        for (auto&& token : lexer) {
            if (token.get()->has_value()) {
                tokens.push_back(std::get<0>(token.get()->value()));
            }
        }
        static const char *blue = "\033[34m", *red = "\033[31m", *green = "\033[32m", *reset = "\033[0m";

        std::cout << blue << "[driver] finish lexing: " << reset << std::endl;
        std::cout << "[driver] tokens:" << vmlua::to_string(tokens) << std::endl;
        parser parser(tokens);
        auto ast = parser.parse();
        emitter emitter;
        auto prog = emitter.compile(ast);
        std::cout << green << "[driver] finish compile" << reset << std::endl;
        vm vm;
        vm.show_asm(prog);
        std::cout << blue << "[driver] running" << reset << std::endl;
        vm.set_debug(debug);
        vm.eval(prog);
        std::cout << green << "[driver] done!" << reset << std::endl;
    }
};

}  // namespace lb::vmlua
