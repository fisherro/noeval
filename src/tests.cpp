#include <cassert>
#include <exception>
#include <memory>
#include <print>
#include <string>

#include "noeval.hpp"
#include "parser.hpp"
#include "tests.hpp"
#include "unicode.hpp"
#include "utils.hpp"

// Helper function for running tests
struct test_runner {
    env_ptr env;
    int failures = 0;
    
    test_runner(env_ptr e) : env(e) {}
    bool test_eval(const std::string& input, const std::string& expected_output);
    bool test_error(const std::string& input, const std::string& expected_error_substring);
};

bool test_runner::test_eval(const std::string& input, const std::string& expected_output)
{
    try {
        parser p(input);
        auto expr = p.parse();
        auto result = eval(expr, env);
        auto actual_output = value_to_string(result);
        if (actual_output == expected_output) {
            std::println("✓ {} => {}", input, actual_output);
            return true;
        } else {
            println_red("✗ {}: expected {}, got {}", input, expected_output, actual_output);
            failures++;
            return false;
        }
    } catch (const std::exception& e) {
        println_red("✗ {}: threw exception: {}", input, e.what());
        failures++;
        return false;
    }
}

bool test_runner::test_error(const std::string& input, const std::string& expected_error_substring)
{
    try {
        parser p(input);
        auto expr = p.parse();
        auto result = eval(expr, env);
        println_red("✗ {}: expected error containing '{}', but got result: {}", 
                    input, expected_error_substring, value_to_string(result));
        failures++;
        return false;
    } catch (const std::exception& e) {
        std::string error_msg = e.what();
        if (error_msg.find(expected_error_substring) != std::string::npos) {
            std::println("✓ {}: correctly threw error containing '{}'", input, expected_error_substring);
            return true;
        } else {
            println_red("✗ {}: expected error containing '{}', got '{}'", 
                        input, expected_error_substring, error_msg);
            failures++;
            return false;
        }
    }
}

void test_lexer()
{
    lexer lex(R"RAW((begin (define x 42) (define y "Say, \"Hello\"")))RAW");
    token tok = lex.next_token();
    while (tok.type != token_type::eof) {
        std::println("{}", tok.to_string());
        tok = lex.next_token();
    }
}

void test_parser()
{
    parser p(R"((begin (define x 42) (define y "Hello")))");
    auto result = p.parse();
    std::println("Parsed: {}", value_to_string(result));
}

void test_evaluator()
{
    auto env = environment::make();
    
    // Test self-evaluating values
    std::println("Testing self-evaluating values:");
    
    // Test integer (now rational)
    auto int_expr = value::make(bignum(42));
    auto int_result = eval(int_expr, env);
    std::println("42 -> {}", value_to_string(int_result));
    
    // Test string
    auto str_expr = value::make(std::string("hello"));
    auto str_result = eval(str_expr, env);
    std::println("\"hello\" -> {}", value_to_string(str_result));
    
    // Test nil
    auto nil_expr = value::make(nullptr);
    auto nil_result = eval(nil_expr, env);
    std::println("nil -> {}", value_to_string(nil_result));
    
    // Test variable binding and lookup
    std::println("\nTesting variable binding:");
    env->define("x", value::make(123));
    auto sym_expr = value::make(symbol{"x"});
    auto sym_result = eval(sym_expr, env);
    std::println("x -> {}", value_to_string(sym_result));
    
    // Test undefined variable (should throw)
    std::println("\nTesting undefined variable:");
    try {
        auto undef_expr = value::make(symbol{"undefined"});
        eval(undef_expr, env);
    } catch (const std::exception& e) {
        std::println("Error (expected): {}", e.what());
    }
}

// Test vau
void test_vau()
{
    auto env = create_global_environment();
    
    std::println("Testing vau operative:");
    
    // Parse and evaluate: (vau (x) env x)
    // This creates an operative that just returns its first argument unevaluated
    parser p("(vau (x) env x)");
    auto vau_expr = p.parse();
    auto identity_op = eval(vau_expr, env);
    
    std::println("Created operative: {}", value_to_string(identity_op));
    
    // Now test using this operative
    env->define("my-op", identity_op);
    
    // Test: (my-op (+ 1 2)) should return the unevaluated expression (+ 1 2)
    parser p2("(my-op (+ 1 2))");
    auto test_expr = p2.parse();
    auto result = eval(test_expr, env);
    
    std::println("(my-op (+ 1 2)) -> {}", value_to_string(result));
}

