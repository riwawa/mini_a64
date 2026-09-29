# Parser em C

## 1. O que é um parser

O **parser** recebe os tokens produzidos pelo lexer e verifica se eles formam uma estrutura válida da linguagem.

O lexer responde:

> “que pedaço é esse?”

O parser responde:

> “esses pedaços juntos formam o quê?”

No seu assembler:

```text
código assembly
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

Exemplo:

```asm
ADD X1, X0, #5
```

O lexer entrega:

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

O parser interpreta isso como:

```text
ADD immediate
```

e pode gerar:

```c
Instruction {
    opcode = OP_ADD_IMM,
    rd = 1,
    rn = 0,
    immediate = 5
}
```

---

# 2. O parser trabalha com tokens, não com caracteres

Isso é a grande diferença.

O lexer trabalhava com:

```text
'A'
'D'
'D'
' '
'X'
'1'
','
...
```

O parser trabalha com:

```text
TOKEN_IDENTIFIER
TOKEN_REGISTER
TOKEN_COMMA
TOKEN_REGISTER
TOKEN_COMMA
TOKEN_HASH
TOKEN_NUMBER
```

Então ele não precisa mais pensar:

> “será que `X` seguido de `1` é registrador?”

O lexer já resolveu isso.

O parser pode pensar em algo de nível mais alto:

> “depois de ADD deve vir um registrador de destino.”

---

# 3. O que é sintaxe

A sintaxe define quais sequências de tokens são válidas.

Por exemplo:

```asm
ADD X1, X0, #5
```

é válida.

Mas:

```asm
ADD #5, X0, X1
```

não corresponde à forma `ADD immediate` que você quer suportar.

Podemos descrever a sintaxe de forma informal:

```text
ADD immediate:

ADD REGISTER , REGISTER , # NUMBER
```

Ou:

```text
mnemonic Rd , Rn , # immediate
```

Isso já é praticamente uma pequena **gramática**.

---

# 4. Gramática

Uma gramática descreve como construções válidas são formadas.

Para o seu assembler, você pode pensar em regras como:

```text
instruction:
    add_instruction
    sub_instruction
    mov_instruction
    load_instruction
    branch_instruction
```

E:

```text
add_instruction:
    "ADD" register "," register "," "#" number
```

Ou:

```text
ADD X1, X0, #5
```

segue:

```text
ADD
↓
register
↓
comma
↓
register
↓
comma
↓
hash
↓
number
```

---

# 5. Lexer vs parser

Uma distinção importante:

```text
ADD
```

no lexer pode ser apenas:

```text
TOKEN_IDENTIFIER("ADD")
```

O parser é quem percebe:

```text
"ADD"
→ mnemonic
```

Agora considere:

```asm
loop:
```

O lexer gera:

```text
IDENTIFIER("loop")
COLON
```

O parser percebe:

```text
IDENTIFIER + COLON
→ definição de label
```

Ou seja:

```text
lexer:
reconhece categorias

parser:
reconhece estruturas
```

---

# 6. O parser também precisa de estado

Assim como o lexer tinha:

```c
typedef struct
{
    const char *source;
    size_t pos;
    size_t line;
} Lexer;
```

o parser pode ter algo como:

```c
typedef struct
{
    Lexer *lexer;

    Token current;
    Token previous;

    int had_error;

} Parser;
```

A ideia:

```text
Parser
┌─────────────────────────────┐
│ lexer ───────→ Lexer        │
│ current = token atual       │
│ previous = token anterior   │
│ had_error = houve erro?     │
└─────────────────────────────┘
```

O parser não anda diretamente pela string.

Ele pede tokens ao lexer.

---

# 7. Por que `Lexer *lexer` dentro do parser?

Porque o parser precisa chamar:

```c
lexer_next(parser->lexer);
```

Então temos uma cadeia:

```text
Parser
  │
  ▼
Lexer
  │
  ▼
source
```

Visualmente:

```text
parser
  │
  ├── current token
  │
  └── lexer ───────┐
                   ▼
                  Lexer
                   │
                   └── source
