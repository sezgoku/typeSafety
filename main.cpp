#include <clang-c/Index.h>
#include <iostream>
#include <string>
#include <vector>

// We use this struct to pass state down the AST as we traverse it
struct LinterContext {
    CXType current_function_return_type;
    bool has_errors = false;
};

// Helper: Convert CXType to std::string
std::string getTypeName(CXType type) {
    CXString spelling = clang_getTypeSpelling(type);
    std::string result = clang_getCString(spelling);
    clang_disposeString(spelling);
    return result;
}

// Helper: Get formatted File:Line:Column string
std::string getLocation(CXCursor cursor) {
    CXSourceLocation location = clang_getCursorLocation(cursor);
    CXFile file;
    unsigned int line, column, offset;
    clang_getSpellingLocation(location, &file, &line, &column, &offset);
    
    CXString fileName = clang_getFileName(file);
    std::string locStr = std::string(clang_getCString(fileName)) + ":" + 
                         std::to_string(line) + ":" + std::to_string(column);
    clang_disposeString(fileName);
    return locStr;
}

// Main AST Visitor
CXChildVisitResult linterVisitor(CXCursor cursor, CXCursor parent, CXClientData client_data) {
    LinterContext* ctx = static_cast<LinterContext*>(client_data);
    CXCursorKind kind = clang_getCursorKind(cursor);

    // ---------------------------------------------------------
    // RULE 1: Track Function Declarations & Check Return Statements
    // ---------------------------------------------------------
    if (kind == CXCursor_FunctionDecl) {
        // Save the return type of the function we are entering
        CXType old_ret_type = ctx->current_function_return_type;
        ctx->current_function_return_type = clang_getCursorResultType(cursor);

        // Manually visit the inside of the function
        clang_visitChildren(cursor, linterVisitor, ctx);

        // Restore context when leaving the function
        ctx->current_function_return_type = old_ret_type;
        return CXChildVisit_Continue; 
    }

    if (kind == CXCursor_ReturnStmt) {
        // A ReturnStmt has a child cursor (the expression being returned)
        clang_visitChildren(cursor, [](CXCursor child, CXCursor, CXClientData data) {
            LinterContext* child_ctx = static_cast<LinterContext*>(data);
            
            // In libclang, implicit casts might hide the original type, but getCursorType evaluates the final type
            CXType expr_type = clang_getCursorType(child);
            
            // Check if the returned type matches the function's expected type
            if (expr_type.kind != child_ctx->current_function_return_type.kind && expr_type.kind != CXType_Unexposed) {
                std::cerr << "[ERROR] " << getLocation(child) 
                          << " -> Mismatched return type. Returning '" << getTypeName(expr_type) 
                          << "', but function expects '" << getTypeName(child_ctx->current_function_return_type) << "'\n";
                child_ctx->has_errors = true;
            }
            return CXChildVisit_Break; // Only need to check the immediate child of the return statement
        }, ctx);
    }

    // ---------------------------------------------------------
    // RULE 2: Check Binary Operators for Mixed Types (e.g. float + int)
    // ---------------------------------------------------------
    if (kind == CXCursor_BinaryOperator) {
        std::vector<CXType> operand_types;
        
        // Grab the types of the Left Hand Side and Right Hand Side
        clang_visitChildren(cursor, [](CXCursor child, CXCursor, CXClientData data) {
            std::vector<CXType>* types = static_cast<std::vector<CXType>*>(data);
            
            // Note: If C++ does an implicit cast (like int to float), it wraps the child in an ImplicitCastExpr.
            // A truly rigorous linter would check if `clang_getCursorKind(child) == CXCursor_UnexposedExpr` 
            // and dig one level deeper to find the *original* uncasted type.
            types->push_back(clang_getCursorType(child));
            
            return CXChildVisit_Continue;
        }, &operand_types);

        if (operand_types.size() >= 2) {
            CXType lhs = operand_types[0];
            CXType rhs = operand_types[1];

            // Strict check: If the underlying type kinds don't match exactly, flag it.
            if (lhs.kind != rhs.kind && lhs.kind != CXType_Unexposed && rhs.kind != CXType_Unexposed) {
                std::cerr << "[ERROR] " << getLocation(cursor) 
                          << " -> Invalid binary operation between '" << getTypeName(lhs) 
                          << "' and '" << getTypeName(rhs) << "'\n";
                ctx->has_errors = true;
            }
        }
    }

    return CXChildVisit_Recurse;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: ./linter <file.cpp>\n";
        return 1;
    }

    CXIndex index = clang_createIndex(0, 0);
    CXTranslationUnit unit = clang_parseTranslationUnit(
        index, argv[1], nullptr, 0, nullptr, 0, CXTranslationUnit_None);

    if (unit == nullptr) {
        std::cerr << "Fatal: Cannot parse file.\n";
        return 1; // Halt compilation step
    }

    CXCursor cursor = clang_getTranslationUnitCursor(unit);
    
    // Initialize our state tracker
    LinterContext ctx;

    // Run the linter
    clang_visitChildren(cursor, linterVisitor, &ctx);

    clang_disposeTranslationUnit(unit);
    clang_disposeIndex(index);

    // --- The Critical Step for Linters ---
    // If we found any errors, return a non-zero exit code.
    // This tells Make/CMake/Bash that the build failed.
    if (ctx.has_errors) {
        std::cerr << "Linter found errors. Compilation halted.\n";
        return 1; 
    }

    std::cout << "Linter passed successfully.\n";
    return 0;
}