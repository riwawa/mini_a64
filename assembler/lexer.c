#include "lexer.h"

static char peek(Lexer *lexer)
{
    return lexer->source[lexer->pos];
}

static char advance(Lexer *lexer)
{
    char c = lexer->source[lexer->pos];
    lexer->pos++;
    return c;
}

static int is_at_end(Lexer *lexer)
{
    return lexer->source[lexer->pos] == '\0';
}


static Token make_token(
    TokenType type,
    const char *start,
    size_t length)
{
    Token token;

    token.type = type;
    token.start = start;
    token.length = length;

    token.number = 0;

    return token;
}

static int is_identifier_start(char c)
{
    return
        (c >= 'A' && c <= 'Z') ||
        (c >= 'a' && c <= 'z') ||
        c == '_';
}

static int is_identifier_char(char c)
{
    return
        is_identifier_start(c) ||
        (c >= '0' && c <= '9') ||
        c == '.';
}

static Token read_identifier(
    Lexer *lexer,
    size_t start)
{
    while (
        is_identifier_char(
            peek(lexer)
        )
    )
    {
        advance(lexer);
    }

    return make_token(
        TOKEN_IDENTIFIER,
        &lexer->source[start],
        lexer->pos - start
    );
}

void lexer_init(
    Lexer *lexer,
    const char *source)
{
    lexer->source = source;

    lexer->pos = 0;
    lexer->line = 1;
}

static int is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static Token read_register(
    Lexer *lexer,
    size_t start)
{
    uint64_t value = 0;

    while (
        is_digit(
            lexer->source[lexer->pos]
        )
    )
    {
        value =
            value * 10 +
            (lexer->source[lexer->pos] - '0');

        lexer->pos++;
    }

    Token token =
        make_token(
            TOKEN_REGISTER,
            &lexer->source[start],
            lexer->pos - start
        );

    token.number = value;

    return token;
}

static Token read_number(
    Lexer *lexer,
    size_t start)
{
    uint64_t value = 0;

    while (
        is_digit(
            lexer->source[lexer->pos]
        )
    )
    {
        value =
            value * 10 +
            (lexer->source[lexer->pos] - '0');

        lexer->pos++;
    }

    Token token =
        make_token(
            TOKEN_NUMBER,
            &lexer->source[start],
            lexer->pos - start
        );

    token.number = value;

    return token;
}

Token lexer_next(Lexer *lexer)
{
    while (!is_at_end(lexer))
    {
        size_t start =
            lexer->pos;

        char c =
            advance(lexer);

        if (c == ' ' ||
            c == '\t' ||
            c == '\r')
        {
            continue;
        }

        if (c == '\n')
        {
            lexer->line++;

            return make_token(
                TOKEN_NEWLINE,
                &lexer->source[start],
                1
            );
        }


        /*
         * X123
         */
        if ((c == 'X' || c == 'x') &&
            is_digit(peek(lexer)))
        {
            return read_register(
                lexer,
                start
            );
        }


        /*
         * 123
         */
        if (is_digit(c))
        {
            lexer->pos = start;

            return read_number(
                lexer,
                start
            );
        }


        /*
         * ADD, MOVZ, loop, B.NE...
         */
        if (is_identifier_start(c))
        {
            return read_identifier(
                lexer,
                start
            );
        }


        switch (c)
        {
            case ',':
                return make_token(
                    TOKEN_COMMA,
                    &lexer->source[start],
                    1
                );

            case '#':
                return make_token(
                    TOKEN_HASH,
                    &lexer->source[start],
                    1
                );

            case ':':
                return make_token(
                    TOKEN_COLON,
                    &lexer->source[start],
                    1
                );

            case '[':
                return make_token(
                    TOKEN_LBRACKET,
                    &lexer->source[start],
                    1
                );

            case ']':
                return make_token(
                    TOKEN_RBRACKET,
                    &lexer->source[start],
                    1
                );

            default:
                return make_token(
                    TOKEN_INVALID,
                    &lexer->source[start],
                    1
                );
        }
    }

    return make_token(
        TOKEN_EOF,
        &lexer->source[lexer->pos],
        0
    );
}