```

O parser controla o lexer.

---

# 8. `current` e `previous`

Imagine:

```asm
ADD X1, X0, #5
```

O parser vai consumindo:

```text
current = ADD
```

Depois avança:

```text
previous = ADD
current = X1
```

Depois:

```text
previous = X1
current = ,
```

E assim por diante.

Isso é muito útil porque às vezes você quer saber:

> qual token estou olhando agora?

e:

> qual token acabei de consumir?

---

# 9. A função `advance()`

No parser você provavelmente terá outra função chamada `advance`.

Ela é diferente do `advance()` do lexer.

No lexer:

```text
advance()
→ avança um caractere
```

No parser:

```text
advance()
→ avança um token
```

Algo assim:

```c
static void parser_advance(Parser *parser)
{
    parser->previous = parser->current;

    parser->current =
        lexer_next(parser->lexer);
}
```

Visualmente:

```text
antes:

previous = X0
current  = COMMA
```

chama:

```c
parser_advance(parser);
```

depois:

```text
previous = COMMA
current  = HASH
```

---

# 10. `check()`

Uma função muito útil:

```c
static int check(
    Parser *parser,
    TokenType type)
{
    return parser->current.type == type;
}
```

Ela pergunta:

> o token atual é desse tipo?

Exemplo:

```c
if (check(parser, TOKEN_REGISTER))
{
    ...
}
```

Tradução:

> estou olhando para um registrador?

Ela não consome nada.

É equivalente ao `peek()` conceitualmente.

---

# 11. `match()`

Outra função comum:

```c
static int match(
    Parser *parser,
    TokenType type)
{
    if (!check(parser, type))
        return 0;

    parser_advance(parser);

    return 1;
}
```

Ela pergunta:

> o token atual é desse tipo?

Se for:

> consome.

Exemplo:

```c
if (match(parser, TOKEN_NEWLINE))
{
    ...
}
```

---

# 12. `expect()` ou `consume()`

Essa é uma das funções mais importantes.

```c
static Token consume(
    Parser *parser,
    TokenType expected,
    const char *message)
{
    if (parser->current.type != expected)
    {
        parser_error(
            parser,
            message
        );
    }

    Token token =
        parser->current;

    parser_advance(parser);

    return token;
}
```

Ela significa:

> eu EXIJO que o próximo token seja desse tipo.

Exemplo:

```c
Token rd =
    consume(
        parser,
        TOKEN_REGISTER,
        "expected destination register"
    );
```

Se vier:

```text
REGISTER(1)
```

beleza.

Se vier:

```text
NUMBER(5)
```

erro.

---

# 13. Por que `consume()` é tão importante

Porque o parser é basicamente uma sequência de expectativas.

Para:

```asm
ADD X1, X0, #5
```

você poderia fazer:

```c
consume(parser, TOKEN_REGISTER, ...);
consume(parser, TOKEN_COMMA, ...);
consume(parser, TOKEN_REGISTER, ...);
consume(parser, TOKEN_COMMA, ...);
consume(parser, TOKEN_HASH, ...);
consume(parser, TOKEN_NUMBER, ...);
```

Isso representa diretamente a sintaxe:

```text
REGISTER
COMMA
REGISTER
COMMA
HASH
NUMBER
```

---

# 14. Estrutura `Instruction`

Exemplo:

```c
typedef struct
{
    enum opcode opcode;

    uint32_t rd;
    uint32_t rn;
    uint32_t rm;

    uint64_t immediate;

    uint32_t shift_type;
    uint32_t shift_amount;

} Instruction;
```

Essa struct não representa texto.

Ela representa uma instrução já entendida semanticamente.

---

# 15. Exemplo completo: parseando `ADD immediate`

Imagine que o parser já reconheceu `"ADD"`.

Agora precisamos verificar:

```asm
ADD X1, X0, #5
```

Uma função poderia ser:

```c
static Instruction parse_add(
    Parser *parser)
{
    Instruction instr = {0};

    instr.opcode =
        OP_ADD_IMM;

    Token rd =
        consume(
            parser,
            TOKEN_REGISTER,
            "expected destination register"
        );

    consume(
        parser,
        TOKEN_COMMA,
        "expected ',' after Rd"
    );

    Token rn =
        consume(
            parser,
            TOKEN_REGISTER,
            "expected source register"
        );

    consume(
        parser,
        TOKEN_COMMA,
        "expected ',' after Rn"
    );

    consume(
        parser,
        TOKEN_HASH,
        "expected '#'"
    );

    Token imm =
        consume(
            parser,
            TOKEN_NUMBER,
            "expected immediate value"
        );

    instr.rd =
        rd.number;

    instr.rn =
        rn.number;

    instr.immediate =
        imm.number;

    return instr;
}
```

---

# 16. Executando mentalmente `parse_add()`

Entrada:

```asm
ADD X1, X0, #5
```

Suponha que `"ADD"` já foi consumido.

Agora:

```text
current
  ↓
