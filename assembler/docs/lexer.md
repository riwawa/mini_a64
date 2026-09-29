# Lexer em C 

## 1. O que é um lexer

Um **lexer** é a primeira etapa de um assembler, compilador ou interpretador.

Ele recebe **texto bruto** e transforma esse texto em unidades com significado chamadas **tokens**.

Exemplo:

```asm
ADD X1, X0, #5
```

O lexer não quer trabalhar caractere por caractere:

```text
'A' 'D' 'D' ' ' 'X' '1' ',' ...
```

Ele transforma isso em:

```text
IDENTIFIER("ADD")
REGISTER(1)
COMMA
REGISTER(0)
COMMA
HASH
NUMBER(5)
NEWLINE
```

Ou seja:

```text
texto
↓
lexer
↓
tokens
```

---

## 2. Lexer não entende a instrução inteira

O lexer só identifica **pedaços**.

Ele não precisa saber ainda que:

```asm
ADD X1, X0, #5
```

é um `ADD immediate`.

Esse trabalho pertence ao **parser**.

Fluxo:

```text
ADD X1, X0, #5
        ↓
      Lexer
        ↓
IDENTIFIER
REGISTER
COMMA
REGISTER
COMMA
HASH
NUMBER
        ↓
      Parser
        ↓
OP_ADD_IMM
Rd = 1
Rn = 0
imm = 5
```

A divisão de responsabilidade é:

```text
Lexer:
"o que é esse pedaço?"

Parser:
"esses pedaços juntos formam o quê?"
```

---

# 3. Tokens

Em C, podemos representar os tipos de token com um `enum`.

```c
typedef enum
{
    TOKEN_EOF,
    TOKEN_INVALID,

    TOKEN_IDENTIFIER,
    TOKEN_REGISTER,
    TOKEN_NUMBER,

    TOKEN_COMMA,
    TOKEN_HASH,
    TOKEN_COLON,

    TOKEN_LBRACKET,
    TOKEN_RBRACKET,

    TOKEN_NEWLINE

} TokenType;
```

Exemplos:

```text
ADD     → TOKEN_IDENTIFIER
X5      → TOKEN_REGISTER
123     → TOKEN_NUMBER
,       → TOKEN_COMMA
#       → TOKEN_HASH
:       → TOKEN_COLON
[       → TOKEN_LBRACKET
]       → TOKEN_RBRACKET
```

`TOKEN_EOF` significa:

```text
End Of File
```

ou seja, acabou o código-fonte.

---

# 4. Estrutura de um token

Um token precisa guardar informações sobre aquilo que foi encontrado.

```c
typedef struct
{
    TokenType type;

    const char *start;
    size_t length;

    uint64_t number;

} Token;
```

## `type`

Diz qual é o tipo do token.

```c
token.type = TOKEN_REGISTER;
```

---

## `start`

É um ponteiro para o início do texto daquele token.

Se o source for:

```text
ADD X1, X0
```

o token `ADD` aponta para:

```text
ADD X1, X0
^
start
```

---

## `length`

Diz quantos caracteres fazem parte daquele token.

Para:

```text
ADD
```

temos:

```c
length = 3;
```

Logo:

```text
start + length
```

permite saber exatamente qual trecho do source pertence ao token.

---

## `number`

É usado quando o token possui um valor numérico.

Exemplo:

```asm
X17
```

pode gerar:

```text
type   = TOKEN_REGISTER
number = 17
```

E:

```asm
#125
```

gera:

```text
TOKEN_HASH
TOKEN_NUMBER(number = 125)
```

---

# 5. Estado do lexer

O lexer precisa saber:

* qual código está lendo;
* em qual posição está;
* em qual linha está.

```c
typedef struct
{
    const char *source;

    size_t pos;
    size_t line;

} Lexer;
```

Visualmente:

```text
source:
ADD X1, X0, #5
    ^
    pos
```

A string fica parada.

Quem se move é:

```c
lexer->pos
```

