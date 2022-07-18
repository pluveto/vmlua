#pragma once
#include "vm.h"
namespace lb::vmlua {

class emitter {
public:
    program compile(const ast& ast) {
        program prog;
        std::map<std::string, int32_t> locals;
        for (auto&& stmt : ast) {
            compile_statement(prog, locals, stmt.get());
        }
        return prog;
    }

    void compile_statement(program& prog, std::map<std::string, int32_t>& locals, stmt_t* stmt) {
        if (auto* p = dynamic_cast<if_stmt*>(stmt)) {
            compile_if(prog, locals, p);
        } else if (auto* p = dynamic_cast<local_stmt*>(stmt)) {
            compile_local(prog, locals, p);
        } else if (auto* p = dynamic_cast<ret_stmt*>(stmt)) {
            compile_ret(prog, locals, p);
        } else if (auto* p = dynamic_cast<expr_stmt*>(stmt)) {
            compile_expr(prog, locals, p);
        } else if (auto* p = dynamic_cast<func_decl*>(stmt)) {
            compile_func_decl(prog, locals, p);
        } else {
            throw std::runtime_error("unknown statement");
        }
    }
    void compile_if(program& prog, std::map<std::string, int32_t>& locals, if_stmt* stmt) {
        /**
         *___if a > b then
         *___jz: label_else
         *___x (stmt)
         *___jmp: label_out
         *___else
         *_[label_else]
         *___y
         *___end
         *_[label_out]
         *___z
         * */

        auto label_else = lb::string_util::concat("label_else_", prog.insts.size());
        auto label_out = lb::string_util::concat("label_out_", prog.insts.size());

        auto cond = stmt->condition.get()->clone();
        {
            // condition
            expr_stmt cond_expr(cond);
            compile_expr(prog, locals, &cond_expr);
        }
        // then body
        prog.insts.push_back(std::make_unique<jump_if_zero_inst>(label_else));
        for (auto&& stmt_ : stmt->then_body) {
            compile_statement(prog, locals, stmt_.get());
        }
        prog.insts.push_back(std::make_unique<jump_inst>(label_out));
        // else body
        // [label_else]:
        symbol sym_label_else{static_cast<int32_t>(prog.insts.size()), 0, 0};
        prog.syms.insert(std::make_pair(label_else, sym_label_else));
        for (auto&& stmt_ : stmt->else_body) {
            compile_statement(prog, locals, stmt_.get());
        }
        // [label_out]:
        symbol sym_label_out{static_cast<int32_t>(prog.insts.size()), 0, 0};
        prog.syms.insert(std::make_pair(label_out, sym_label_out));
    }
    void compile_local(program& prog, std::map<std::string, int32_t>& locals, local_stmt* local) {
        auto index = locals.size();
        locals.insert(std::make_pair(local->name.literal, static_cast<int32_t>(index)));
        auto expr_uptr = local->expr.get()->clone();
        expr_stmt expr_tmp(expr_uptr);
        compile_expr(prog, locals, &expr_tmp);
        prog.insts.push_back(std::make_unique<move_plus_fp_inst>(index));
    }
    void compile_literal(program& prog, std::map<std::string, int32_t>& locals, literal_t* lit) {
        if (auto* p = dynamic_cast<literal_number*>(lit)) {
            auto str = p->token.literal;
            auto num = std::stoi(str);
            prog.insts.push_back(std::make_unique<store_inst>(num));
        } else if (auto* p = dynamic_cast<literal_id*>(lit)) {
            prog.insts.push_back(std::make_unique<dup_plus_fp_inst>(locals[p->token.literal]));
        } else {
            throw std::runtime_error("unknown literal");
        }
    }
    void compile_function_call(program& prog, std::map<std::string, int32_t>& locals, func_call* fc) {
        auto len = fc->arguments.size();
        for (auto&& arg : fc->arguments) {
            auto tmp_uptr = arg.get()->clone();
            expr_stmt expr_tmp(tmp_uptr);
            compile_expr(prog, locals, &expr_tmp);
        }
        prog.insts.push_back(std::make_unique<call_inst>(fc->name.literal, len));
    }
    void compile_binary_op(program& prog, std::map<std::string, int32_t>& locals, binary_op* op) {
        auto tmp_uptr_l = op->left.get()->clone();
        expr_stmt expr_tmp_l(tmp_uptr_l);
        compile_expr(prog, locals, &expr_tmp_l);
        auto tmp_uptr_r = op->right.get()->clone();
        expr_stmt expr_tmp_r(tmp_uptr_r);
        compile_expr(prog, locals, &expr_tmp_r);
        auto oplit = op->op.literal;
        if (oplit == "+") {
            prog.insts.push_back(std::make_unique<add_inst>());
        } else if (oplit == "-") {
            prog.insts.push_back(std::make_unique<subtract_inst>());
        } else if (oplit == "<") {
            prog.insts.push_back(std::make_unique<logic_cond_inst>(logical_op::LT));
        } else if (oplit == ">") {
            prog.insts.push_back(std::make_unique<logic_cond_inst>(logical_op::GT));
        } else if (oplit == "<=") {
            prog.insts.push_back(std::make_unique<logic_cond_inst>(logical_op::LE));
        } else if (oplit == ">=") {
            prog.insts.push_back(std::make_unique<logic_cond_inst>(logical_op::GE));
        } else if (oplit == "==") {
            prog.insts.push_back(std::make_unique<logic_cond_inst>(logical_op::EQ));
        } else if (oplit == "!=") {
            prog.insts.push_back(std::make_unique<logic_cond_inst>(logical_op::NE));
        } else if (oplit == "&&" || oplit == "and ") {
            prog.insts.push_back(std::make_unique<logic_cond_inst>(logical_op::AND));
        } else if (oplit == "||" || oplit == "or ") {
            prog.insts.push_back(std::make_unique<logic_cond_inst>(logical_op::OR));
        } else {
            throw std::runtime_error("unknown operator");
        }
    }
    void compile_ret(program& prog, std::map<std::string, int32_t>& locals, ret_stmt* stmt) {
        auto tmp_uptr = stmt->expr.get()->clone();
        expr_stmt expr_tmp(tmp_uptr);
        compile_expr(prog, locals, &expr_tmp);
        prog.insts.push_back(std::make_unique<return_inst>());
    }
    void compile_expr(program& prog, std::map<std::string, int32_t>& locals, expr_stmt* expr) {
        if (auto* p = dynamic_cast<literal_t*>(expr->expr.get())) {
            compile_literal(prog, locals, p);
        } else if (auto* p = dynamic_cast<func_call*>(expr->expr.get())) {
            compile_function_call(prog, locals, p);
        } else if (auto* p = dynamic_cast<binary_op*>(expr->expr.get())) {
            compile_binary_op(prog, locals, p);
        } else {
            throw std::runtime_error("unknown expression");
        }
    }
    void compile_func_decl(program& prog, std::map<std::string, int32_t>& locals, func_decl* fd) {
        auto done_label = lb::string_util::concat("function_done_", prog.insts.size());
        prog.insts.push_back(std::make_unique<jump_inst>(done_label));

        // scope visibility
        std::map<std::string, int32_t> new_locals;

        auto func_index = static_cast<int32_t>(prog.insts.size());
        auto nargs = fd->params.size();
        for (auto i = 0; i < nargs; i++) {
            auto param = fd->params[i].get();
            prog.insts.push_back(std::make_unique<move_minus_fp_inst>(i, nargs - (i + 1)));
            new_locals.insert({param->literal, static_cast<int32_t>(i)});
        }

        for (auto&& stmt : fd->body) {
            compile_statement(prog, new_locals, stmt.get());
        }
        // if user forget to return, we need to add a return inst
        auto& back = prog.insts.back();
        if (auto* p = dynamic_cast<return_inst*>(back.get())) {
            // do nothing
        } else {
            prog.insts.push_back(std::make_unique<return_inst>(false));
        }

        symbol sym_func{static_cast<int32_t>(func_index), nargs, new_locals.size()};
        prog.syms.insert(std::make_pair(fd->name.literal, sym_func));

        symbol sym_done_label{static_cast<int32_t>(prog.insts.size()), 0, 0};
        prog.syms.insert(std::make_pair(done_label, sym_done_label));
    }
};
}  // namespace lb::vmlua
