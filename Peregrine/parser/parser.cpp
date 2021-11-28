#include "parser.hpp"
#include "lexer/lexer.hpp"
#include "lexer/tokens.hpp"

#include <iostream>
#include <map>
#include <memory>
#include <vector>

using namespace ast;

std::map<TokenType, PrecedenceType> createMap() {
    std::map<TokenType, PrecedenceType> precedenceMap;

    precedenceMap[tk_negative] = pr_prefix;
    precedenceMap[tk_bit_not] = pr_prefix;
    precedenceMap[tk_and] = pr_and_or;
    precedenceMap[tk_or] = pr_and_or;
    precedenceMap[tk_not] = pr_not;
    precedenceMap[tk_not_equal] = pr_compare;
    precedenceMap[tk_is_not] = pr_compare;
    precedenceMap[tk_is] = pr_compare;
    precedenceMap[tk_not_in] = pr_compare;
    precedenceMap[tk_in] = pr_compare;
    precedenceMap[tk_greater] = pr_compare;
    precedenceMap[tk_less] = pr_compare;
    precedenceMap[tk_gr_or_equ] = pr_compare;
    precedenceMap[tk_less_or_equ] = pr_compare;
    precedenceMap[tk_equal] = pr_compare;
    precedenceMap[tk_bit_or] = pr_bit_or;
    precedenceMap[tk_xor] = pr_bit_xor;
    precedenceMap[tk_bit_and] = pr_bit_and;
    precedenceMap[tk_shift_left] = pr_bit_shift;
    precedenceMap[tk_shift_right] = pr_bit_shift;
    precedenceMap[tk_plus] = pr_sum_minus;
    precedenceMap[tk_minus] = pr_sum_minus;
    precedenceMap[tk_multiply] = pr_mul_div;
    precedenceMap[tk_divide] = pr_mul_div;
    precedenceMap[tk_modulo] = pr_mul_div;
    precedenceMap[tk_floor] = pr_mul_div;
    precedenceMap[tk_exponent] = pr_expo;
    precedenceMap[tk_dot] = pr_dot_ref;
    precedenceMap[tk_list_open] = pr_list_access;
    precedenceMap[tk_l_paren] = pr_call;

    return precedenceMap;
}

Parser::Parser(const std::vector<Token>& tokens) : m_tokens(tokens) {
    m_currentToken = tokens[0];
}

Parser::~Parser() {}

void Parser::advance() {
    m_tokIndex++;

    if (m_tokIndex < m_tokens.size()) {
        m_currentToken = m_tokens[m_tokIndex];
    }
}

void Parser::advanceOnNewLine() {
    if (next().tkType == tk_new_line) {
        advance();
    }
}

Token Parser::next() {
    Token token;

    if (m_tokIndex + 1 < m_tokens.size()) {
        token = m_tokens[m_tokIndex + 1];
    }

    return token;
}

PrecedenceType Parser::nextPrecedence() {
    if (precedenceMap.count(next().tkType) > 0) {
        return precedenceMap[next().tkType];
    }

    return pr_lowest;
}

void Parser::error(Token tok, std::string_view msg) {
    PEError err = {{tok.line, tok.start, m_filename, tok.statement},
                   std::string(msg),
                   "",
                   "",
                   ""};

    m_errors.push_back(err);
}

void Parser::expect(TokenType expectedType) {
    if (next().tkType != expectedType) {
        error(next(), "expected token of type " + std::to_string(expectedType) +
                          ", got " + std::to_string(next().tkType) +
                          " instead");
    }

    advance();
}

AstNodePtr Parser::parse() {
    std::vector<AstNodePtr> statements;

    while (m_currentToken.tkType != tk_eof) {
        statements.push_back(parseStatement());
        advance();
    }
    if (!m_errors.empty()) {
        for (auto& err : m_errors) {
            display(err);
        }

        exit(1);
    }

    return std::make_shared<Program>(statements);
}