---

# 6. Por que usamos `Lexer *lexer`

As funções recebem:

```c
Lexer *lexer
```

porque queremos modificar o **mesmo lexer existente na memória**.

Exemplo:

```c
lexer->pos++;
```

altera a posição real do lexer.

Se recebêssemos:

```c
Lexer lexer
```

seria criada uma cópia da struct.

Então:

```c
lexer.pos++;
```

alteraria apenas a cópia.

Regra mental:

```text
Lexer lexer
→ objeto

Lexer *lexer
→ ponteiro para o objeto

lexer.pos
→ campo de uma struct normal

lexer->pos
→ campo de uma struct acessada por ponteiro
```

E:

```c
lexer->pos
```

é equivalente a:

```c
(*lexer).pos
```

---

# 7. `peek()`

```c
static char peek(Lexer *lexer)
{
    return lexer->source[lexer->pos];
}
```

`peek()` olha o caractere atual sem avançar.

Exemplo:

```text
ADD X1
    ^
```

```c
peek(lexer)
```

retorna:

```text
'X'
```

Mas `pos` continua no mesmo lugar.

---

# 8. `advance()`

```c
static char advance(Lexer *lexer)
{
    char c = lexer->source[lexer->pos];

    lexer->pos++;

    return c;
}
```

Faz duas coisas:

```text
1. pega o caractere atual
2. avança a posição
```

Exemplo:

```text
ADD
^
```

Depois de:

```c
char c = advance(lexer);
```

temos:

```text
c = 'A'

ADD
 ^
```

---

# 9. Detectando o fim

Strings em C terminam em:

```c
'\0'
```

Então:

```c
static int is_at_end(Lexer *lexer)
{
    return lexer->source[lexer->pos] == '\0';
}
```

pergunta:

> chegamos ao final da string?

---

# 10. Criando tokens

Para evitar repetir código, usamos:

```c
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
```

Exemplo:

```c
make_token(
    TOKEN_COMMA,
    &lexer->source[start],
    1
);
```

produz um token que representa:

```text
,
```

---

# 11. Identificadores

Identificador é basicamente uma palavra.

Exemplos:

```text
ADD
SUB
MOVZ
loop
start
B.NE
```

Primeiro verificamos se um caractere pode começar um identificador:

```c
static int is_identifier_start(char c)
{
    return
        (c >= 'A' && c <= 'Z') ||
        (c >= 'a' && c <= 'z') ||
        c == '_';
}
```

Depois verificamos se um caractere pode continuar o identificador:

```c
static int is_identifier_char(char c)
{
    return
        is_identifier_start(c) ||
        (c >= '0' && c <= '9') ||
        c == '.';
}
```

Isso permite:

```text
ADD
label1
loop2
B.NE
```

---

# 12. Lendo um identificador

```c
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
```

Imagine:

```text
ADD X1
```

O `A` já foi consumido.

Então:

```text
A D D   X 1
  ^
```

O loop continua enquanto houver caracteres válidos:

```text
D → válido
D → válido
' ' → inválido
```

Então:

```text
start = 0
pos = 3
```

Logo:

```c
lexer->pos - start
```

é:

```text
3
```

E o token representa:

```text
ADD
```

---

# 13. Números

Para reconhecer dígitos:

```c
static int is_digit(char c)
{
    return c >= '0' && c <= '9';
}
```

Para converter texto decimal em número:

```c
static Token read_number(
    Lexer *lexer,
    size_t start)
{
    uint64_t value = 0;

    while (is_digit(peek(lexer)))
    {
        value =
            value * 10 +
            (peek(lexer) - '0');

        advance(lexer);
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
```

Para:

```text
123
```

o cálculo é:

```text
0 * 10 + 1 = 1

1 * 10 + 2 = 12

12 * 10 + 3 = 123
```

---

# 14. Registradores

Um registrador tem o formato:

```text
X + número
```

Exemplo:

```asm
X17
```