void test_eval()
{
    auto env = create_global_environment();
    
    std::println("Testing eval operative:");
    
    // Test 1: Basic eval - (eval 42 env) should return 42
    std::println("\nTest 1: Basic eval");
    parser p1("(eval 42 env)");
    auto eval_expr1 = p1.parse();
    
    // We need to bind 'env' to the current environment for this test
    env->define("env", value::make(env));
    
    auto result1 = eval(eval_expr1, env);
    std::println("(eval 42 env) -> {}", value_to_string(result1));
    
    // Test 2: Eval a symbol - first define a variable, then eval it
    std::println("\nTest 2: Eval a symbol");
    env->define("x", value::make(123));
    parser p2("(eval x env)");
    auto eval_expr2 = p2.parse();
    auto result2 = eval(eval_expr2, env);
    std::println("x = 123, (eval x env) -> {}", value_to_string(result2));
    
    // Test 3: Eval an expression - (eval (+ 1 2) env) should return 3
    std::println("\nTest 3: Eval an expression");
    parser p3("(eval (+ 1 2) env)");
    auto eval_expr3 = p3.parse();
    auto result3 = eval(eval_expr3, env);
    std::println("(eval (+ 1 2) env) -> {}", value_to_string(result3));
    
    // Test 4: Powerful combo - use vau and eval together
    std::println("\nTest 4: vau + eval combo");
    // Create an operative that evaluates its argument twice
    parser p4("(vau (x) env (eval (eval x env) env))");
    auto double_eval_expr = p4.parse();
    auto double_eval_op = eval(double_eval_expr, env);
    env->define("double-eval", double_eval_op);
    
    // Test it: (double-eval x) where x = 123
    // First eval: x -> 123, Second eval: 123 -> 123
    parser p5("(double-eval x)");
    auto test_expr = p5.parse();
    auto result4 = eval(test_expr, env);
    std::println("(double-eval x) where x=123 -> {}", value_to_string(result4));
    
    // Test 5: Create a variable that holds an expression, then eval it
    std::println("\nTest 5: Eval stored expression");
    parser p6("(+ 10 20)");
    auto stored_expr = p6.parse();
    env->define("my-expr", stored_expr);
    
    parser p7("(eval my-expr env)");
    auto eval_stored = p7.parse();
    auto result5 = eval(eval_stored, env);
    std::println("my-expr = (+ 10 20), (eval my-expr env) -> {}", value_to_string(result5));
}

int test_self_evaluating_values()
{
    std::println("\n--- Self-evaluating values ---");
    auto env = create_global_environment();
    test_runner runner(env);
    
    runner.test_eval("42", "42");
    runner.test_eval("-17", "-17");
    runner.test_eval("\"hello\"", "\"hello\"");
    runner.test_eval("\"\"", "\"\"");
    
    return runner.failures;
}

int test_variable_operations()
{
    std::println("\n--- Variable operations ---");
    auto env = create_global_environment();
    test_runner runner(env);
    
    // Test nil symbol lookup
    env->define("nil-val", value::make(nullptr));
    runner.test_eval("nil-val", "()");
    
    // Test variable definition and lookup
    runner.test_eval("(define x 123)", "123");
    runner.test_eval("x", "123");
    runner.test_eval("(define msg \"Hello World\")", "\"Hello World\"");
    runner.test_eval("msg", "\"Hello World\"");
    
    return runner.failures;
}

int test_arithmetic_operations()
{
    std::println("\n--- Arithmetic operations ---");
    auto env = create_global_environment();
    test_runner runner(env);
    
    // Basic arithmetic
    runner.test_eval("(+ 1 2)", "3");
    runner.test_eval("(+ 1 2 3 4)", "10");
    runner.test_eval("(- 10 3)", "7");
    runner.test_eval("(- 10 3 2)", "5");
    runner.test_eval("(* 3 4)", "12");
    runner.test_eval("(* 2 3 4)", "24");
    runner.test_eval("(/ 12 3)", "4");
    runner.test_eval("(/ 24 4 2)", "3");

    // Test rational arithmetic that produces fractions
    runner.test_eval("(/ 1 3)", "0.(3)");
    runner.test_eval("(/ 22 7)", "3.(142857)");
    runner.test_eval("(/ 1 6)", "0.1(6)");
    runner.test_eval("(/ 5 4)", "1.25");
    runner.test_eval("(/ 1 2)", "0.5");

    // Nested arithmetic
    runner.test_eval("(+ (* 2 3) (- 10 5))", "11");
    runner.test_eval("(* (+ 1 2) (+ 3 4))", "21");

    // Mixed fraction/decimal arithmetic
    runner.test_eval("(+ 1/2 0.25)", "0.75");
    runner.test_eval("(* 2/3 3/4)", "0.5");

    return runner.failures;
}

int test_list_operations()
{
    std::println("\n--- List operations ---");
    auto env = create_global_environment();
    test_runner runner(env);
    
    env->define("nil-val", value::make(nullptr));
    
    runner.test_eval("(cons 1 nil-val)", "(1)");
    runner.test_eval("(cons 1 (cons 2 nil-val))", "(1 2)");
    runner.test_eval("(first (cons 42 nil-val))", "42");
    runner.test_eval("(rest (cons 1 (cons 2 nil-val)))", "(2)");
    runner.test_eval("(nil? nil-val)", "true");
    runner.test_eval("(nil? (cons 1 nil-val))", "false");

    return runner.failures;
}