AstNodePtr Parser::parseStatement() {
    AstNodePtr stmt;

    switch (m_currentToken.tkType) {
        case tk_const: {
            stmt = parseConstDeclaration();
            break;
        }
        case tk_at:{
            stmt = parseDecoratorCall();
            break;
        }
        case tk_if: {
            stmt = parseIf();
            break;
        }

        case tk_while: {
            stmt = parseWhile();
            break;
        }

        case tk_for: {
            stmt = parseFor();
            break;
        }

        case tk_from:
        case tk_import: {
            stmt = parseImport();
            break;
        }

        case tk_def: {
            stmt = parseFunctionDef();
            break;
        }

        case tk_break: {
            stmt = std::make_shared<BreakStatement>(m_currentToken);
            advanceOnNewLine();
            break;
        }

        case tk_pass: {
            stmt = std::make_shared<PassStatement>(m_currentToken);
            advanceOnNewLine();
            break;
        }
        case tk_match:{
            stmt = parseMatch();
            break;
        }
        case tk_continue: {
            stmt = std::make_shared<ContinueStatement>(m_currentToken);
            advanceOnNewLine();
            break;
        }

        case tk_return: {
            stmt = parseReturn();
            break;
        }
        case tk_scope: {
            stmt = parseScope();
            break;
        }
        case tk_cppcode: {
            advance();
            stmt = parseCpp();
            advanceOnNewLine();
            break;
        }

        case tk_type: {
            stmt = parseTypeDef();
            break;
        }

        case tk_identifier: {
            if (next().tkType == tk_identifier || next().tkType == tk_assign) {
                // variable
                stmt = parseVariableStatement();
                break;
            }

            // if it got here, it will go down the cases and match the default
            // case. DO NOT add another case below this one
        }

        default: {
            /*
                if it didn't match the statements above, then it must be
                either an expression or invalid
            */

            stmt = parseExpression();
            break;
        }
    }

    return stmt;
}

AstNodePtr Parser::parseBlockStatement() {
    advance(); // when this is called, we are on the tk_ident token

    std::vector<AstNodePtr> statements;

    while (m_currentToken.tkType != tk_dedent) {
        if (m_currentToken.tkType == tk_eof) {
            error(m_currentToken,
                  "expected end of identation, got eof instead");
        }

        statements.push_back(parseStatement());
        advance();
    }

    return std::make_shared<BlockStatement>(statements);
}

AstNodePtr Parser::parseImport() {
    Token tok = m_currentToken;
    bool hasFrom = m_currentToken.tkType == tk_from;

    advance(); // skip from or import token 

    std::pair<AstNodePtr, AstNodePtr> moduleName;
    std::vector<std::pair<AstNodePtr, AstNodePtr>> importedSymbols;

    moduleName.first = parseName();

    if (!hasFrom) {
        if (next().tkType == tk_as) {
            advance();
            advance();

            moduleName.second = parseName();
        }
        //advance on new line?
        return std::make_shared<ImportStatement>(tok, moduleName, importedSymbols);
    }

    expect(tk_import);

    do {
        advance();

        std::pair<AstNodePtr, AstNodePtr> importedSymbol;

        importedSymbol.first = parseName();

        if (next().tkType == tk_as) {
            advance();
            advance();

            importedSymbol.second = parseName();
        }

        importedSymbols.push_back(importedSymbol);

        if (next().tkType == tk_comma)
            advance();

    } while (m_currentToken.tkType == tk_comma);

    advanceOnNewLine();
    return std::make_shared<ImportStatement>(tok, moduleName, importedSymbols);
}

// TODO: make this invalid: int = 43
AstNodePtr Parser::parseVariableStatement() {
    Token tok = m_currentToken;
    AstNodePtr varType = std::make_shared<NoLiteral>();

    if (next().tkType == tk_identifier) {
        varType = parseType();
        advance();
    }

    AstNodePtr name = parseName();

    AstNodePtr value = std::make_shared<NoLiteral>();

    if (next().tkType == tk_assign) {
        advance();
        advance();

        value = parseExpression();
    } else {
        advanceOnNewLine();
    }

    return std::make_shared<VariableStatement>(tok, varType, name, value);
}

