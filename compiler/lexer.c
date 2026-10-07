#include "lexer.h"

static char peek(Lexer *lexer){
    return lexer->source[lexer->pos];
}

static char advance(Lexer *lexer){
    return lexer->source[lexer->pos++];
}

static int is_at_end(Lexer *lexer){
    return peek(lexer) == '\0';
}

static int is_digit(char c){
    return c >= '0' && c <= '9';
}

static int is_identifier_start(char c){
    return (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') ||
           c == '_';
}

static int is_identifier_char(char c){
    return is_identifier_start(c) || is_digit(c);
}

static OperationType get_operation_type(char c){
    switch(c){
        case '+': return OP_ADD;
        case '-': return OP_SUB;
        case '*': return OP_MUL;
        case '/': return OP_DIV;
        default: return OP_UNKNOWN;
    }
}

static Token make_token(TokenType type, const char *start, size_t length){
    Token token;
    token.type = type;
    token.start = start;
    token.length = length;
    token.operation = OP_UNKNOWN;
    return token;
}

static Token make_operator_token(OperationType operation, const char *start, size_t length){
    Token token = make_token(TOKEN_OPERATOR, start, length);
    token.operation = operation;
    return token;
}

static int token_matches(const char *start, size_t length, const char *word){
    size_t i = 0;
    while(i < length && word[i] != '\0'){
        if(start[i] != word[i]) return 0;
        i++;
    }
    return i == length && word[i] == '\0';
}

static Token read_number(Lexer *lexer, size_t start){
    while(is_digit(peek(lexer))) advance(lexer);
    return make_token(TOKEN_NUMBER, &lexer->source[start], lexer->pos-start);
}

static Token read_identifier(Lexer *lexer, size_t start){
    while(is_identifier_char(peek(lexer))) advance(lexer);

    size_t length = lexer->pos-start;
    const char *text = &lexer->source[start];

    if(token_matches(text, length, "let"))
        return make_token(TOKEN_LET, text, length);

    return make_token(TOKEN_IDENTIFIER, text, length);
}

void lexer_init(Lexer *lexer, const char *source){
    lexer->source = source;
    lexer->pos = 0;
    lexer->line = 1;
}

Token lexer_next(Lexer *lexer){
    while(!is_at_end(lexer)){
        size_t start = lexer->pos;
        char c = advance(lexer);

        if(c == ' ' || c == '\t' || c == '\r') continue;

        if(c == '\n'){
            lexer->line++;
            return make_token(TOKEN_NEWLINE, &lexer->source[start], 1);
        }

        if(is_digit(c)){
            lexer->pos = start;
            return read_number(lexer, start);
        }

        if(is_identifier_start(c)){
            lexer->pos = start;
            return read_identifier(lexer, start);
        }

        if(c == '=')
            return make_token(TOKEN_ASSIGN, &lexer->source[start], 1);

        if(c == '(')
            return make_token(TOKEN_LPAREN, &lexer->source[start], 1);

        if(c == ')')
            return make_token(TOKEN_RPAREN, &lexer->source[start], 1);

        OperationType operation = get_operation_type(c);

        if(operation != OP_UNKNOWN)
            return make_operator_token(operation, &lexer->source[start], 1);

        return make_token(TOKEN_INVALID, &lexer->source[start], 1);
    }

    return make_token(TOKEN_EOF, &lexer->source[lexer->pos], 0);
}