int test_church_booleans()
{
    std::println("\n--- Church Boolean behavior ---");
    auto env = create_global_environment();
    test_runner runner(env);
    
    env->define("nil-val", value::make(nullptr));
    
    // Test that Church Booleans work as selectors
    runner.test_eval("((nil? nil-val) \"true\" \"false\")", "\"true\"");
    runner.test_eval("((nil? 42) \"true\" \"false\")", "\"false\"");
    runner.test_eval("((= 1 1) \"equal\" \"not-equal\")", "\"equal\"");
    runner.test_eval("((= 1 2) \"equal\" \"not-equal\")", "\"not-equal\"");
    
    // Test Church Booleans with operatives
    runner.test_eval("((nil? nil-val) (+ 1 2) (+ 3 4))", "3");
    runner.test_eval("((nil? 42) (+ 1 2) (+ 3 4))", "7");
        
    return runner.failures;
}

int test_vau_operatives()
{
    std::println("\n--- Vau operative creation ---");
    auto env = create_global_environment();
    test_runner runner(env);

    // Set up environment
    env->define("global-env", value::make(env));
    
    // Test 1: Basic vau operative creation
    runner.test_eval("(vau (x) env x)", "(operative (x) env x)");
    
    // Test 2: Variadic vau operative creation
    runner.test_eval("(vau args env args)", "(operative args env args)");
    
    // Test 3: Define and store a vau operative
    runner.test_eval("(define identity (vau (x) env x))", "(operative (x) env x)");
    
    // Test 4: Use stored operative - should return unevaluated argument
    runner.test_eval("(identity (+ 1 2))", "(+ 1 2)");
    runner.test_eval("(identity hello)", "hello");
    runner.test_eval("(identity 42)", "42");
    
    // Test 5: Variadic operative usage
    runner.test_eval("(define collect-all (vau args env args))", "(operative args env args)");
    runner.test_eval("(collect-all)", "()");
    runner.test_eval("(collect-all a)", "(a)");
    runner.test_eval("(collect-all a b c)", "(a b c)");
    runner.test_eval("(collect-all (+ 1 2) hello)", "((+ 1 2) hello)");
    
    // Test 6: Environment parameter access - operative that evaluates its argument
    runner.test_eval("(define evaluator (vau (x) e (eval x e)))", "(operative (x) e (eval x e))");
    
    // Test 7: Need to provide environment for evaluator to work
    env->define("current-env", value::make(env));
    runner.test_eval("(evaluator (+ 10 5))", "15");
    
    // Test 8: Operative that manipulates its environment parameter
    runner.test_eval("(define get-env (vau () e e))", "(operative () e e)");
    // Note: We can't easily test the result since environments print as addresses
    
    // Test 9: Nested vau creation
    runner.test_eval("(define make-identity (vau () env (vau (x) env x)))", 
                     "(operative () env (vau (x) env x))");
    runner.test_eval("((make-identity) test)", "test");
    
    // Test 10: Operative with multiple parameters
    runner.test_eval("(define first-arg (vau (x y) env x))", "(operative (x y) env x)");
    runner.test_eval("(first-arg hello world)", "hello");
    runner.test_eval("(first-arg (+ 1 2) (* 3 4))", "(+ 1 2)");

    // Test 11: Operative that evaluates only some arguments
    runner.test_eval("(define eval-second (vau (x y) e (eval y e)))", 
                     "(operative (x y) e (eval y e))");
    runner.test_eval("(eval-second dont-eval-me (+ 5 5))", "10");

    // Test 12: Environment parameter ignored with ()
    runner.test_eval("(define ignore-env-op (vau (x) () x))", "(operative (x)  x)");
    runner.test_eval("(ignore-env-op hello-world)", "hello-world");

    // Test 13: Verify () doesn't create environment binding
    runner.test_eval("(define test-no-binding (vau (x) () (eval x global-env)))", 
                    "(operative (x)  (eval x global-env))");
    env->define("test-value", value::make(42));
    runner.test_eval("(test-no-binding test-value)", "42");

    // Test 15: Compare behavior with named vs ignored environment parameter
    runner.test_eval("(define with-env (vau (x) e (eval x e)))", 
                    "(operative (x) e (eval x e))");
    runner.test_eval("(define without-env (vau (x) () x))", 
                    "(operative (x)  x)");
    runner.test_eval("(with-env (+ 1 2))", "3");   // Evaluates the expression
    runner.test_eval("(without-env (+ 1 2))", "(+ 1 2)");  // Returns unevaluated

    return runner.failures;
}

int test_eval_operative()
{
    std::println("\n--- Eval operative ---");
    auto env = create_global_environment();
    test_runner runner(env);
    
    // Set up environment
    env->define("global-env", value::make(env));
    runner.test_eval("(define x 123)", "123");  // Define x for later tests
    
    runner.test_eval("(eval 42 global-env)", "42");
    runner.test_eval("(eval x global-env)", "123");  // x was defined as 123 above
    runner.test_eval("(eval (+ 2 3) global-env)", "5");
    
    // Complex eval scenarios
    env->define("nil-val", value::make(nullptr));
    runner.test_eval("(define expr (cons (+ 1 1) nil-val))", "(2)");  // cons evaluates its arguments
    
    return runner.failures;
}

int test_invoke_operative()
{
    std::println("\n--- Invoke operative ---");
    auto env = create_global_environment();
    test_runner runner(env);
    
    env->define("nil-val", value::make(nullptr));

    runner.test_eval("(invoke + (cons 1 (cons 2 (cons 3 nil-val))))", "6");
    runner.test_eval("(invoke * (cons 2 (cons 3 (cons 4 nil-val))))", "24");

    return runner.failures;
}