AstNodePtr Parser::parseConstDeclaration() {
    Token tok = m_currentToken;
    expect(tk_identifier);

    AstNodePtr constType = std::make_shared<NoLiteral>();

    if (next().tkType == tk_identifier) {
        constType = parseType();
        advance();
    }

    AstNodePtr name = parseName();

    expect(tk_assign);
    advance();

    AstNodePtr value = parseExpression();

    return std::make_shared<ConstDeclaration>(tok, constType, name, value);
}

AstNodePtr Parser::parseIf() {
    Token tok = m_currentToken;
    advance(); // skip the if token

    AstNodePtr condition = parseExpression();

    expect(tk_colon);

    // TODO: maybe support single-line ifs
    expect(tk_ident);

    AstNodePtr ifBody = parseBlockStatement();

    std::vector<std::pair<AstNodePtr, AstNodePtr>> elifs;

    while (next().tkType == tk_elif) {
        advance();
        advance();

        AstNodePtr condition = parseExpression();

        expect(tk_colon);
        expect(tk_ident);

        AstNodePtr body = parseBlockStatement();

        elifs.push_back(std::pair(condition, body));
    }

    AstNodePtr elseBody = std::make_shared<NoLiteral>();

    if (next().tkType == tk_else) {
        advance();

        expect(tk_colon);
        expect(tk_ident);

        elseBody = parseBlockStatement();
    }

    return std::make_shared<IfStatement>(tok, condition, ifBody, elseBody, elifs);
}

AstNodePtr Parser::parseScope() {
    Token tok = m_currentToken;
    expect(tk_colon);

    // TODO:  support single-line scope
    expect(tk_ident);

    AstNodePtr scope_body = parseBlockStatement();
    return std::make_shared<ScopeStatement>(tok, scope_body);
}

AstNodePtr Parser::parseWhile() {
    Token tok = m_currentToken;
    advance(); // skip the while token

    AstNodePtr condition = parseExpression();

    expect(tk_colon);
    expect(tk_ident);

    AstNodePtr body = parseBlockStatement();

    return std::make_shared<WhileStatement>(tok, condition, body);
}

AstNodePtr Parser::parseFor() {
    Token tok = m_currentToken;
    advance();

    AstNodePtr variable = parseName();

    expect(tk_in);
    advance();

    AstNodePtr sequence = parseExpression();

    expect(tk_colon);
    expect(tk_ident);

    AstNodePtr body = parseBlockStatement();

    return std::make_shared<ForStatement>(tok, variable, sequence, body);
}

AstNodePtr Parser::parseFunctionDef() {
    Token tok = m_currentToken;
    expect(tk_identifier);

    AstNodePtr name = parseName();

    expect(tk_l_paren);

    std::vector<parameter> parameters;

    if (next().tkType != tk_r_paren) {
        do {
            advance();

            // TODO: make this a separate function
            if (m_currentToken.tkType != tk_identifier) {
                error(next(), "expected token of type " +
                                  std::to_string(tk_identifier) + ", got " +
                                  std::to_string(m_currentToken.tkType) +
                                  " instead");
            }

            AstNodePtr paramType = parseType();
            expect(tk_identifier);
            AstNodePtr paramName = parseName();

            parameters.push_back(parameter{paramType, paramName});
            advance();
        } while (m_currentToken.tkType == tk_comma);
    } else {
        advance();
    }

    if (m_currentToken.tkType != tk_r_paren) {
        error(m_currentToken,
              "expected ), got " + m_currentToken.keyword + " instead");
    }

    // returns void by default
    AstNodePtr returnType = std::make_shared<IdentifierExpression>(m_currentToken, "void");

    if (next().tkType == tk_arrow) {
        advance();
        expect(tk_identifier);

        returnType = parseType();
    }

    expect(tk_colon);
    expect(tk_ident);

    AstNodePtr body = parseBlockStatement();

    return std::make_shared<FunctionDefinition>(tok, returnType, name, parameters,
                                                body);
}

AstNodePtr Parser::parseReturn() {
    Token tok = m_currentToken;
    AstNodePtr returnValue = std::make_shared<NoLiteral>();

    if (next().tkType != tk_new_line) {
        advance();
        returnValue = parseExpression();
    } else {
        advance();
    }

    return std::make_shared<ReturnStatement>(tok, returnValue);
}

