#pragma once
#include <sstream>
#include <string>

#include "lb/util.h"

namespace lb::vmlua {

class scope_guard {
public:
    template <class Callable>
    scope_guard(Callable&& undo_func) try : f(std::forward<Callable>(undo_func)) {
    } catch (...) {
        undo_func();
        throw;
    }

    scope_guard(scope_guard&& other) : f(std::move(other.f)) { other.f = nullptr; }

    ~scope_guard() {
        if (f)
            f();  // must not throw
    }

    void dismiss() noexcept { f = nullptr; }

    scope_guard(const scope_guard&) = delete;
    void operator=(const scope_guard&) = delete;

private:
    std::function<void()> f;
};

enum token_kind { t_identifier = 1, t_syntax, t_keyword, t_number, t_operator, t_eof, t_unk };

inline std::string to_string(token_kind k) {
    switch (k) {
        case t_identifier:
            return "T_IDENTIFIER";
        case t_syntax:
            return "T_SYNTAX";
        case t_keyword:
            return "T_KEYWORD";
        case t_number:
            return "T_NUMBER";
        case t_operator:
            return "T_OPERATOR";
        case t_eof:
            return "T_EOF";
        default:
            return "T_UNKNOWN";
    }
}

struct location {
    int line;
    int column;
    size_t offset;

    explicit location(int line = 1, int column = 1, size_t offset = 0) noexcept
        : line(line), column(column), offset(offset) {}

    std::string to_string() const { return lb::string_util::concat(":", line, ":", column, " (", offset, " )"); }

    location step(bool is_newline) noexcept {
        if (is_newline) {
            return location(line + 1, 1, offset + 1);
        }
        return location(line, column + 1, offset + 1);
    }

    std::string debug(std::string const& msg) const {
        std::stringstream ss;
        ss << msg << to_string();
        return ss.str();
    }
};

struct token_t {
    token_kind kind;
    std::string literal;
    location loc;

    explicit token_t(token_kind kind = token_kind::t_unk, std::string literal = "",
                     location loc = location{0, 0, 0}) noexcept
        : kind(kind), literal(literal), loc(loc) {}

    std::string to_string() const {
        return lb::string_util::concat("token(kind: ", vmlua::to_string(kind), " literal: ", literal,
                                       " loc: ", loc.to_string() + " )");
    }
};

const static token_t tok_eof{token_kind::t_eof, "", location{0, 0, 0}};
const static token_t tok_unk{token_kind::t_unk, "", location{0, 0, 0}};

/**
 * AST
 */
struct stmt_t {
    virtual ~stmt_t() {}
};

typedef std::vector<std::unique_ptr<stmt_t>> ast;

struct expr_t {
    virtual ~expr_t(){};
    virtual std::unique_ptr<expr_t> clone() const = 0;
};

struct literal_t : public expr_t {
    virtual ~literal_t() {}
};

struct literal_id : public literal_t {
    token_t token{tok_unk};

    literal_id(token_t token) : token(token) {}

    std::unique_ptr<expr_t> clone() const override { return std::make_unique<literal_id>(token); }
};
struct literal_number : public literal_t {
    token_t token{tok_unk};

    literal_number(token_t token) : token(token) {}

    std::unique_ptr<expr_t> clone() const override { return std::make_unique<literal_number>(token); }
};

struct func_call : public expr_t {
    token_t name;
    std::vector<std::unique_ptr<expr_t>> arguments;

    func_call(token_t name, std::vector<std::unique_ptr<expr_t>>& arguments) : name(name) {
        for (auto&& arg : arguments) {
            this->arguments.push_back(std::move(arg));
        }
    }

    std::unique_ptr<expr_t> clone() const override {
        std::vector<std::unique_ptr<expr_t>> args;
        for (auto&& arg : arguments) {
            args.push_back(arg->clone());
        }
        return std::make_unique<func_call>(name, args);
    }
};

struct binary_op : public expr_t {
    token_t op;
    std::unique_ptr<expr_t> left;
    std::unique_ptr<expr_t> right;

    binary_op(token_t op, std::unique_ptr<expr_t>& left, std::unique_ptr<expr_t>& right)
        : op(op), left(std::move(left)), right(std::move(right)) {}

