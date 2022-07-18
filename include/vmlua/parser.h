#pragma once
#include <fstream>
#include <iostream>
#include <memory>

namespace lb::vmlua {

class parser {
public:
    template <typename T>
    using ast_yield = std::optional<std::pair<std::unique_ptr<T>, size_t>>;

private:
    std::vector<token_t> _tokens;
    std::string levels{""};
    std::vector<std::function<ast_yield<stmt_t>(size_t)>> _stmt_parsers;

public:
    explicit parser(std::vector<token_t> tokens) noexcept : _tokens(tokens) {}
    vmlua::ast parse() {
        load_parsers();
        vmlua::ast ast;
        size_t it = 0;
        while (it < _tokens.size()) {
            auto stmt_yield = parse_statement(it);
            if (!stmt_yield.has_value()) {
                throw std::runtime_error("parse error, end too early, check your source code");
            }
            auto stmt = std::move(stmt_yield.value().first);
            auto syntax = vmlua::to_string(stmt.get());
            // std::replace(syntax.begin(), syntax.end(), '(', '[');
            // std::replace(syntax.begin(), syntax.end(), ')', ']');
            std::cout << "[parser][debug] syntax tree: " << syntax << "" << std::endl;
            it = stmt_yield.value().second;
            ast.push_back(std::move(stmt));
        }
        return ast;
    }
    void enter() { levels += "    "; }
    void leave() { levels = levels.substr(0, levels.size() - 4); }
    ast_yield<stmt_t> parse_statement(size_t it) {
        enter();
        scope_guard guard([this]() { leave(); });

        std::cout << "[debug]" << levels << "parse_statement(" << it << ")" << std::endl;
        for (auto &&parser : _stmt_parsers) {
            auto res = parser(it);
            if (res.has_value()) {
                return res;
            }
        }
        return std::nullopt;
    }
    bool expect_keyword(size_t it, const std::string &keyword) {
        auto t = _tokens[it];
        if (t.kind != token_kind::t_keyword) {
            return false;
        }
        if (t.literal != keyword) {
            return false;
        }
        return true;
    }
    bool expect_syntax(size_t it, const std::string &syntax) {
        auto t = _tokens[it];

        if (t.kind != token_kind::t_syntax) {
            return false;
        }
        if (t.literal != syntax) {
            return false;
        }
        return true;
    }
    bool expect_identifier(size_t it) {
        auto t = _tokens[it];

        if (t.kind != token_kind::t_identifier) {
            return false;
        }
        return true;
    }
    ast_yield<stmt_t> parse_function(size_t it) {
        enter();
        scope_guard guard([this]() { leave(); });
        std::cout << "[debug]" << levels << "parse_function" << std::endl;
        if (!expect_keyword(it, "function")) {
            return std::nullopt;
        }
        auto next_it = it + 1;
        if (!expect_identifier(next_it)) {
            std::cerr << "expected identifier after " << _tokens[it].to_string() << std::endl;
            return std::nullopt;
        }
        auto name = _tokens[next_it];
        next_it++;

        if (!expect_syntax(next_it, "(")) {
            std::cerr << "expected ( after " << name.to_string() << std::endl;
            return std::nullopt;
        }
        next_it++;  // (
        std::vector<std::unique_ptr<token_t>> params;
        while (!expect_syntax(next_it, ")")) {
            if (!params.empty()) {
                if (!expect_syntax(next_it, ",")) {
                    std::cerr << "expected , after " << (*params.back()).to_string() << std::endl;
                    return std::nullopt;
                }
                next_it++;
            }
            params.push_back(std::move(std::make_unique<token_t>(_tokens[next_it])));
            next_it++;
        }

        next_it++;  // )
        std::cout << "[debug]" << levels << "--- parse function statements" << std::endl;

        std::vector<std::unique_ptr<stmt_t>> stmts;
        while (!expect_keyword(next_it, "end")) {
            auto stmt = parse_statement(next_it);
            if (!stmt.has_value()) {
                std::cerr << "[debug]" << levels << "--- expected statement after " << _tokens[next_it].to_string()
                          << std::endl;
                return std::nullopt;
            }
            stmts.push_back(std::move(stmt.value().first));
            next_it = stmt.value().second;
        }
        std::cout << "[debug]" << levels << "parse function end" << std::endl;

        next_it++;  // end
        return std::make_pair(std::move(std::make_unique<func_decl>(name, params, stmts)), next_it);
    }
    ast_yield<stmt_t> parse_if(size_t it) {
        enter();
        scope_guard guard([this]() { leave(); });
        std::cout << "[debug]" << levels << "parse_if" << std::endl;
        if (!expect_keyword(it, "if")) {
            return std::nullopt;
        }
        auto next_it = it + 1;

        // cond expr
        std::cout << "[debug]" << levels << "parse_if - finding cond expr" << std::endl;

        auto cond = parse_expression(next_it);
        if (!cond.has_value()) {
            std::cerr << "[error]" << levels << "--- if: expected expression after " << _tokens[next_it].literal
                      << std::endl;
            return std::nullopt;
        }
        next_it = cond.value().second;
        std::cout << "[debug]" << levels << "parse_if - finding then" << std::endl;

        // then
        if (!expect_keyword(next_it, "then")) {
            std::cerr << "[error]" << levels << "--- if: expected then after " << _tokens[next_it].literal
                      << std::endl;
            return std::nullopt;
        }
        next_it++;
        std::cout << "[debug]" << levels << "parse_if - finding statements" << std::endl;

        // stmts
        std::vector<std::unique_ptr<stmt_t>> stmts;
        while (!expect_keyword(next_it, "end") && !expect_keyword(next_it, "else")) {
            auto stmt = parse_statement(next_it);
            if (!stmt.has_value()) {
                std::cerr << "[error]" << levels << "--- if: stmt expected statement after " << _tokens[next_it].literal
                          << std::endl;
                return std::nullopt;
            }
            stmts.push_back(std::move(stmt.value().first));
            next_it = stmt.value().second;
        }
        std::vector<std::unique_ptr<stmt_t>> else_stmts;
        {
            std::cout << "[debug]" << levels << "parse_if - finding else" << std::endl;
            if (expect_keyword(next_it, "else")) {
                next_it++;
                while (!expect_keyword(next_it, "end")) {
                    auto stmt = parse_statement(next_it);
                    if (!stmt.has_value()) {
                        std::cerr << "[error]" << levels << "--- if: else_stmts expected statement after "
                                  << _tokens[next_it].to_string() << std::endl;
                        return std::nullopt;
                    }
                    else_stmts.push_back(std::move(stmt.value().first));
                    next_it = stmt.value().second;
                }
            }
        }
        // skip end
        next_it++;
        std::cout << "[debug]" << levels << "!! success parse_if" << std::endl;
        // todo
        return std::make_pair(std::move(std::make_unique<if_stmt>(cond.value().first, stmts, else_stmts)), next_it);
    }
    ast_yield<stmt_t> parse_expression_statement(size_t it) {
        enter();
        scope_guard guard([this]() { leave(); });
        std::cout << "[debug]" << levels << "call parse_expression_statement" << std::endl;
        auto next_it = it;
        auto res = parse_expression(next_it);
        if (!res.has_value()) {
            std::cerr << "[debug]" << levels << "parse_expression_statement expected expression after "
                      << _tokens[next_it].to_string() << std::endl;
            return std::nullopt;
        }
        std::unique_ptr<expr_t> res_expr = std::move(res.value().first);
        next_it = res.value().second;
        if (!expect_syntax(next_it, ";")) {
            std::cerr << "[error]" << levels << "expect ';' but got " << _tokens[next_it].literal << std::endl;
            return std::nullopt;
        }
        next_it++;
        std::cout << "[debug]" << levels << "!! success parse_expression_statement" << std::endl;
        return std::make_pair(std::move(std::make_unique<expr_stmt>(expr_stmt{res_expr})), next_it);
    }
    ast_yield<expr_t> parse_expression(size_t it) {
        enter();
        scope_guard guard([this]() { leave(); });
        std::cout << "[debug]" << levels << "call parse_expression" << std::endl;
        if (it >= _tokens.size()) {
            return std::nullopt;
        }
        auto left_tok = _tokens[it];
        std::unique_ptr<expr_t> left;
        {
            switch (left_tok.kind) {
                case token_kind::t_number:
                    left = std::make_unique<literal_number>(left_tok);
                    break;
                case token_kind::t_identifier:
                    left = std::make_unique<literal_id>(left_tok);
                    break;
                default:
                    break;
            }
        }
        auto next_it = it + 1;
        std::cout << "[debug]" << levels << "expr - try func call" << std::endl;

        // func call
        if (expect_syntax(next_it, "(")) {
            next_it++;
            std::vector<std::unique_ptr<expr_t>> args;
            while (!expect_syntax(next_it, ")")) {
                // if (!args.empty()) {
                //     if (!expect_syntax(next_it, ",")) {
                //         std::cerr << "[error]"<<levels<< "expect ',' but got " << _tokens[next_it].literal <<
                //         std::endl; return std::nullopt;
                //     }
                //     next_it++;
                // }
                auto res = parse_expression(next_it);
                if (!res.has_value()) {
                    std::cerr << "[error]" << levels << "-- func call expect expression but got "
                              << _tokens[next_it].literal << std::endl;
                    return std::nullopt;
                }
                std::unique_ptr<vmlua::expr_t> arg = std::move(res.value().first);
                next_it = res.value().second;

                std::cout << "[debug]" << levels << "parse arg"
                          << "syntax tree: " << vmlua::to_string(arg.get()) << std::endl;
                args.push_back(std::move(arg));
                if (expect_syntax(next_it, ",")) {
                    next_it++;
                }
            }
            next_it++;  // )
            auto ret = std::make_unique<func_call>(left_tok, args);
            std::cout << "[debug]" << levels << "func_call"
                      << "syntax tree: " << vmlua::to_string(*ret) << std::endl;

            return std::make_pair(std::move(ret), next_it);
        }
        std::cout << "[debug]" << levels << "expr - try liter expr" << std::endl;

        // liter expr
        if (next_it >= _tokens.size() || _tokens[next_it].kind != token_kind::t_operator) {
            return std::make_pair(std::move(left), next_it);
        }
        std::cout << "[debug]" << levels << "expr - try binary expr" << std::endl;

        // binary expr
        auto op = _tokens[next_it];
        if (op.kind != token_kind::t_operator) {
            std::cerr << "[error]" << levels << "binary expr - expected operator but got " << op.to_string()
                      << std::endl;
            return std::nullopt;
        }
        std::cout << "[debug]" << levels << "expr - operator is " << op.literal << std::endl;
        next_it++;

        if (next_it >= _tokens.size()) {
            std::cerr << "[error]" << levels << "binary expr - unexpected end of tokens" << std::endl;
            return std::nullopt;
        }
        auto right_tok = _tokens[next_it];
        std::unique_ptr<expr_t> right;
        {
            switch (right_tok.kind) {
                case token_kind::t_number:
                    right = std::make_unique<literal_number>(right_tok);
                    break;
                case token_kind::t_identifier:
                    right = std::make_unique<literal_id>(right_tok);
                    break;
                default:
                    std::cerr << "[error]" << levels << "binary expr - expect literal but got "
                              << _tokens[next_it].literal << std::endl;
                    return std::nullopt;
            }
        }
        next_it++;  // operand right
        std::cout << "[debug]" << levels << "!! success parse_expression" << std::endl;
        return std::make_pair(std::move(std::make_unique<binary_op>(op, left, right)), next_it);
    }
    ast_yield<stmt_t> parse_return(size_t it) {
        enter();
        scope_guard guard([this]() { leave(); });
        std::cout << "[debug]" << levels << "call parse_return" << std::endl;
        if (!expect_keyword(it, "return")) {
            return std::nullopt;
        }
        auto next_it = it + 1;
        auto res = parse_expression(next_it);
        if (!res.has_value()) {
            std::cerr << "[error]" << levels << "-- parse_return expect expression after return" << std::endl;
            return std::nullopt;
        }
        std::unique_ptr<vmlua::expr_t> expr = std::move(res.value().first);
        next_it = res.value().second;
        if (!expect_syntax(next_it, ";")) {
            std::cerr << "[error]" << levels << "-- parse_return expect ';' but got " << _tokens[next_it].literal
                      << std::endl;
            return std::nullopt;
        }
        next_it++;
        std::cout << "[debug]" << levels << "!! success parse_return" << std::endl;

        return std::make_pair(std::move(std::make_unique<ret_stmt>(expr)), next_it);
    }
    ast_yield<stmt_t> parse_local(size_t it) {
        enter();
        scope_guard guard([this]() { leave(); });
        std::cout << "[debug]" << levels << "call parse_local" << std::endl;
        if (!expect_keyword(it, "local")) {
            return std::nullopt;
        }
        auto next_it = it + 1;
        // get id
        if (!expect_identifier(next_it)) {
            std::cerr << "[error]" << levels << "parse_local -- expect identifier after" << _tokens[it].literal
                      << std::endl;
            return std::nullopt;
        }
        auto id = _tokens[next_it];
        next_it++;

        // get assign
        if (!expect_syntax(next_it, "=")) {
            std::cerr << "[error]" << levels << "parse_local -- expect '=' after" << _tokens[next_it - 1].literal
                      << std::endl;
            return std::nullopt;
        }
        next_it++;

        // get expr
        auto res = parse_expression(next_it);
        if (!res.has_value()) {
            std::cerr << "[error]" << levels << "parse_local -- expect expression after" << _tokens[next_it - 1].literal
                      << std::endl;
            return std::nullopt;
        }
        auto expr = std::move(res.value().first);
        next_it = res.value().second;

        if (!expect_syntax(next_it, ";")) {
            std::cerr << "[error]" << levels << "parse_local -- expect ';' after" << _tokens[next_it - 1].literal
                      << std::endl;
            return std::nullopt;
        }
        next_it++;
        std::cout << "[debug]" << levels << "!! success parse_local" << std::endl;
        return std::make_pair(std::move(std::make_unique<local_stmt>(id, expr)), next_it);
    }
    void load_parsers() {
        if (!_stmt_parsers.empty()) {
            return;
        }
        _stmt_parsers.push_back([this](size_t it) { return parse_if(it); });
        _stmt_parsers.push_back([this](size_t it) { return parse_return(it); });
        _stmt_parsers.push_back([this](size_t it) { return parse_expression_statement(it); });
        _stmt_parsers.push_back([this](size_t it) { return parse_function(it); });
        _stmt_parsers.push_back([this](size_t it) { return parse_local(it); });
        return;
    }
};

}  // namespace lb::vmlua