AstNodePtr Parser::parseTypeDef() {
    Token tok = m_currentToken;
    advance();

    AstNodePtr name = parseName(); 

    expect(tk_assign);
    advance();
    AstNodePtr type;
    if (m_currentToken.tkType==tk_identifier){
        type = parseType();
    }
    else if (m_currentToken.tkType==tk_def){
        type = parseLamda();
    }
    else{
        //TODO: throw error
    }

    advanceOnNewLine();
    return std::make_shared<TypeDefinition>(tok, name, type);
}
AstNodePtr Parser::parseLamda() {
    auto tok=m_currentToken;
    expect(tk_l_paren);
    std::vector<AstNodePtr> types;//arg types
    std::vector<AstNodePtr> return_types;
    while (m_currentToken.tkType!=tk_r_paren){
        advance();
        if (m_currentToken.tkType==tk_identifier){
            types.push_back(parseName());
        }
        else if (m_currentToken.tkType==tk_comma) {
            expect(tk_identifier);
            types.push_back(parseName());
        }
        else if (m_currentToken.tkType==tk_r_paren){
            break;
        }
        else{
            //TODO: throw error
        }
        advance();
    }
    if (next().tkType==tk_arrow){
        advance();
        expect(tk_identifier);
        //TODO: Implement multiple return
        return_types.push_back(parseName());
    }
    return std::make_shared<LamdaDefine>(tok, types, return_types);
}
AstNodePtr Parser::parseExpression(PrecedenceType currPrecedence) {
    AstNodePtr left;

    switch (m_currentToken.tkType) {
        case tk_integer: {
            left = parseInteger();
            break;
        }

        case tk_decimal: {
            left = parseDecimal();
            break;
        }

        case tk_none: {
            left = parseNone();
            break;
        }
        case tk_format: {
            advance(); // making it a string
            left = parseString(true, false);
            break;
        }
        case tk_raw: {
            advance(); // making it a string
            left = parseString(false, true);
            break;
        }
        case tk_string: {
            left = parseString(false, false);
            break;
        }

        case tk_true:
        case tk_false: {
            left = parseBool();
            break;
        }

        case tk_identifier: {
            left = parseIdentifier();
            break;
        }

        case tk_l_paren: {
            left = parseGroupedExpr();
            break;
        }

        case tk_list_open: {
            left = parseList();
            break;
        }

        case tk_dict_open: {
            left = parseDict();
            break;
        }

        case tk_negative:
        case tk_not:
        case tk_bit_not: {
            left = parsePrefixExpression();
            break;
        }

        default: {
            error(m_currentToken,
                  m_currentToken.keyword + " is not an expression");
            break;
        }
    }

    while (nextPrecedence() > currPrecedence) {
        advance();

        switch (m_currentToken.tkType) {
            case tk_l_paren: {
                left = parseFunctionCall(left);
                break;
            }

            case tk_list_open: {
                left = parseListOrDictAccess(left);
                break;
            }

            case tk_dot: {
                left = parseDotExpression(left);
                break;
            }

            default: {
                left = parseBinaryOperation(left);
                break;
            }
        }
    }

    advanceOnNewLine();

    return left;
}

AstNodePtr Parser::parseBinaryOperation(AstNodePtr left) {
    Token op = m_currentToken;
    PrecedenceType precedence = precedenceMap[m_currentToken.tkType];

    advance();
    AstNodePtr right = parseExpression(precedence);
    return std::make_shared<BinaryOperation>(op, left, op, right);
}

AstNodePtr Parser::parseFunctionCall(AstNodePtr left) {
    Token tok = m_currentToken;
    std::vector<AstNodePtr> arguments;

    if (next().tkType != tk_r_paren) {
        do {
            advance();

            arguments.push_back(parseExpression());

            advance();
        } while (m_currentToken.tkType == tk_comma);
    } else {
        advance();
    }

    if (m_currentToken.tkType != tk_r_paren) {
        error(m_currentToken,
              "expected ), got " + m_currentToken.keyword + " instead");
    }

    advanceOnNewLine();

    return std::make_shared<FunctionCall>(tok, left, arguments);
}