int test_error_conditions()
{
    std::println("\n--- Error conditions ---");
    auto env = create_global_environment();
    test_runner runner(env);
    
    runner.test_error("undefined-var", "Unbound variable");
    runner.test_error("(+ 1 \"hello\")", "number");
    runner.test_error("(42 1 2)", "Not an operative");
    runner.test_error("(first 42)", "not a cons cell");
    runner.test_error("(vau x)", "expected 3 arguments");
    runner.test_error("(eval 42)", "expected 2 arguments");
    
    return runner.failures;
}

// Comprehensive tests for eval functionality
int test_eval_comprehensive()
{
    std::println("Running comprehensive eval tests...");
    int total_failures = 0;
    
    total_failures += test_self_evaluating_values();
    total_failures += test_variable_operations();
    total_failures += test_arithmetic_operations();
    total_failures += test_list_operations();
    total_failures += test_church_booleans();
    total_failures += test_vau_operatives();
    total_failures += test_eval_operative();
    total_failures += test_invoke_operative();
    total_failures += test_error_conditions();
    
    std::println("\nComprehensive eval tests completed: {} failures", total_failures);
    return total_failures;
}

// Test specifically for operative parameter binding
int test_parameter_binding()
{
    std::println("\nTesting parameter binding scenarios...");
    auto env = create_global_environment();
    int failures = 0;
    
    auto test_eval = [&](const std::string& input, const std::string& expected_output) -> bool {
        try {
            parser p(input);
            auto expr = p.parse();
            auto result = eval(expr, env);
            auto actual_output = value_to_string(result);
            if (actual_output == expected_output) {
                std::println("✓ {} => {}", input, actual_output);
                return true;
            } else {
                println_red("✗ {}: expected {}, got {}", input, expected_output, actual_output);
                return false;
            }
        } catch (const std::exception& e) {
            println_red("✗ {}: threw exception: {}", input, e.what());
            return false;
        }
    };
    
    env->define("nil-val", value::make(nullptr));
    
    // Test fixed parameter binding
    std::println("\n--- Fixed parameters ---");
    if (!test_eval("(define add-op (vau (x y) env (+ (eval x env) (eval y env))))", 
              "(operative (x y) env (+ (eval x env) (eval y env)))")) failures++;
    if (!test_eval("(add-op 3 4)", "7")) failures++;
    if (!test_eval("(add-op (+ 1 1) (* 2 3))", "8")) failures++;

    // Test environment parameter access
    std::println("\n--- Environment parameter ---");
    if (!test_eval("(define show-env (vau (var) e (eval var e)))", 
              "(operative (var) e (eval var e))")) failures++;
    if (!test_eval("(define test-var 999)", "999")) failures++;
    if (!test_eval("(show-env test-var)", "999")) failures++;
    
    std::println("Parameter binding tests completed: {} failures", failures);
    return failures;
}

int test_inline_comments()
{
    std::println("\n--- Inline comments ---");
    auto env = create_global_environment();
    test_runner runner(env);
    
    // Basic inline comments
    runner.test_eval("42 ; this is a comment", "42");
    runner.test_eval("(+ 1 2) ; adding numbers", "3");
    runner.test_eval("(+ 1 ; first number\n   2) ; second number", "3");
    
    // Comments at different positions
    runner.test_eval("; comment at start\n42", "42");
    runner.test_eval("42\n; comment after", "42");
    runner.test_eval("(+ 1 2 ; inline comment\n   3)", "6");
    
    // Multiple comments
    runner.test_eval("; first comment\n; second comment\n42", "42");
    runner.test_eval("(+ 1 ; comment 1\n   2 ; comment 2\n   3)", "6");
    
    // Comments with various content
    runner.test_eval("42 ; comment with (parens)", "42");
    runner.test_eval("42 ; comment with \"quotes\"", "42");
    runner.test_eval("42 ; comment with ; semicolons", "42");
    
    // Edge cases
    runner.test_eval("42;no space before comment", "42");
    runner.test_eval("42 ;", "42");  // Empty comment
    runner.test_eval("42 ; \n", "42");  // Comment with just whitespace
    
    // Comments should NOT interfere with string literals
    runner.test_eval("\"string with ; semicolon\"", "\"string with ; semicolon\"");
    runner.test_eval("\"string with \\\" quote ; and comment\"", "\"string with \\\" quote ; and comment\"");
    
    // Comments with symbols/operators - when evaluated, + returns the operative
    runner.test_eval("+ ; this should not break symbol parsing", "#<builtin-operative:+>");
    runner.test_eval("(define x 42) ; define a variable", "42");
    runner.test_eval("x ; use the variable", "42");
    
    return runner.failures;
}

