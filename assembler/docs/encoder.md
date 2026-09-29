# Encoder do Mini-A64 

## 1. O papel do encoder

O **encoder** é a etapa do assembler que pega uma instrução já entendida pelo parser e transforma essa informação em uma palavra de **32 bits** que a CPU consegue executar.

Fluxo:

```text
assembly
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
uint32_t
   ↓
machine code
```

Exemplo:

```asm
ADD X1, X2, #5
```

Depois do parser, isso vira algo conceitualmente assim:

```c
Instruction {
    opcode = OP_ADD_IMM,
    rd = 1,
    rn = 2,
    immediate = 5
}
```

O encoder não precisa mais interpretar texto. Ele só precisa responder:

> Em quais bits cada campo dessa `Instruction` deve ser colocado?

---

## 2. O encoder é o inverso do decoder

Essa é a ideia mais importante.

No decoder da CPU, fazemos algo assim:

```c
uint32_t rn = (instr >> 5) & 0x1F;
```

Isso quer dizer:

1. mover os bits de `Rn` até a direita;
2. mascarar com `0x1F` para ficar somente com 5 bits;
3. recuperar o valor de `Rn`.

No encoder fazemos o contrário:

```c
word |= (rn & 0x1F) << 5;
```

Ou seja:

1. pega `Rn`;
2. limita o valor a 5 bits;
3. desloca para a posição correta;
4. junta com a palavra final usando OR.

Regra mental:

```text
DECODER:
(instr >> posição) & máscara

ENCODER:
(campo & máscara) << posição
```

---

## 3. O que significam `&`, `<<` e `|`

### `&` — limitar o tamanho do campo

Exemplo:

```c
instr->rn & 0x1F
```

`0x1F` em binário:

```text
11111
```

Como um registrador A64 usa 5 bits, isso garante que só esses 5 bits sejam usados.

### `<<` — colocar o campo na posição certa

Se `Rn` ocupa os bits `9..5`:

```c
(instr->rn & 0x1F) << 5
```

O valor é deslocado cinco posições para a esquerda.

### `|` — combinar os campos

```c
word |= (instr->rn & 0x1F) << 5;
word |= instr->rd & 0x1F;
```

Mentalmente:

```text
bits fixos
   OR
Rn
   OR
Rd
   OR
immediate
   ↓
instrução completa
```

---

## 4. Pattern fixo + campos variáveis

Uma instrução possui dois tipos de bits.

### Bits fixos

Identificam a família da instrução.

Exemplo:

```c
uint32_t word = 0x11000000;
```

Esse valor é a base da família `ADD/SUB immediate`.

### Bits variáveis

Dependem do assembly escrito:

```text
op
S
imm12
Rn
Rd
```

O encoder começa com o pattern fixo e depois encaixa os campos variáveis.

---

## 5. Estrutura típica de uma função de encoding

Quase todas seguem esta lógica:

```c
static int encode_alguma_instrucao(
    const Instruction *instr,
    uint32_t *out)
{
    uint32_t word = BASE_PATTERN;

    // validar campos

    word |= ...;
    word |= ...;
    word |= ...;

    *out = word;

    return 1;
}
```

Mentalmente:

```text
1. identificar o formato
2. validar os operandos
3. começar com os bits fixos
4. inserir os campos
5. devolver os 32 bits
```

---

## 6. ADD/SUB immediate

Exemplo:

```asm
ADD X1, X2, #5
```

Campos principais:

```text
sf
op
S
sh
imm12
Rn
Rd
```

Na mini-A64 usamos somente registradores X, portanto:

```text
sf = 1
```

Combinações:

```text
ADD  → op=0 S=0
ADDS → op=0 S=1
SUB  → op=1 S=0
SUBS → op=1 S=1
```

Encoding:

```c
word |= 1u << 31;
word |= (op & 1u) << 30;
word |= (S & 1u) << 29;

word |= ((uint32_t)instr->immediate & 0xFFF) << 10;
word |= (instr->rn & 0x1F) << 5;
word |= instr->rd & 0x1F;
```

Observe a simetria:

```text
encoder:
(Rn & 0x1F) << 5

CPU:
(instr >> 5) & 0x1F
```

---

## 7. ADD/SUB shifted register

Exemplo:

```asm
ADD X1, X2, X3, LSL #4
```

O parser já converteu isso para:

```text
rd = 1
rn = 2
rm = 3
shift_type = 0
shift_amount = 4
```

Mapeamento:

```text
LSL = 0
LSR = 1
ASR = 2
```

Encoding:

```c
word |= (instr->shift_type & 0x3) << 22;
word |= (instr->rm & 0x1F) << 16;
word |= (instr->shift_amount & 0x3F) << 10;
word |= (instr->rn & 0x1F) << 5;
word |= instr->rd & 0x1F;
```