AstNodePtr Parser::parseListOrDictAccess(AstNodePtr left) {
    Token tok = m_currentToken;
    advance();

    AstNodePtr keyOrIndex = parseExpression();

    expect(tk_list_close);

    AstNodePtr node = std::make_shared<ListOrDictAccess>(tok, left, keyOrIndex);

    if (next().tkType != tk_assign)
        return node;

    // parsing variable statements in 2 different places, is this really ideal?

    advance();
    advance();

    AstNodePtr newValue = parseExpression();
    advanceOnNewLine();

    return std::make_shared<VariableStatement>(tok, std::make_shared<NoLiteral>(), node, newValue);
}

AstNodePtr Parser::parseDotExpression(AstNodePtr left) {
    Token tok = m_currentToken;
    advance();

    //TODO: validate output of parseExpression
    AstNodePtr referenced = parseExpression();

    return std::make_shared<DotExpression>(tok, left, referenced);
}

AstNodePtr Parser::parsePrefixExpression() {
    Token prefix = m_currentToken;
    PrecedenceType precedence = precedenceMap[m_currentToken.tkType];

    advance();

    AstNodePtr right = parseExpression(precedence);

    return std::make_shared<PrefixExpression>(prefix, prefix, right);
}

AstNodePtr Parser::parseGroupedExpr() {
    advance();

    AstNodePtr expr = parseExpression();

    expect(tk_r_paren);

    return expr;
}

AstNodePtr Parser::parseIdentifier() {
    return std::make_shared<IdentifierExpression>(m_currentToken, m_currentToken.keyword);
}

AstNodePtr Parser::parseType() {
    return std::make_shared<TypeExpression>(m_currentToken, m_currentToken.keyword);
}

AstNodePtr Parser::parseName() {
    if (m_currentToken.tkType != tk_identifier) {
        error(m_currentToken, "expected an identifier, got " + std::to_string(m_currentToken.tkType) + " instead");
    }

    return parseIdentifier();
}

AstNodePtr Parser::parseMatch() {
    Token tok = m_currentToken;
    //TODO: implement errors
    advance();
    std::vector<AstNodePtr> to_match;
    while (m_currentToken.tkType!=tk_colon){
        to_match.push_back(parseExpression());
        advance();
        if (m_currentToken.tkType!=tk_colon){
            advance();
        }
    }
    expect(tk_ident);
    std::vector<std::pair<std::vector<AstNodePtr>, AstNodePtr>> cases;
    while (next().tkType == tk_case) {
        advance();
        advance();
        std::vector<AstNodePtr> cases_arg;
        while (m_currentToken.tkType!=tk_colon){
            if (m_currentToken.tkType==tk_underscore){
                cases_arg.push_back(std::make_shared<NoLiteral>());
            }
            else{
                cases_arg.push_back(parseExpression());
            }
            advance();
            if (m_currentToken.tkType!=tk_colon){
                advance();
            }
        }
        expect(tk_ident);
        AstNodePtr body = parseBlockStatement();
        cases.push_back(std::pair<std::vector<AstNodePtr>, AstNodePtr>(cases_arg,body));
    }
    AstNodePtr default_body = std::make_shared<NoLiteral>();

    if (next().tkType == tk_default) {
        advance();
        expect(tk_colon);
        expect(tk_ident);

        default_body = parseBlockStatement();
    }
    expect(tk_dedent);
    return std::make_shared<MatchStatement>(tok, to_match,cases,default_body);
}

AstNodePtr Parser::parseDecoratorCall(){
    auto tok=m_currentToken;
    std::vector<AstNodePtr> decorators;
    AstNodePtr body;
    while (m_currentToken.tkType==tk_at){
        expect(tk_identifier);
        parseExpression(pr_lowest);
        advance();
    }
    if(m_currentToken.tkType==tk_def){
        body=parseFunctionDef();
    }
    return std::make_shared<DecoratorStatement>(tok, decorators,body);
}