void test_lexer_comments()
{
    std::println("\n--- Lexer comment handling ---");
    
    // Test that comments are properly skipped in tokenization
    lexer lex1("42 ; comment");
    auto tok1 = lex1.next_token();
    assert(tok1.type == token_type::number && tok1.value == "42");
    auto tok2 = lex1.next_token();
    assert(tok2.type == token_type::eof);
    std::println("✓ Simple inline comment");
    
    // Test comment at start of line
    lexer lex2("; comment\n42");
    auto tok3 = lex2.next_token();
    assert(tok3.type == token_type::number && tok3.value == "42");
    std::println("✓ Comment at start of line");
    
    // Test comment in expression
    lexer lex3("(+ 1 ; comment\n 2)");
    auto tok4 = lex3.next_token();
    assert(tok4.type == token_type::left_paren);
    auto tok5 = lex3.next_token();
    assert(tok5.type == token_type::symbol && tok5.value == "+");
    auto tok6 = lex3.next_token();
    assert(tok6.type == token_type::number && tok6.value == "1");
    auto tok7 = lex3.next_token();
    assert(tok7.type == token_type::number && tok7.value == "2");
    auto tok8 = lex3.next_token();
    assert(tok8.type == token_type::right_paren);
    std::println("✓ Comment within expression");
    
    // Test that semicolon in string is preserved
    lexer lex4("\"string ; with semicolon\"");
    auto tok9 = lex4.next_token();
    assert(tok9.type == token_type::string_literal && tok9.value == "string ; with semicolon");
    std::println("✓ Semicolon preserved in string literal");
    
    std::println("All lexer comment tests passed!");
}

int test_operative_as_first_element()
{
    std::println("\n--- Operative values as first element ---");
    auto env = create_global_environment();
    test_runner runner(env);
    
    env->define("nil-val", value::make(nullptr));
    
    // Test 1: Direct operative value invocation
    // This should work: ((vau args env args) 1 2 3)
    runner.test_eval("((vau args env args) 1 2 3)", "(1 2 3)");
    
    // Test 2: Operative value from variable
    runner.test_eval("(define my-op (vau (x) env x))", "(operative (x) env x)");
    runner.test_eval("(my-op hello)", "hello");

    // Test 3: Operative value from Church Boolean selection  
    runner.test_eval("(((nil? ()) (vau (x) env x) (vau (y) env y)) test)", "test");

    // Test 4: Nested operative invocations
    runner.test_eval("(((vau () env (vau (x) env x))) world)", "world");
    
    // Test 5: Built-in operative as value
    runner.test_eval("(define plus-op +)", "#<builtin-operative:+>");
    runner.test_eval("(plus-op 1 2 3)", "6");
    
    // Test 6: Mixed scenarios - operative returned from evaluation
    runner.test_eval("(define make-identity (vau () env (vau (x) env x)))", 
                     "(operative () env (vau (x) env x))");
    runner.test_eval("((make-identity) foo)", "foo");
    
    return runner.failures;
}

int test_mutable_bindings()
{
    std::println("\n--- Mutable bindings ---");
    auto env = create_global_environment();
    test_runner runner(env);
    
    // Test 1: Basic mutable definition and initial value
    runner.test_eval("(define-mutable x 42)", "42");
    runner.test_eval("x", "42");
    
    // Test 2: Successful mutation
    runner.test_eval("(set! x 100)", "100");
    runner.test_eval("x", "100");
    
    // Test 3: Multiple mutations
    runner.test_eval("(set! x (+ x 5))", "105");
    runner.test_eval("x", "105");
    
    // Test 4: Mutable with different data types
    runner.test_eval("(define-mutable msg \"hello\")", "\"hello\"");
    runner.test_eval("(set! msg \"world\")", "\"world\"");
    runner.test_eval("msg", "\"world\"");
    
    // Test 5: Error - trying to set! an immutable binding
    runner.test_eval("(define y 50)", "50");  // Regular define (immutable)
    runner.test_error("(set! y 60)", "not mutable");
    runner.test_eval("y", "50");  // Should be unchanged
    
    // Test 6: Error - set! on undefined variable
    runner.test_error("(set! undefined-var 123)", "Unbound variable");
    
    // Test 7: Error - define-mutable with wrong argument count
    runner.test_error("(define-mutable)", "expected 2 arguments");
    runner.test_error("(define-mutable x)", "expected 2 arguments");
    runner.test_error("(define-mutable x 1 2)", "expected 2 arguments");
    
    // Test 8: Error - define-mutable with non-symbol first argument
    runner.test_error("(define-mutable 123 456)", "must be a symbol");
    runner.test_error("(define-mutable \"x\" 456)", "must be a symbol");
    
    // Test 9: Error - set! with wrong argument count
    runner.test_error("(set!)", "expected 2 arguments");
    runner.test_error("(set! x)", "expected 2 arguments");
    runner.test_error("(set! x 1 2)", "expected 2 arguments");
    
    // Test 10: Error - set! with non-symbol first argument
    runner.test_error("(set! 123 456)", "must be a symbol");
    
    // Test 11: Mutable bindings work across scopes
    runner.test_eval("(define-mutable counter 0)", "0");
    runner.test_eval("(define increment (vau () env (set! counter (+ counter 1))))", 
                     "(operative () env (set! counter (+ counter 1)))");
    runner.test_eval("(increment)", "1");
    runner.test_eval("(increment)", "2");
    runner.test_eval("counter", "2");
    
    // Test 12: Mutable binding with nil value
    env->define("nil-val", value::make(nullptr));
    runner.test_eval("(define-mutable nullable nil-val)", "()");
    runner.test_eval("(set! nullable 42)", "42");
    runner.test_eval("nullable", "42");
    
    return runner.failures;
}