    std::unique_ptr<expr_t> clone() const override {
        auto newleft = left->clone();
        auto newright = right->clone();
        return std::make_unique<binary_op>(op, newleft, newright);
    }
};

struct func_decl : public stmt_t {
    token_t name;
    std::vector<std::unique_ptr<token_t>> params;
    std::vector<std::unique_ptr<stmt_t>> body;

    func_decl(token_t name, std::vector<std::unique_ptr<token_t>>& params, std::vector<std::unique_ptr<stmt_t>>& body)
        : name(name) {
        for (auto&& param : params) {
            this->params.push_back(std::move(param));
        }
        for (auto&& stmt : body) {
            this->body.push_back(std::move(stmt));
        }
    }
};

struct if_stmt : public stmt_t {
    std::unique_ptr<expr_t> condition;
    std::vector<std::unique_ptr<stmt_t>> then_body;
    std::vector<std::unique_ptr<stmt_t>> else_body;

    if_stmt(std::unique_ptr<expr_t>& condition, std::vector<std::unique_ptr<stmt_t>>& then_body,
            std::vector<std::unique_ptr<stmt_t>>& else_body)
        : condition(std::move(condition)) {
        for (auto&& stmt : then_body) {
            this->then_body.push_back(std::move(stmt));
        }
        for (auto&& stmt : else_body) {
            this->else_body.push_back(std::move(stmt));
        }
    }
};

struct local_stmt : public stmt_t {
    token_t name;
    std::unique_ptr<expr_t> expr;

    local_stmt(token_t name, std::unique_ptr<expr_t>& e) : name(name), expr(std::move(e)) {}
};

struct ret_stmt : public stmt_t {
    std::unique_ptr<expr_t> expr;

    ret_stmt(std::unique_ptr<expr_t>& e) : expr(std::move(e)) {}
};

struct expr_stmt : public stmt_t {
    std::unique_ptr<expr_t> expr;

