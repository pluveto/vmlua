#pragma once
#include <iomanip>
#include <map>

#include "types.h"

namespace lb::vmlua {
struct instruction {
    virtual ~instruction() {}
};

struct dup_plus_fp_inst : public instruction {
    int32_t offset;

    dup_plus_fp_inst(int32_t offset) : offset(offset) {}
};
struct move_minus_fp_inst : public instruction {
    size_t local_off;
    int32_t fp_off;

    move_minus_fp_inst(size_t local_off, int32_t fp_off) : local_off(local_off), fp_off(fp_off) {}
};
struct move_plus_fp_inst : public instruction {
    size_t value;

    move_plus_fp_inst(size_t value) : value(value) {}
};
struct store_inst : public instruction {
    int32_t n;

    store_inst(int32_t offset) : n(offset) {}
};
struct return_inst : public instruction {
    bool has_value;
    return_inst(bool has_value = true) : has_value(has_value) {}
};
struct jump_if_not_zero_inst : public instruction {
    std::string label;

    jump_if_not_zero_inst(std::string label) : label(label) {}
};
struct jump_if_zero_inst : public instruction {
    std::string label;

    jump_if_zero_inst(std::string label) : label(label) {}
};
struct jump_inst : public instruction {
    std::string label;

    jump_inst(std::string label) : label(label) {}
};
struct call_inst : public instruction {
    std::string label;
    size_t argc;