Separação importante:

```text
parser:
texto → significado

encoder:
significado → bits
```

---

## 8. ADD/SUB extended register

Exemplo:

```asm
ADD X1, X2, X3, UXTX #2
```

O parser entrega:

```text
rm = 3
extend_type = UXTX
shift_amount = 2
```

O campo `option` possui 3 bits:

```text
000 UXTB
001 UXTH
010 UXTW
011 UXTX
100 SXTB
101 SXTH
110 SXTW
111 SXTX
```

Encoding:

```c
word |= (instr->rm & 0x1F) << 16;
word |= (instr->extend_type & 0x7) << 13;
word |= (instr->shift_amount & 0x7) << 10;
word |= (instr->rn & 0x1F) << 5;
word |= instr->rd & 0x1F;
```

---

## 9. Logical shifted

Família:

```text
AND
ANDS
ORR
EOR
```

Exemplo:

```asm
ORR X1, X2, X3, ROR #8
```

Logical shifted aceita:

```text
LSL = 0
LSR = 1
ASR = 2
ROR = 3
```

O campo `opc` diferencia a operação:

```text
AND  → 0
ORR  → 1
EOR  → 2
ANDS → 3
```

O restante segue o mesmo padrão de inserir campos com máscara, shift e OR.

---

## 10. Logical immediate é especial

Exemplo:

```asm
AND X0, X1, #255
```

Nesse formato, `255` não é colocado diretamente em um campo.

A64 usa:

```text
N
immr
imms
```

Esses campos descrevem uma máscara binária.

Então o encoder precisa descobrir quais valores de `N`, `immr` e `imms` produzem exatamente a máscara desejada.

Na implementação educacional, podemos testar todas as combinações possíveis:

```text
2 × 64 × 64 = 8192
```

Fluxo:

```text
immediate desejado
↓
testa N/immr/imms
↓
reconstrói máscara
↓
é igual?
├── sim → encontrou encoding
└── não → continua
```

Se nenhuma combinação produzir o valor, aquele immediate não é representável como logical immediate A64.

---

## 11. MOVZ / MOVN / MOVK

Exemplo:

```asm
MOVZ X0, #1234, LSL #16
```

Campos:

```text
opc
hw
imm16
Rd
```

O parser guarda:

```text
immediate = 1234
shift_amount = 16
```

Mas o encoding usa `hw`:

```text
hw = shift_amount / 16
```

Logo:

```text
LSL #0  → hw = 0
LSL #16 → hw = 1
LSL #32 → hw = 2
LSL #48 → hw = 3
```

Depois:

```c
word |= (hw & 0x3) << 21;
word |= (instr->immediate & 0xFFFF) << 5;
word |= instr->rd & 0x1F;
```

---

## 12. LDR / STR e offset escalado

Exemplo:

```asm
LDR X0, [X1, #24]
```

O parser guarda:

```text
immediate = 24
```

Mas no encoding usado pela mini-A64 o campo `imm12` representa unidades de 8 bytes.

Então:

```text
24 / 8 = 3
```

O encoder grava:

```text
imm12 = 3
```

Na CPU:

```text
3 << 3 = 24
```

Portanto:

```text
assembler:
24 bytes → 3

CPU:
3 → 24 bytes
```

Isso explica por que um offset como `#7` não é válido nessa forma: ele não é múltiplo de 8.

---

## 13. Branches e labels

Exemplo:

```asm
loop:
    SUBS X0, X0, #1
    B.NE loop
```

O parser produz algo como:

```text
opcode = OP_B_COND
condition = NE
label = "loop"
```

Mas ainda falta descobrir o endereço numérico de `loop`.

A symbol table resolve isso.

Suponha:

```text
loop = 4
B.NE está no PC = 8
```

Então:

```text
target - pc
=
4 - 8
=
-4 bytes
```

Como cada instrução ocupa 4 bytes:

```text
-4 / 4 = -1
```

Esse `-1` é o deslocamento em unidades de instrução.

---

## 14. Por que branch usa signed

Um branch pode ir para frente:

```asm
B end
```

ou para trás:

```asm
B loop
```

Por isso o offset precisa suportar negativos.

Usamos algo como:

```c
int64_t offset;
```

Depois mantemos apenas os bits do campo:

```c
uint32_t imm19 =
    (uint32_t)offset & 0x7FFFF;
```

ou:

```c
uint32_t imm26 =
    (uint32_t)offset & 0x03FFFFFF;
```

Isso mantém a representação em complemento de dois dentro do tamanho do campo.

---

## 15. B / BL

Para:

```asm
B label
BL label
```

Fluxo:

```text
label
↓
symbol_table_find()
↓
target
↓
target - pc
↓
/ 4
↓
imm26
```