int test_number_parsing()
{
    std::println("\n--- Number parsing ---");
    auto env = create_global_environment();
    test_runner runner(env);
    
    // Test basic integer parsing
    runner.test_eval("0", "0");
    runner.test_eval("42", "42");
    runner.test_eval("-17", "-17");
    
    // Test decimal parsing
    runner.test_eval("0.5", "0.5");
    runner.test_eval("3.14", "3.14");
    runner.test_eval("-2.718", "-2.718");
    runner.test_eval("0.0", "0");
    
    // Test fraction parsing  
    runner.test_eval("1/2", "0.5");
    runner.test_eval("1/3", "0.(3)");
    runner.test_eval("22/7", "3.(142857)");
    runner.test_eval("-5/6", "-0.8(3)");
    runner.test_eval("7/1", "7");
    
    // Test repeating decimal parsing
    runner.test_eval("0.(3)", "0.(3)");
    runner.test_eval("0.1(6)", "0.1(6)");
    runner.test_eval("3.(142857)", "3.(142857)");
    runner.test_eval("-0.(9)", "-1");  // 0.999... = 1
    
    // Test edge cases
    runner.test_eval("0.25", "0.25");
    runner.test_eval("1.0", "1");
    runner.test_eval("10/5", "2");
    
    return runner.failures;
}

int test_number_operations()
{
    std::println("\n--- Advanced number operations ---");
    auto env = create_global_environment();
    test_runner runner(env);
    
    // Test numerator/denominator extraction
    runner.test_eval("(numerator 22/7)", "22");
    runner.test_eval("(denominator 22/7)", "7");
    runner.test_eval("(numerator 0.5)", "1");
    runner.test_eval("(denominator 0.5)", "2");
    runner.test_eval("(numerator 42)", "42");
    runner.test_eval("(denominator 42)", "1");
    
    // Test comparison with spaceship operator
    runner.test_eval("(<=> 1 2)", "-1");
    runner.test_eval("(<=> 2 1)", "1");
    runner.test_eval("(<=> 2 2)", "0");
    runner.test_eval("(<=> 1/2 0.5)", "0");
    runner.test_eval("(<=> 1/3 0.33)", "1");  // 1/3 > 0.33
    
    // Test modulo with integers
    runner.test_eval("(remainder 7 3)", "1");
    runner.test_eval("(remainder 10 4)", "2");
    runner.test_eval("(remainder -7 3)", "-1");
    
    // Test remainder with rational numbers (now supported)
    runner.test_eval("(remainder 1.5 2)", "1.5");  // 1.5 - truncate(1.5/2) * 2 = 1.5 - 0 * 2 = 1.5
    runner.test_eval("(remainder 7 2.5)", "2");    // 7 - truncate(7/2.5) * 2.5 = 7 - 2 * 2.5 = 2
    
    return runner.failures;
}