X1 , X0 , #5
```

Primeiro:

```c
consume(TOKEN_REGISTER)
```

pega:

```text
X1
```

e agora:

```text
current
 ↓
 , X0 , #5
```

Depois:

```c
consume(TOKEN_COMMA)
```

consome:

```text
,
```

Depois:

```c
consume(TOKEN_REGISTER)
```

pega:

```text
X0
```

Depois:

```text
,
#
5
```

No final:

```c
rd.number = 1;
rn.number = 0;
imm.number = 5;
```

Então:

```c
instr.rd = 1;
instr.rn = 0;
instr.immediate = 5;
```

---

# 17. Mas como sabemos que é ADD?

Você precisa olhar o texto do identifier.

O token tem:

```c
const char *start;
size_t length;
```

Então pode criar:

```c
static int token_equals(
    Token token,
    const char *text)
{
    size_t len =
        strlen(text);

    if (token.length != len)
        return 0;

    return memcmp(
        token.start,
        text,
        len
    ) == 0;
}
```

Exemplo:

```c
token_equals(token, "ADD")
```

retorna verdadeiro se o token for exatamente `"ADD"`.

---

# 18. Parser principal de instrução

Algo conceitualmente assim:

```c
static Instruction parse_instruction(
    Parser *parser)
{
    Token mnemonic =
        consume(
            parser,
            TOKEN_IDENTIFIER,
            "expected instruction"
        );

    if (token_equals(
            mnemonic,
            "ADD"))
    {
        return parse_add(parser);
    }

    if (token_equals(
            mnemonic,
            "SUB"))
    {
        return parse_sub(parser);
    }

    if (token_equals(
            mnemonic,
            "MOVZ"))
    {
        return parse_movz(parser);
    }

    parser_error(
        parser,
        "unknown instruction"
    );

    Instruction invalid = {0};

    invalid.opcode =
        OP_UNKNOWN;

    return invalid;
}
```

---

# 19. O parser começa a dar significado

Observe a transformação.

Lexer:

```text
IDENTIFIER("ADD")
REGISTER(1)
COMMA
REGISTER(0)
COMMA
HASH
NUMBER(5)
```

Parser:

```text
isso é:

ADD immediate
```

Resultado:

```text
opcode = OP_ADD_IMM
Rd = 1
Rn = 0
imm = 5
```

O parser elimina muita informação textual desnecessária.

Por exemplo, vírgulas:

```text
,
```

são importantes para validar sintaxe.

Mas depois que validamos:

```asm
ADD X1, X0, #5
```

não precisamos guardar as vírgulas.

Então:

```text
texto
↓
tokens
↓
estrutura semântica
```

---

# 20. Parser não gera machine code ainda

Esse ponto é importante.

O parser deveria gerar algo como:

```c
Instruction instr;
```

Não:

```c
uint32_t machine_code;
```

Separando:

```text
Parser
↓
Instruction

Encoder
↓
uint32_t
```

Isso deixa o código muito mais fácil de entender e testar.

---

# 21. O encoder vem depois

Depois de:

```c
Instruction instr;
```

com:

```text
opcode = OP_ADD_IMM
rd = 1
rn = 0
imm = 5
```

o encoder pega isso e monta os bits.

Exemplo conceitual:

```c
uint32_t encode_add_imm(
    Instruction *instr)
{
    uint32_t result =
        0x91000000;

    result |=
        (instr->immediate & 0xFFF)
        << 10;

    result |=
        (instr->rn & 0x1F)
        << 5;

    result |=
        instr->rd & 0x1F;

    return result;
}
```

Agora aparece o inverso do seu decoder.

CPU:

```c
Rn =
    (instr >> 5)
    & 0x1F;