    expr_stmt(std::unique_ptr<expr_t>& e) : expr(std::move(e)) {}
};

std::string to_string(expr_t* v);
std::string to_string(token_t* v);
std::string to_string(token_t& t);
std::string to_string(std::vector<token_t>& v);
std::string to_string(std::vector<std::unique_ptr<token_t>>& v);
std::string to_string(literal_t& v);
std::string to_string(literal_id& v);
std::string to_string(literal_number& v);
std::string to_string(func_call& v);
std::string to_string(binary_op& v);
std::string to_string(stmt_t* v);
std::string to_string(func_decl* v);
std::string to_string(if_stmt* v);
std::string to_string(local_stmt* v);
std::string to_string(ret_stmt* v);
std::string to_string(expr_stmt* v);

std::string to_string(token_t* t) { return t->literal; }
std::string to_string(token_t& t) { return t.literal; }
std::string to_string(std::vector<token_t>& v) {
    static const char *blue = "\033[34m", *red = "\033[31m", *green = "\033[32m", *reset = "\033[0m",
                      *gray = "\033[37m";
    std::stringstream ss;
    int index = 0;
    auto newline = [&]() { ss << std::endl << std::setw(4) << index++ << " | "; };
    for (auto&& t : v) {
        newline();
        if (t.kind == t_keyword) {
            ss << blue << t.literal << reset;
        } else if (t.kind == t_identifier) {
            ss << gray << t.literal << reset;
        } else if (t.kind == t_number) {
            ss << red << t.literal << reset;
        } else {
            ss << t.literal;
        }
        ss << " ";
    }
    return ss.str();
}

std::string to_string(std::vector<std::unique_ptr<token_t>>& v) {
    // std::cout << "[debug] call token_t>& v) " << std::endl;
    std::stringstream ss;
    for (int i = 0; i < v.size(); i++) {
        ss << to_string(*v[i]);
        if (i != v.size() - 1) {
            ss << " ";
        }
    }
    return ss.str();
}

std::string to_string(literal_id& v) {
    // std::cout << "[debug] call to_string(literal_id& v)" << std::endl;
    return "id " + to_string(v.token);
}

std::string to_string(literal_number& v) {
    // std::cout << "[debug] call to_string(literal_number& v)" << std::endl;
    return "number " + to_string(v.token);
}

std::string to_string(literal_t& v) {
    // std::cout << "[debug] call to_string(literal_t& v)" << std::endl;
    if (auto* p = dynamic_cast<literal_id*>(&v)) {
        return "id (" + to_string(p->token) + ")";
    } else if (auto* p = dynamic_cast<literal_number*>(&v)) {
        return "number (" + to_string(p->token) + ")";
    } else {
        return "unknown literal_t";
    }
}

std::string to_string(expr_t* v) {
    // std::cout << "[debug] call to_string(expr_t* v)" << std::endl;
    if (auto* p = dynamic_cast<literal_t*>(v)) {
        return to_string(*p);
    } else if (auto* p = dynamic_cast<func_call*>(v)) {
        return to_string(*p);
    } else if (auto* p = dynamic_cast<binary_op*>(v)) {
        return to_string(*p);
    } else {
        std::cout << "unknown expr_t typeid: " << typeid(v).name() << std::endl;
        return "unknown expr_t";
    }
}

std::string to_string(func_call& v) {
    // std::cout << "[debug] call to_string(func_call& v)" << std::endl;
    std::string args;
    for (auto& e : v.arguments) {
        args += to_string(e.get()) + " ";
    }
    return "func_call ( " + to_string(v.name) + " ( " + args + " ) )";
}

std::string to_string(binary_op& v) {
    // std::cout << "[debug] call to_string(binary_op& v)" << std::endl;
    return "binary_op ( " + to_string(v.op) + " ( " + to_string(v.left.get()) + " ) ( " + to_string(v.right.get()) +
           " ) )";
}

std::string to_string(func_decl* v) {
    // std::cout << "[debug] call to_string(func_decl* v)" << std::endl;
    std::string params;
    for (auto& e : v->params) {
        params += "(" + to_string(e.get()) + " )";
    }
    std::string body;
    for (auto& e : v->body) {
        body += "(" + to_string(e.get()) + " )";
    }
    return "func_decl ( " + to_string(v->name) + " ( " + params + " ) ( " + body + " ) )";
}

std::string to_string(stmt_t* v) {
    if (v == nullptr) {
        throw std::runtime_error("unreachable");
    }
    // std::cout << "[debug] call to_string(stmt_t* v)" << std::endl;
    if (auto* p = dynamic_cast<if_stmt*>(v)) {
        return to_string(p);
    } else if (auto* p = dynamic_cast<local_stmt*>(v)) {
        return to_string(p);
    } else if (auto* p = dynamic_cast<ret_stmt*>(v)) {
        return to_string(p);
    } else if (auto* p = dynamic_cast<expr_stmt*>(v)) {
        return to_string(p);
    } else if (auto* p = dynamic_cast<func_decl*>(v)) {
        return to_string(p);
    } else {
        return "unknown stmt_t";
    }
}

std::string to_string(if_stmt* v) {
    // std::cout << "[debug] call to_string(if_stmt* v)" << std::endl;
    std::string body;
    for (int i = 0; i < v->then_body.size(); i++) {
        body += "(" + to_string(v->then_body[i].get()) + ")";
        if (i != v->then_body.size() - 1) {
            body += " ";
        }
    }
    std::string else_body;
    for (int i = 0; i < v->else_body.size(); i++) {
        else_body += "(" + to_string(v->else_body[i].get()) + ")";
        if (i != v->else_body.size() - 1) {
            else_body += " ";
        }
    }
    return "if_stmt (cond ( " + to_string(v->condition.get()) + " ) (then ( " + body + " ) ) (else " + else_body +
           " ) )";
}

std::string to_string(local_stmt* v) {
    // std::cout << "[debug] call to_string(local_stmt* v)" << std::endl;
    return "local_stmt ( " + to_string(v->name) + " " + to_string(v->expr.get()) + " )";
}

std::string to_string(ret_stmt* v) {
    // std::cout << "[debug] call to_string(ret_stmt* v)" << std::endl;
    return "ret_stmt ( " + to_string(v->expr.get()) + " )";
}

std::string to_string(expr_stmt* v) {
    // std::cout << "[debug] call to_string(expr_stmt* v)" << std::endl;
    return "expr_stmt ( " + to_string(v->expr.get()) + " )";
}
}  // namespace lb::vmlua