int test_unicode_functions()
{
    std::println("\n--- Unicode conversion functions ---");
    int failures = 0;
    
    auto test_utf32_to_utf8 = [&](const std::u32string& input, const std::string& expected_hex) -> bool {
        try {
            auto result = utf32_to_utf8(input);
            std::string hex_result;
            for (char8_t byte : result) {
                hex_result += std::format("{:02X}", static_cast<uint8_t>(byte));
            }
            if (hex_result == expected_hex) {
                std::println("✓ UTF-32 to UTF-8: U+{} => {}", 
                           [&]() {
                               std::string codepoints;
                               for (char32_t cp : input) {
                                   if (not codepoints.empty()) codepoints += " ";
                                   codepoints += std::format("{:04X}", static_cast<uint32_t>(cp));
                               }
                               return codepoints;
                           }(), hex_result);
                return true;
            } else {
                println_red("✗ UTF-32 to UTF-8: expected {}, got {}", expected_hex, hex_result);
                failures++;
                return false;
            }
        } catch (const std::exception& e) {
            println_red("✗ UTF-32 to UTF-8: threw exception: {}", e.what());
            failures++;
            return false;
        }
    };
    
    auto test_utf8_to_utf32 = [&](const std::string& input_hex, const std::u32string& expected) -> bool {
        try {
            // Convert hex string to u8string
            std::u8string input;
            for (size_t i = 0; i < input_hex.length(); i += 2) {
                std::string byte_str = input_hex.substr(i, 2);
                auto byte_val = static_cast<char8_t>(std::stoi(byte_str, nullptr, 16));
                input.push_back(byte_val);
            }
            
            auto result = utf8_to_utf32(input);
            if (result == expected) {
                std::println("✓ UTF-8 to UTF-32: {} => U+{}", input_hex,
                           [&]() {
                               std::string codepoints;
                               for (char32_t cp : result) {
                                   if (not codepoints.empty()) codepoints += " ";
                                   codepoints += std::format("{:04X}", static_cast<uint32_t>(cp));
                               }
                               return codepoints;
                           }());
                return true;
            } else {
                println_red("✗ UTF-8 to UTF-32: wrong result");
                failures++;
                return false;
            }
        } catch (const std::exception& e) {
            println_red("✗ UTF-8 to UTF-32: threw exception: {}", e.what());
            failures++;
            return false;
        }
    };
    
    auto test_utf32_error = [&](const std::u32string& input, const std::string& expected_error) -> bool {
        try {
            auto result = utf32_to_utf8(input);
            println_red("✗ UTF-32 error test: expected error containing '{}', but got result", expected_error);
            failures++;
            return false;
        } catch (const std::exception& e) {
            std::string error_msg = e.what();
            if (error_msg.find(expected_error) != std::string::npos) {
                std::println("✓ UTF-32 error: correctly threw error containing '{}'", expected_error);
                return true;
            } else {
                println_red("✗ UTF-32 error: expected '{}', got '{}'", expected_error, error_msg);
                failures++;
                return false;
            }
        }
    };
    
    auto test_utf8_error = [&](const std::string& input_hex, const std::string& expected_error) -> bool {
        try {
            // Convert hex string to u8string
            std::u8string input;
            for (size_t i = 0; i < input_hex.length(); i += 2) {
                std::string byte_str = input_hex.substr(i, 2);
                auto byte_val = static_cast<char8_t>(std::stoi(byte_str, nullptr, 16));
                input.push_back(byte_val);
            }
            
            auto result = utf8_to_utf32(input);
            println_red("✗ UTF-8 error test: expected error containing '{}', but got result", expected_error);
            failures++;
            return false;
        } catch (const std::exception& e) {
            std::string error_msg = e.what();
            if (error_msg.find(expected_error) != std::string::npos) {
                std::println("✓ UTF-8 error: correctly threw error containing '{}'", expected_error);
                return true;
            } else {
                println_red("✗ UTF-8 error: expected '{}', got '{}'", expected_error, error_msg);
                failures++;
                return false;
            }
        }
    };
    
    // Test basic ASCII characters (1-byte UTF-8)
    std::println("\n--- ASCII characters ---");
    test_utf32_to_utf8(U"A", "41");
    test_utf32_to_utf8(U"Hello", "48656C6C6F");
    test_utf8_to_utf32("41", U"A");
    test_utf8_to_utf32("48656C6C6F", U"Hello");
    
    // Test 2-byte UTF-8 sequences
    std::println("\n--- 2-byte sequences ---");
    test_utf32_to_utf8(U"é", "C3A9");        // U+00E9 (Latin small letter e with acute)
    test_utf32_to_utf8(U"ñ", "C3B1");        // U+00F1 (Latin small letter n with tilde)
    test_utf8_to_utf32("C3A9", U"é");
    test_utf8_to_utf32("C3B1", U"ñ");
    
    // Test 3-byte UTF-8 sequences
    std::println("\n--- 3-byte sequences ---");
    test_utf32_to_utf8(U"€", "E282AC");      // U+20AC (Euro sign)
    test_utf32_to_utf8(U"한", "ED959C");      // U+D55C (Korean character)
    test_utf8_to_utf32("E282AC", U"€");
    test_utf8_to_utf32("ED959C", U"한");
    
    // Test 4-byte UTF-8 sequences
    std::println("\n--- 4-byte sequences ---");
    test_utf32_to_utf8(U"𝄞", "F09D849E");     // U+1D11E (Musical symbol treble clef)
    test_utf32_to_utf8(U"😀", "F09F9880");     // U+1F600 (Grinning face emoji)
    test_utf8_to_utf32("F09D849E", U"𝄞");
    test_utf8_to_utf32("F09F9880", U"😀");
    
    // Test boundary conditions
    std::println("\n--- Boundary conditions ---");
    test_utf32_to_utf8(std::u32string(1, 0x7F), "7F");           // Last 1-byte
    test_utf32_to_utf8(std::u32string(1, 0x80), "C280");         // First 2-byte
    test_utf32_to_utf8(std::u32string(1, 0x7FF), "DFBF");        // Last 2-byte
    test_utf32_to_utf8(std::u32string(1, 0x800), "E0A080");      // First 3-byte
    test_utf32_to_utf8(std::u32string(1, 0xFFFF), "EFBFBF");     // Last 3-byte
    test_utf32_to_utf8(std::u32string(1, 0x10000), "F0908080");  // First 4-byte
    test_utf32_to_utf8(std::u32string(1, 0x10FFFF), "F48FBFBF"); // Last valid Unicode
    
    // Test empty string
    std::println("\n--- Empty string ---");
    test_utf32_to_utf8(U"", "");
    test_utf8_to_utf32("", U"");
    
    // Test mixed character lengths
    std::println("\n--- Mixed sequences ---");
    test_utf32_to_utf8(U"A€😀", "41E282ACF09F9880");
    test_utf8_to_utf32("41E282ACF09F9880", U"A€😀");
    
    // Test error conditions - Invalid codepoints
    std::println("\n--- Invalid codepoint errors ---");
    test_utf32_error(std::u32string(1, 0x110000), "must be <= U+10FFFF");     // Too high
    test_utf32_error(std::u32string(1, 0xD800), "surrogate pair range");      // Surrogate
    test_utf32_error(std::u32string(1, 0xDFFF), "surrogate pair range");      // Surrogate
    
    // Test error conditions - Invalid UTF-8
    std::println("\n--- Invalid UTF-8 errors ---");
    test_utf8_error("80", "Invalid UTF-8 start byte");           // Continuation byte as start
    test_utf8_error("C0", "Truncated UTF-8 sequence");           // Incomplete 2-byte
    test_utf8_error("E0A0", "Truncated UTF-8 sequence");         // Incomplete 3-byte
    test_utf8_error("F0908080FF", "Invalid UTF-8 start byte");   // Valid 4-byte + truly invalid start byte
    test_utf8_error("C080", "Overlong UTF-8 encoding");          // Overlong 2-byte for ASCII
    test_utf8_error("E08080", "Overlong UTF-8 encoding");        // Overlong 3-byte
    test_utf8_error("F0808080", "Overlong UTF-8 encoding");      // Overlong 4-byte
    test_utf8_error("EDA080", "surrogate pair");                 // UTF-8 encoded surrogate
    test_utf8_error("F7BFBFBF", "outside Unicode range");        // Beyond U+10FFFF
    test_utf8_error("C020", "Invalid UTF-8 continuation byte");  // Space instead of continuation
    
    // Test round-trip conversion
    std::println("\n--- Round-trip conversion ---");
    auto test_roundtrip = [&](const std::u32string& original) -> bool {
        try {
            auto utf8_result = utf32_to_utf8(original);
            auto utf32_result = utf8_to_utf32(utf8_result);
            if (utf32_result == original) {
                std::println("✓ Round-trip: {} characters", original.length());
                return true;
            } else {
                println_red("✗ Round-trip failed");
                failures++;
                return false;
            }
        } catch (const std::exception& e) {
            println_red("✗ Round-trip threw: {}", e.what());
            failures++;
            return false;
        }
    };
    
    test_roundtrip(U"Hello, 世界! 🌍");
    test_roundtrip(U"Mixed: AéЯ中🎵");
    test_roundtrip(std::u32string(1, 0x10FFFF)); // Maximum valid codepoint
    
    std::println("Unicode function tests completed: {} failures", failures);
    return failures;
}