    call_inst(std::string label, size_t argc) : label(label), argc(argc) {}
};
struct add_inst : public instruction {
    add_inst() {}
};
struct subtract_inst : public instruction {
    subtract_inst() {}
};
enum logical_op { AND, OR, LT, GT, LE, GE, EQ, NE };
struct logic_cond_inst : public instruction {
    logical_op op;
    logic_cond_inst(logical_op op) : op(op) {}
};

struct symbol {
    int32_t loc;
    size_t nargs;
    size_t nlocals;
};

struct program {
    std::map<std::string, symbol> syms;
    std::vector<std::unique_ptr<instruction>> insts;
};

class vm {
private:
    int32_t pc{0};
    int32_t fp{0};
    std::vector<int32_t> stack;
    bool debug{false};

public:
    void eval(program& prog) {
        while (pc < prog.insts.size()) {
            if (debug) {
                std::cout << "pc = " << pc << '\n';
                std::cout << "stack: " << '\n';
                show_stack();
                std::cout << "program: " << '\n';
                show_asm(prog);
                std::cout << "> " << std::flush;
                std::string line;
                while (std::getline(std::cin, line)) {
                    if (line == "quit") {
                        return;
                    }
                    if (line == "debug off") {
                        debug = false;
                        break;
                    }
                    if (lb::string_util::start_with(line, "mem")) {
                        auto args = lb::string_util::split(line, ' ', true, 3);
                        if (args.size() == 2) {
                            if (!lb::string_util::is_number(args[1])) {
                                std::cout << "invalid argument: " << args[1] << std::endl;
                                std::cout << "> " << std::flush;
                                continue;
                            }
                            auto addr = std::stoi(args[1]);
                            if (addr >= 0 && addr < stack.size()) {
                                std::cout << "mem[" << addr << "] = " << stack.at(addr) << '\n';
                            } else {
                                std::cout << "mem[" << addr << "] = out of range\n";
                            }
                        } else if (args.size() == 3) {
                            // you have no choice
                            auto reg = args[1] == "pc" ? pc : pc;
                            if (!lb::string_util::is_number(args[2])) {
                                std::cout << "invalid argument: " << args[2] << std::endl;
                                std::cout << "> " << std::flush;
                                continue;
                            }
                            auto off = std::stoi(args[2]);
                            auto addr = reg + off;
                            if (addr >= 0 && addr < stack.size()) {
                                std::cout << "mem[" << addr << "] = " << stack.at(addr) << '\n';
                            } else {
                                std::cout << "mem[" << addr << "] = out of range\n";
                            }
                        } else {
                            std::cout << "invalid arguments\n";
                        }
                        std::cout << "> " << std::flush;
                        continue;
                    }
                    if (line == "step" || line == "") {
                        break;
                    }
                    std::cout << "unknown command: " << line << '\n';
                    std::cout << "> " << std::flush;
                }
            }
            auto inst = prog.insts[pc].get();
            if (auto* p = dynamic_cast<add_inst*>(inst)) {
                auto right = pop_stack();
                auto left = pop_stack();
                push_stack(left + right);
                pc++;
            } else if (auto* p = dynamic_cast<subtract_inst*>(inst)) {
                auto right = pop_stack();
                auto left = pop_stack();
                push_stack(left - right);
                pc++;
            } else if (auto* p = dynamic_cast<logic_cond_inst*>(inst)) {
                auto right = pop_stack();
                auto left = pop_stack();
                int result = 0;
                switch (p->op) {
                    case AND:
                        result = left & right;
                        break;
                    case OR:
                        result = left | right;
                        break;
                    case LT:
                        result = left < right;
                        break;
                    case GT:
                        result = left > right;
                        break;
                    case LE:
                        result = left <= right;
                        break;
                    case GE:
                        result = left >= right;
                        break;
                    case EQ:
                        result = left == right;
                        break;
                    case NE:
                        result = left != right;
                        break;
                }
                push_stack(result);
                pc++;
            } else if (auto* p = dynamic_cast<dup_plus_fp_inst*>(inst)) {
                push_stack(stack.at(fp + p->offset));
                pc++;
            } else if (auto* p = dynamic_cast<move_minus_fp_inst*>(inst)) {
                stack.at(fp + p->local_off) = stack.at((fp - (p->fp_off + 4)));
                pc++;
            } else if (auto* p = dynamic_cast<move_plus_fp_inst*>(inst)) {
                auto val = pop_stack();
                auto index = static_cast<size_t>(fp) + p->value;
                while (index >= stack.size()) {
                    stack.push_back(0);
                }
                stack.at(index) = val;
                pc++;
            } else if (auto* p = dynamic_cast<store_inst*>(inst)) {
                push_stack(p->n);
                pc++;
            } else if (auto* p = dynamic_cast<return_inst*>(inst)) {
                if (!p->has_value) {
                    auto nargs = pop_stack();
                    pc = pop_stack();
                    fp = pop_stack();
                    continue;
                }
                auto ret = pop_stack();
                while (fp < stack.size()) {
                    pop_stack();
                }
                auto nargs = pop_stack();
                pc = pop_stack();
                fp = pop_stack();
                while (nargs--) {
                    pop_stack();
                }
                push_stack(ret);
            } else if (auto* p = dynamic_cast<jump_if_not_zero_inst*>(inst)) {
                auto value = pop_stack();
                if (value != 0) {
                    pc = prog.syms[p->label].loc;
                    continue;
                }
                pc += 1;
            } else if (auto* p = dynamic_cast<jump_if_zero_inst*>(inst)) {
                auto value = pop_stack();
                if (value == 0) {
                    pc = prog.syms[p->label].loc;
                    continue;
                }
                pc += 1;
            } else if (auto* p = dynamic_cast<jump_inst*>(inst)) {
                pc = prog.syms[p->label].loc;
            } else if (auto* p = dynamic_cast<call_inst*>(inst)) {
                if (p->label == "print") {
                    for (int i = 0; i < p->argc; i++) {
                        std::cout << pop_stack() << " ";
                    }
                    std::cout << std::endl;
                    pc++;
                    continue;
                }
                push_stack(fp);
                push_stack(pc + 1);
                push_stack(prog.syms[p->label].nargs);
                pc = prog.syms[p->label].loc;
                fp = stack.size();

                auto nlocals = prog.syms[p->label].nlocals;
                while (nlocals--) {
                    stack.push_back(0);
                }
            } else {
                throw std::runtime_error("unknown instruction");
            }
        }
    }
    void set_debug(bool debug) { this->debug = debug; }
    void show_asm(program& prog) {
        auto vpc = 0;
        std::cout << std::setw(8) << "--------"
                  << "+------------------------------" << std::endl;
        std::cout << std::setw(8) << " OFFSET "
                  << "| INSTRUCTION" << std::endl;
        std::cout << std::setw(8) << "--------"
                  << "+------------------------------" << std::endl;

        while (vpc < prog.insts.size()) {
            if (debug) {
                std::cout << " "                      //
                          << (vpc == pc ? "*" : " ")  //
                          << std::setw(5) << vpc << "| ";
            } else {
                std::cout << std::setw(8) << vpc << "| ";
            }
            // print labels
            for (auto& sym : prog.syms) {
                if (vpc == sym.second.loc) {
                    std::cout << sym.first << ": " << std::endl
                              << std::setw(8) << " "
                              << "| ";
                }
            }
            std::cout << std::setw(4) << " ";
            auto inst = prog.insts[vpc].get();
            if (auto* p = dynamic_cast<add_inst*>(inst)) {
                std::cout << "ADD" << std::endl;
            } else if (auto* p = dynamic_cast<subtract_inst*>(inst)) {
                std::cout << "SUB" << std::endl;
            } else if (auto* p = dynamic_cast<logic_cond_inst*>(inst)) {
                std::string cond;
                switch (p->op) {
                    case AND:
                        cond = "AND";
                        break;
                    case OR:
                        cond = "OR";
                        break;
                    case LT:
                        cond = "LT";
                        break;
                    case GT:
                        cond = "GT";
                        break;
                    case LE:
                        cond = "LE";
                        break;
                    case GE:
                        cond = "GE";
                        break;
                    case EQ:
                        cond = "EQ";
                        break;
                    case NE:
                        cond = "NE";
                        break;
                }
                std::cout << "COND " << cond << std::endl;
            } else if (auto* p = dynamic_cast<dup_plus_fp_inst*>(inst)) {
                std::cout << "PUSH FP + " << p->offset << std::endl;
            } else if (auto* p = dynamic_cast<move_minus_fp_inst*>(inst)) {
                std::cout << "ST FP - " << (p->fp_off + 4) << " -> "
                          << "FP + " << p->local_off << std::endl;
            } else if (auto* p = dynamic_cast<move_plus_fp_inst*>(inst)) {
                auto index = static_cast<size_t>(fp) + p->value;
                std::cout << "POP FP + " << p->value << "" << std::endl;
            } else if (auto* p = dynamic_cast<store_inst*>(inst)) {
                std::cout << "PUSH " << p->n << std::endl;
            } else if (auto* p = dynamic_cast<return_inst*>(inst)) {
                if (p->has_value) {
                    std::cout << "RETVAL" << std::endl;
                } else {
                    std::cout << "RET" << std::endl;
                }
                std::cout << std::setw(8) << vpc << "| " << std::endl;
            } else if (auto* p = dynamic_cast<jump_if_not_zero_inst*>(inst)) {
                std::cout << "JNZ " << p->label << " (offset=" << prog.syms[p->label].loc << ")" << std::endl;
            } else if (auto* p = dynamic_cast<jump_if_zero_inst*>(inst)) {
                std::cout << "JZ " << p->label << " (offset=" << prog.syms[p->label].loc << ")" << std::endl;
            } else if (auto* p = dynamic_cast<jump_inst*>(inst)) {
                std::cout << "JMP " << p->label << " (offset=" << prog.syms[p->label].loc << ")" << std::endl;
            } else if (auto* p = dynamic_cast<call_inst*>(inst)) {
                if (p->label == "print") {
                    std::cout << "CALL print@internal, ARGC=" << p->argc << std::endl;
                } else {
                    std::cout << "CALL " << p->label << "(" << prog.syms[p->label].loc
                              << "), nargs=" << prog.syms[p->label].nargs << ", nlocals=" << prog.syms[p->label].nlocals
                              << std::endl;
                }
            } else {
                throw std::runtime_error("unknown instruction");
            }
            vpc++;
        }
    }
    void show_stack() {
        auto size = stack.size();
        if (size == 0) {
            std::cout << "(empty)" << '\n';
            return;
        }
        std::stringstream stream;
        stream << std::setfill(' ') << std::setw(sizeof(int32_t)) << "addr"
               << "  ";
        stream << "  " << std::setfill(' ') << std::setw(sizeof(int32_t) * 2) << "hex";
        stream << "  " << std::setfill(' ') << std::setw(sizeof(int32_t) * 2) << "dec" << '\n';
        for (auto i = 0; i < size; i++) {
            auto value = stack.at(i);
            stream << std::setfill('0') << std::setw(sizeof(int32_t)) << i << "  ";
            stream << "0x" << std::setfill('0') << std::setw(sizeof(int32_t) * 2) << std::hex << value;
            stream << "  " << std::setfill('0') << std::setw(sizeof(int32_t) * 2) << std::dec << value << '\n';
        }
        std::cout << stream.str();
    }

private:
    int32_t pop_stack() {
        auto v = stack.back();
        stack.pop_back();
        return v;
    }
    void push_stack(int32_t v) { stack.push_back(v); }
};

}  // namespace lb::vmlua
