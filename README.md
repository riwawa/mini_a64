# Mini-A64

Mini-A64 is an educational CPU toolchain inspired by AArch64, built from scratch in C.

The goal of the project is to understand how software moves through multiple abstraction layers:

```text
assembly
   ↓
assembler
   ↓
machine code
   ↓
CPU emulator
   ↓
registers / memory / flags
```

The project is still a **work in progress**. The next major step is to build a compiler that generates Mini-A64 assembly.

---

## Current status

The project currently includes:

- a simplified 64-bit AArch64-inspired ISA
- 31 general-purpose registers (`X0`–`X30`)
- a byte-addressable 1 MiB memory model
- fixed-width 32-bit instructions
- NZCV flags
- little-endian memory access
- a two-pass assembler
- labels and symbol resolution
- binary generation
- a CPU emulator with fetch / decode / execute
- arithmetic, logical, branch, function call, and memory instructions
- automated ISA and integration tests
- 57 assertions currently passing

---

## Project structure

```text
projects/
├── assembler/
├── cpu/
│   ├── cpu.c
│   ├── cpu.h
│   └── main.c
├── programs/
│   ├── loop.s
│   ├── arithmetic.s
│   ├── branch.s
│   ├── memory.s
│   └── function_call.s
├── tests/
│   ├── test_cpu.c
│   ├── asm/
│   └── bin/
├── diagrams/
└── Makefile
```

---

## Supported instruction families

### Arithmetic

- `ADD` immediate
- `ADDS` immediate
- `SUB` immediate
- `SUBS` immediate
- `ADD` shifted register
- `ADDS` shifted register
- `SUB` shifted register
- `SUBS` shifted register
- `ADD` extended register
- `ADDS` extended register
- `SUB` extended register
- `SUBS` extended register

### Logical

- `AND` immediate
- `ANDS` immediate
- `ORR` immediate
- `EOR` immediate
- `AND` shifted register
- `ANDS` shifted register
- `ORR` shifted register
- `EOR` shifted register

### Move wide

- `MOVZ`
- `MOVK`
- `MOVN`

### Memory

- `LDR`
- `STR`

### Control flow

- `B`
- `BL`
- conditional branch (`B.cond`)
- `BR`
- `BLR`
- `RET`

---

## Example

Source:

```asm
MOVZ X0, #3

loop:
SUBS X0, X0, #1
B.NE loop
```

Assembler:

```bash
assembler/mini_a64_as programs/loop.s programs/loop.bin
```

Run:

```bash
./mini_a64_cpu programs/loop.bin
```

Expected final state:

```text
X0  = 0x0000000000000000
PC  = 0x000000000000100c
NZCV = 0110
```

---

## Building

Build the CPU emulator:

```bash
make cpu
```

Run the automated test suite:

```bash
make test
```

Clean generated artifacts:

```bash
make clean
```

Rebuild everything:

```bash
make rebuild
```

---

## Testing

The test suite assembles source programs automatically before executing them on the emulator.

Current flow:

```text
.s source
   ↓
assembler
   ↓
.bin
   ↓
CPU emulator
   ↓
register / NZCV assertions
```

Current result:

```text
57 assertions
0 failed
ALL TESTS PASSED
```

The suite currently covers:

- arithmetic instructions
- shifted and extended operands
- logical instructions
- move-wide instructions
- load/store
- branches
- function calls
- register branches
- NZCV flags
- end-to-end assembler → binary → CPU execution

---

## Assembler architecture

The assembler follows this pipeline:

```text
source
↓
lexer
↓
tokens
↓
parser
↓
statements / instructions
↓
pass 1
↓
symbol table
↓
pass 2
↓
encoder
↓
binary
```

### Why two passes?

The first pass discovers label addresses:

```text
label → address
```

The second pass resolves those labels and encodes instructions:

```text
address / offset → machine bits
```

This also allows forward references such as:

```asm
B end

MOVZ X0, #99

end:
MOVZ X1, #7
```

---

## CPU emulator architecture

The emulator follows the classic execution cycle:

```text
fetch
↓
decode
↓
execute
```

The machine state includes:

- `X0`–`X30`
- program counter (`PC`)
- NZCV flags
- memory

Programs are currently loaded at:

```text
0x1000
```

---

## NZCV flags

The emulator tracks:

```text
N = Negative
Z = Zero
C = Carry
V = Overflow
```

Example:

```text
NZCV = 0110
```

means:

```text
N = 0
Z = 1
C = 1
V = 0
```

---

## What I am learning through this project

This project is primarily educational and is being used to study:

- C
- systems programming
- computer architecture
- instruction encoding / decoding
- bit manipulation
- memory models
- assemblers
- parsers and lexers
- symbol tables
- calling conventions
- control flow
- automated testing
- debugging across abstraction layers

---

## Roadmap

### Done

- [x] simplified Mini-A64 ISA
- [x] CPU emulator
- [x] assembler
- [x] lexer
- [x] parser
- [x] labels
- [x] symbol table
- [x] two-pass assembly
- [x] binary generation
- [x] arithmetic and logical instructions
- [x] memory operations
- [x] branches and conditional branches
- [x] function calls with `BL` / `RET`
- [x] register branches with `BR` / `BLR`
- [x] NZCV flags
- [x] automated ISA tests
- [x] end-to-end assembly → machine code → execution tests

### Next

- [ ] improve assembler diagnostics
- [ ] expand instruction coverage
- [ ] document the Mini-A64 ISA formally
- [ ] build a disassembler
- [ ] add execution tracing / debugging
- [ ] define a simple Mini-A64 ABI
- [ ] add stack support
- [ ] build a high-level language compiler
- [ ] generate Mini-A64 assembly from source code

### Later

- [ ] microarchitecture model
- [ ] microcode
- [ ] datapath diagrams
- [ ] Logisim implementation
- [ ] physical hardware experiments

---

## Long-term goal

The long-term goal is to connect the complete stack:

```text
high-level language
        ↓
compiler
        ↓
Mini-A64 assembly
        ↓
assembler
        ↓
machine code
        ↓
CPU emulator
        ↓
microarchitecture
        ↓
hardware
```

---

## License

This project is currently intended for educational and personal learning purposes.
    