`B` e `BL` têm bits fixos diferentes, mas usam o mesmo princípio de offset relativo.

---

## 16. B.cond

Exemplo:

```asm
B.NE loop
```

O parser já converte:

```text
NE → condition = 1
```

O encoder monta:

```text
imm19
+
condition
```

Fluxo:

```text
"loop"
↓
symbol table
↓
endereço
↓
offset relativo ao PC
↓
imm19
```

E o código de condição vai para seu campo específico.

---

## 17. BR / BLR / RET

Essas instruções usam registrador, não label.

Exemplos:

```asm
BR X5
BLR X10
RET
```

O encoder seleciona o pattern fixo e insere `Rn`:

```c
word |= (instr->rn & 0x1F) << 5;
```

Para:

```asm
RET
```

o parser já transformou implicitamente em:

```text
RET X30
```

Logo o encoder recebe simplesmente:

```text
rn = 30
```

---

## 18. `encode_instruction()` é o dispatcher

A função principal não deve implementar toda a lógica de bits.

Ela olha:

```c
instr->opcode
```

e despacha para a função correta:

```text
OP_ADD_IMM
↓
encode_add_sub_imm()

OP_ADD_SHIFT
↓
encode_add_sub_shift()

OP_MOVZ
↓
encode_move_wide()

OP_B_COND
↓
encode_branch_cond()
```

Assim cada função cuida de uma família específica.

---

## 19. Por que o encoder recebe `pc`

A maioria das instruções não precisa do PC.

Branches precisam calcular:

```text
target - pc
```

Por isso uma interface útil é:

```c
encode_instruction(
    &instr,
    pc,
    &symbols,
    &word
);
```

`pc` é o endereço da própria instrução que está sendo codificada.

---

## 20. Por que o encoder recebe a SymbolTable

Branches simbólicos ainda possuem nomes:

```asm
B loop
BL function
B.NE again
```

A tabela converte:

```text
"loop" → 4
```

Então:

```text
label
↓
SymbolTable
↓
target address
↓
offset
↓
bits
```

---

## 21. Por que usamos `uint32_t *out`

A função precisa devolver duas coisas:

```text
1. sucesso ou falha
2. machine code
```

Então:

```c
return 1;
```

indica sucesso, e:

```c
*out = word;
```

devolve os 32 bits.

Exemplo:

```c
uint32_t machine_code;

int ok = encode_instruction(
    &instr,
    pc,
    &symbols,
    &machine_code
);
```

---

## 22. Pipeline completo do assembler

```text
source
↓
lexer
↓
tokens
↓
parser
↓
Statement
↓
Pass 1
↓
Symbol Table
↓
Pass 2
↓
Encoder
↓
uint32_t
↓
program.bin
```

Depois a CPU faz:

```text
program.bin
↓
fetch
↓
decoder
↓
execute
```

Portanto:

```text
ASSEMBLER:
texto → estrutura → bits

CPU:
bits → estrutura → execução
```

---

## 23. Regra mental mais importante

Se você esquecer quase tudo, lembre:

```text
DECODER:
(instr >> posição) & máscara
```

versus:

```text
ENCODER:
(campo & máscara) << posição
```

Exemplo:

```c
/* encoder */
word |= (rn & 0x1F) << 5;
```

Depois:

```c
/* decoder */
rn = (word >> 5) & 0x1F;
```

O mesmo valor entra e depois sai novamente.

---

## 24. Como testar o encoder: round trip

O melhor teste é fazer um **round trip**:

```text
Instruction original
↓
encoder
↓
32 bits
↓
decoder da CPU
↓
Instruction reconstruída
```

Exemplo inicial:

```text
opcode = OP_ADD_IMM
rd = 1
rn = 2
immediate = 5
```

Depois de `encode → decode`, queremos recuperar:

```text
opcode = OP_ADD_IMM
rd = 1
rn = 2
immediate = 5
```

Isso verifica diretamente se encoder e decoder concordam sobre o layout dos bits.

---

## 25. Visão mental final

O encoder não entende o texto assembly original.

Quem faz isso é o parser.

O encoder recebe:

```c
Instruction
```

e responde:

> Onde cada campo entra nos 32 bits?

Fluxo geral:

```text
Instruction
↓
identificar formato
↓
pegar pattern fixo
↓
validar campos
↓
mascarar
↓
deslocar
↓
combinar com OR
↓
uint32_t
```

Para branches:

```text
label
↓
symbol table
↓
target
↓
target - pc
↓
offset
↓
campo imediato
```

A conexão fundamental é:

```text
encoder coloca os campos
↓
decoder extrai os mesmos campos
```

Esse é o ponto em que o assembler e o emulador Mini-A64 formam um único sistema.