int test_string_primitives()
{
    std::println("\n--- String conversion primitives ---");
    auto env = create_global_environment();
    test_runner runner(env);
    
    // Basic conversions
    runner.test_eval("(string->list \"hi\")", "(104 105)");
    runner.test_eval("(list->string (cons 104 (cons 105 ())))", "\"hi\"");
    runner.test_eval("(string->list \"\")", "()");
    runner.test_eval("(list->string ())", "\"\"");
    
    // Unicode support
    runner.test_eval("(string->list \"é\")", "(233)");  // U+00E9
    runner.test_eval("(list->string (cons 233 ()))", "\"é\"");
    runner.test_eval("(string->list \"😀\")", "(128512)");  // U+1F600
    runner.test_eval("(list->string (cons 128512 ()))", "\"😀\"");
    
    // Round-trip conversion
    runner.test_eval("(list->string (string->list \"Hello, 世界!\"))", "\"Hello, 世界!\"");
    
    // Error conditions
    runner.test_error("(string->list)", "expected 1 argument");
    runner.test_error("(string->list 42)", "argument must be a string");
    runner.test_error("(list->string 42)", "argument must be a list");
    runner.test_error("(list->string (cons \"not-number\" ()))", "all elements must be numbers");
    runner.test_error("(list->string (cons 0.5 ()))", "must be an integer");
    runner.test_error("(list->string (cons 1114112 ()))", "Invalid Unicode codepoint");  // 0x110000
    runner.test_error("(list->string (cons 55296 ()))", "surrogate");  // 0xD800
    
    return runner.failures;
}

bool run_tests()
{
    // Run existing tests (these could also be converted to return failure counts)
    test_lexer();
    test_lexer_comments();
    std::println("---");
    test_parser();
    std::println("---");
    test_evaluator();
    std::println("---");
    test_vau();
    std::println("---");
    test_eval();
    
    // Run comprehensive tests and count failures
    int failures{0};
    std::println("\n{}", std::string(60, '='));
    failures += test_eval_comprehensive();
    failures += test_inline_comments();
    failures += test_operative_as_first_element();
    std::println("{}", std::string(60, '='));
    failures += test_parameter_binding();
    std::println("{}", std::string(60, '='));
    failures += test_mutable_bindings();
    std::println("{}", std::string(60, '='));
    failures += test_number_parsing();
    failures += test_number_operations();
    failures += test_unicode_functions();
    failures += test_string_primitives();
    std::println("{}", std::string(60, '='));

    if (failures != 0) {
        std::println("\n✗ {} test(s) failed!", failures);
        return false;
    }

    std::println("\n✓ All comprehensive tests passed!");
    std::println("{}\n", std::string(50, '='));
    return true;
}