```

Assembler:

```c
instr |=
    (Rn & 0x1F)
    << 5;
```

---

# 22. O parser pode precisar decidir entre variantes

Aqui fica interessante.

Você tem:

```asm
ADD X1, X0, #5
```

e:

```asm
ADD X1, X0, X2
```

Ambas começam com:

```text
ADD REGISTER , REGISTER ,
```

A diferença aparece depois.

Primeiro caso:

```text
HASH
```

Segundo:

```text
REGISTER
```

Então o parser pode olhar:

```c
if (check(parser, TOKEN_HASH))
{
    // ADD immediate
}
else if (check(parser, TOKEN_REGISTER))
{
    // ADD register
}
```

Isso é **lookahead no parser**.

No lexer, lookahead era:

```text
olhar próximo caractere
```

No parser:

```text
olhar próximo token
```

---

# 23. Exemplo de decisão

Para:

```asm
ADD X1, X0, #5
```

depois de consumir:

```text
ADD X1, X0,
```

o token atual é:

```text
HASH
```

Então:

```text
→ OP_ADD_IMM
```

Para:

```asm
ADD X1, X0, X2
```

o token atual é:

```text
REGISTER
```

Então:

```text
→ OP_ADD_SHIFT
```

ou uma variante register que você definir.

---

# 24. Labels

Considere:

```asm
loop:
```

Lexer:

```text
IDENTIFIER("loop")
COLON
```

O parser pode detectar:

```text
identifier + colon
```

e criar algo como:

```c
Label {
    name = "loop"
}
```

Ou inserir na symbol table.

Depois:

```asm
B loop
```

produz:

```text
IDENTIFIER("B")
IDENTIFIER("loop")
```

O parser entende:

```text
branch para label "loop"
```

A resolução do endereço normalmente acontece depois.

---

# 25. Por que assembler costuma usar dois passes

Labels criam um problema.

Exemplo:

```asm
B end

ADD X0, X0, #1
ADD X1, X1, #1

end:
RET
```

Quando o assembler encontra:

```asm
B end
```

talvez ainda não saiba onde `end` está.

Então uma estratégia comum é:

```text
PASS 1
↓
descobrir labels e endereços

PASS 2
↓
gerar machine code
```

No primeiro pass:

```text
end = endereço 0x100C
```

No segundo:

```text
B end
```

pode calcular o offset.

---

# 26. Erros sintáticos

O parser é responsável por mensagens como:

```asm
ADD X1 X0 #5
```

faltam vírgulas.

Lexer provavelmente não vê problema:

```text
ADD
X1
X0
#
5
```

Todos são tokens válidos.

Mas o parser esperava:

```text
REGISTER
COMMA
REGISTER
COMMA
HASH
NUMBER
```

e recebeu:

```text
REGISTER
REGISTER
```

Então ele gera:

```text
expected ',' after destination register
```

Esse é um erro **sintático**.

---

# 27. Erro léxico vs sintático vs semântico

É importante separar.

## Erro léxico

Caractere que não pertence à linguagem:

```asm
ADD X1, @X0
```

O lexer encontra:

```text
@
```

e retorna:

```text
TOKEN_INVALID
```

---

## Erro sintático

Tokens válidos em ordem errada:

```asm
ADD X1 X0 #5
```

O parser reclama:

```text
expected ','
```

---

## Erro semântico

Sintaxe correta, mas valor inválido:

```asm
ADD X99, X0, #5
```

Lexicamente:

```text
X99
```

é um registrador.

Sintaticamente:

```text
ADD REGISTER , REGISTER , # NUMBER
```

está correto.

Mas arquiteturalmente:

```text
X99
```

não existe.

Então isso é validação semântica:

```text
register must be X0-X30
```

---

# 28. Estrutura mental do parser

Pense nele como alguém com uma lista de regras.

Entrada:

```text
ADD
X1
,
X0
,
#
5
```

Ele pensa:

```text
ADD?
sim