Quando o lexer já consumiu o `X`, ele lê os números seguintes:

```c
static Token read_register(
    Lexer *lexer,
    size_t start)
{
    uint64_t value = 0;

    while (is_digit(peek(lexer)))
    {
        value =
            value * 10 +
            (peek(lexer) - '0');

        advance(lexer);
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
```

Resultado:

```text
X17
↓
TOKEN_REGISTER
number = 17
```

---

# 15. Lookahead

Às vezes não é possível decidir usando apenas o caractere atual.

Exemplo:

```text
X12
```

Quando vemos:

```text
X
```

isso pode ser:

```text
X12        → registrador
XLabel     → identificador
```

Então olhamos o próximo caractere:

```c
if ((c == 'X' || c == 'x') &&
    is_digit(peek(lexer)))
```

Se o próximo for número:

```text
X12
 ↑
```

é registrador.

Se for letra:

```text
XLabel
 ↑
```

é identificador.

Isso é chamado de **lookahead**.

---

# 16. `lexer_next()`

Essa é a função principal.

```c
Token lexer_next(Lexer *lexer)
```

Ela retorna:

```text
um token por chamada
```

Não todos de uma vez.

Exemplo:

```c
lexer_next(&lexer);
```

pode retornar:

```text
IDENTIFIER("ADD")
```

A próxima chamada retorna:

```text
REGISTER(1)
```

e assim por diante.

---

## Lógica geral

```text
lexer_next()

    pega caractere

    espaço?
        ignora

    newline?
        retorna NEWLINE

    X seguido de número?
        lê REGISTER

    número?
        lê NUMBER

    letra?
        lê IDENTIFIER

    pontuação?
        retorna token correspondente

    nada reconhecido?
        INVALID

    fim?
        EOF
```

---

# 17. Fluxo completo

Para:

```asm
ADD X1, X0, #5
```

o lexer trabalha assim:

```text
A
↓
identificador
↓
ADD

X seguido de 1
↓
registrador
↓
X1

,
↓
COMMA

X0
↓
REGISTER(0)

,
↓
COMMA

#
↓
HASH

5
↓
NUMBER(5)
```

Resultado:

```text
IDENTIFIER("ADD")
REGISTER(1)
COMMA
REGISTER(0)
COMMA
HASH
NUMBER(5)
NEWLINE
EOF
```

---

# 18. Relação com o parser

O lexer apenas separa.

O parser interpreta a estrutura.

```text
Lexer:

ADD X1, X0, #5

↓ ↓ ↓ ↓ ↓ ↓ ↓

ADD
X1
,
X0
,
#
5
```

Depois:

```text
Parser:

IDENTIFIER("ADD")
REGISTER(1)
COMMA
REGISTER(0)
COMMA
HASH
NUMBER(5)

↓

Instruction

opcode = OP_ADD_IMM
rd = 1
rn = 0
imm = 5
```

---

# 19. Modelo mental

O melhor modelo para visualizar um lexer é:

```text
           código fonte
                ↓
       ┌────────────────┐
       │     Lexer      │
       │                │
       │ source         │
       │ pos   ← dedo   │
       │ line           │
       └───────┬────────┘
               ↓
            Token
```

O lexer é basicamente:

> um cursor andando sobre uma string e agrupando caracteres que pertencem à mesma categoria.

---

# 20. Resumo

Um lexer em C normalmente precisa de:

```text
1. source
   texto que será analisado

2. posição atual
   onde estamos lendo

3. peek()
   olha sem avançar

4. advance()
   lê e avança

5. funções de classificação
   is_digit()
   is_identifier_start()
   etc.

6. funções de leitura
   read_number()
   read_identifier()
   read_register()

7. lexer_next()
   decide qual tipo de token começa na posição atual

8. Token
   resultado devolvido ao parser
```

Fluxo final:

```text
caracteres
↓
lexer
↓
tokens
↓
parser
↓
Instruction
↓
encoder
↓
machine code
```