agora espero REGISTER
→ X1
ok

agora espero COMMA
→ ,
ok

agora espero REGISTER
→ X0
ok

agora espero COMMA
→ ,
ok

agora espero HASH
→ #
ok

agora espero NUMBER
→ 5
ok
```

Então:

```text
instrução válida
```

---

# 29. Organização sugerida

Você pode organizar:

```text
assembler/
├── main.c
│
├── lexer.h
├── lexer.c
│
├── parser.h
├── parser.c
│
├── instruction.h
│
├── encoder.h
└── encoder.c
```

---

# 30. `parser.h`

Uma versão inicial poderia ser:

```c
#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "instruction.h"

typedef struct
{
    Lexer *lexer;

    Token current;
    Token previous;

    int had_error;

} Parser;

void parser_init(
    Parser *parser,
    Lexer *lexer
);

Instruction parser_next_instruction(
    Parser *parser
);

#endif
```

---

# 31. `instruction.h`

```c
#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include <stdint.h>

typedef enum
{
    OP_ADD_IMM,
    OP_ADD_SHIFT,

    OP_SUB_IMM,

    OP_MOVZ,
    OP_MOVN,
    OP_MOVK,

    OP_LDR,
    OP_STR,

    OP_B,
    OP_BL,
    OP_RET,

    OP_UNKNOWN

} Opcode;

typedef struct
{
    Opcode opcode;

    uint32_t rd;
    uint32_t rn;
    uint32_t rm;

    uint64_t immediate;

    uint32_t shift_type;
    uint32_t shift_amount;

} Instruction;

#endif
```

Isso ainda vai evoluir conforme seu assembler crescer.

---

# 32. Fluxo completo real

Para:

```asm
ADD X1, X0, #5
```

temos:

```text
SOURCE
"ADD X1, X0, #5"

        ↓

LEXER

        ↓

IDENTIFIER("ADD")
REGISTER(1)
COMMA
REGISTER(0)
COMMA
HASH
NUMBER(5)

        ↓

PARSER

        ↓

Instruction
{
    opcode = OP_ADD_IMM,
    rd = 1,
    rn = 0,
    immediate = 5
}

        ↓

ENCODER

        ↓

0x91001401

        ↓

program.bin
```

Depois sua CPU faz o contrário:

```text
0x91001401

↓ fetch

32 bits

↓ decode

OP_ADD_IMM

↓ extrai campos

Rd = 1
Rn = 0
imm = 5

↓ execute

X1 = X0 + 5
```

Isso fecha o ciclo:

```text
ASSEMBLER
texto
→ significado
→ bits

CPU
bits
→ significado
→ execução
```

---

# 33. O que você precisa realmente dominar antes de implementar

Não precisa decorar teoria formal de compiladores agora.

Você precisa conseguir explicar:

```text
1. Lexer transforma caracteres em tokens.

2. Parser transforma tokens em estruturas.

3. current é o token que estou olhando.

4. parser_advance() pede o próximo token ao lexer.

5. check() olha sem consumir.

6. match() olha e consome se bater.

7. consume() exige um token específico.

8. O parser reconhece padrões como:

   REGISTER , REGISTER , # NUMBER

9. O resultado do parser é uma Instruction.

10. O encoder transforma Instruction em bits.
```

Se isso estiver claro, já dá para começar seu `parser.h` e `parser.c`.

---

# 34. Modelo mental final

```text
                   TEXTO
                     │
                     ▼
               ┌──────────┐
               │  LEXER   │
               └────┬─────┘
                    │
                    │ tokens
                    ▼
               ┌──────────┐
               │  PARSER  │
               └────┬─────┘
                    │
                    │ Instruction
                    ▼
               ┌──────────┐
               │ ENCODER  │
               └────┬─────┘
                    │
                    │ 32 bits
                    ▼
                MACHINE CODE
```

E do outro lado:

```text
MACHINE CODE
     │
     ▼
Mini-A64 CPU
     │
     ├── fetch
     ├── decode
     └── execute
```

O assembler que você está fazendo é essencialmente o **caminho inverso do decoder que você acabou de construir**.
