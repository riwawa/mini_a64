#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../cpu/cpu.h"


/* =========================================================
 * TEST STATE
 * =========================================================
 */

static int tests_run = 0;
static int tests_failed = 0;


/* =========================================================
 * HELPERS
 * =========================================================
 */

static int assemble_and_run(
    const char *source,
    const char *binary)
{
    char command[512];

    snprintf(
        command,
        sizeof(command),
        "assembler/mini_a64_as %s %s",
        source,
        binary
    );


    printf(
        "assembling %s...\n",
        source
    );


    int result =
        system(command);


    if (result != 0)
    {
        printf(
            "[FAIL] assembler failed for %s\n",
            source
        );

        tests_failed++;

        return 0;
    }


    if (!run_binary(binary))
    {
        printf(
            "[FAIL] CPU failed for %s\n",
            binary
        );

        tests_failed++;

        return 0;
    }


    return 1;
}


static void expect_u64(
    const char *name,
    uint64_t actual,
    uint64_t expected)
{
    tests_run++;


    if (actual != expected)
    {
        tests_failed++;

        printf(
            "[FAIL] %s: "
            "expected 0x%016llx (%llu), "
            "got 0x%016llx (%llu)\n",
            name,
            (unsigned long long)expected,
            (unsigned long long)expected,
            (unsigned long long)actual,
            (unsigned long long)actual
        );
    }
    else
    {
        printf(
            "[PASS] %s\n",
            name
        );
    }
}


static void expect_nzcv(
    const char *name,
    int N,
    int Z,
    int C,
    int V)
{
    tests_run++;


    uint8_t expected =
        ((N & 1) << 3) |
        ((Z & 1) << 2) |
        ((C & 1) << 1) |
        (V & 1);


    if (nzcv != expected)
    {
        tests_failed++;

        printf(
            "[FAIL] %s: "
            "expected NZCV=%d%d%d%d, "
            "got NZCV=%d%d%d%d\n",
            name,
            N,
            Z,
            C,
            V,
            !!(nzcv & (1 << 3)),
            !!(nzcv & (1 << 2)),
            !!(nzcv & (1 << 1)),
            !!(nzcv & (1 << 0))
        );
    }
    else
    {
        printf(
            "[PASS] %s: NZCV=%d%d%d%d\n",
            name,
            N,
            Z,
            C,
            V
        );
    }
}


/* =========================================================
 * EXISTING INTEGRATION PROGRAMS
 * =========================================================
 */


/* ---------------------------------------------------------
 * LOOP
 * ---------------------------------------------------------
 */

static void test_loop(void)
{
    printf(
        "\n== loop ==\n"
    );


    if (!assemble_and_run(
            "programs/loop.s",
            "tests/bin/loop.bin"))
    {
        return;
    }


    expect_u64(
        "X0 == 0",
        reg[0],
        0
    );


    expect_nzcv(
        "loop final flags",
        0,
        1,
        1,
        0
    );
}


/* ---------------------------------------------------------
 * ARITHMETIC
 * ---------------------------------------------------------
 */

static void test_arithmetic(void)
{
    printf(
        "\n== arithmetic ==\n"
    );


    if (!assemble_and_run(
            "programs/arithmetic.s",
            "tests/bin/arithmetic.bin"))
    {
        return;
    }


    expect_u64(
        "X0 == 10",
        reg[0],
        10
    );

    expect_u64(
        "X1 == 20",
        reg[1],
        20
    );

    expect_u64(
        "X2 == 30",
        reg[2],
        30
    );

    expect_u64(
        "X3 == 25",
        reg[3],
        25
    );

    expect_u64(
        "X4 == 65",
        reg[4],
        65
    );
}


/* ---------------------------------------------------------
 * BRANCH
 * ---------------------------------------------------------
 */

static void test_branch(void)
{
    printf(
        "\n== branch ==\n"
    );


    if (!assemble_and_run(
            "programs/branch.s",
            "tests/bin/branch.bin"))
    {
        return;
    }


    expect_u64(
        "X0 == 1",
        reg[0],
        1
    );

    expect_u64(
        "X1 == 5",
        reg[1],
        5
    );
}


/* ---------------------------------------------------------
 * MEMORY
 * ---------------------------------------------------------
 */

static void test_memory(void)
{
    printf(
        "\n== memory ==\n"
    );


    if (!assemble_and_run(
            "programs/memory.s",
            "tests/bin/memory.bin"))
    {
        return;
    }


    expect_u64(
        "X0 == 8192",
        reg[0],
        8192
    );

    expect_u64(
        "X1 == 42",
        reg[1],
        42
    );

    expect_u64(
        "X2 == 42",
        reg[2],
        42
    );
}


/* ---------------------------------------------------------
 * FUNCTION CALL
 * ---------------------------------------------------------
 */

static void test_function_call(void)
{
    printf(
        "\n== function_call ==\n"
    );


    if (!assemble_and_run(
            "programs/function_call.s",
            "tests/bin/function_call.bin"))
    {
        return;
    }


    expect_u64(
        "X0 == 12",
        reg[0],
        12
    );

    expect_u64(
        "X1 == 5",
        reg[1],
        5
    );

    expect_u64(
        "X30 == 0x100c",
        reg[30],
        0x100c
    );
}


/* =========================================================
 * NZCV TESTS
 * =========================================================
 */


/* ---------------------------------------------------------
 * ZERO FLAG
 * ---------------------------------------------------------
 */

static void test_flags_zero(void)
{
    printf(
        "\n== flags: zero ==\n"
    );


    if (!assemble_and_run(
            "tests/asm/flags_zero.s",
            "tests/bin/flags_zero.bin"))
    {
        return;
    }


    expect_u64(
        "X1 == 0",
        reg[1],
        0
    );


    expect_nzcv(
        "SUBS zero",
        0,
        1,
        1,
        0
    );
}


/* ---------------------------------------------------------
 * NEGATIVE FLAG
 * ---------------------------------------------------------
 */

static void test_flags_negative(void)
{
    printf(
        "\n== flags: negative ==\n"
    );


    if (!assemble_and_run(
            "tests/asm/flags_negative.s",
            "tests/bin/flags_negative.bin"))
    {
        return;
    }


    expect_u64(
        "X1 == UINT64_MAX",
        reg[1],
        UINT64_MAX
    );


    expect_nzcv(
        "SUBS negative",
        1,
        0,
        0,
        0
    );
}


/* ---------------------------------------------------------
 * CARRY FLAG
 * ---------------------------------------------------------
 */

static void test_flags_carry(void)
{
    printf(
        "\n== flags: carry ==\n"
    );


    if (!assemble_and_run(
            "tests/asm/flags_carry.s",
            "tests/bin/flags_carry.bin"))
    {
        return;
    }


    expect_u64(
        "X1 == 0 after carry",
        reg[1],
        0
    );


    expect_nzcv(
        "ADDS carry",
        0,
        1,
        1,
        0
    );
}


/* ---------------------------------------------------------
 * OVERFLOW FLAG
 * ---------------------------------------------------------
 */

static void test_flags_overflow(void)
{
    printf(
        "\n== flags: overflow ==\n"
    );


    if (!assemble_and_run(
            "tests/asm/flags_overflow.s",
            "tests/bin/flags_overflow.bin"))
    {
        return;
    }


    expect_u64(
        "X0 == INT64_MAX bits",
        reg[0],
        0x7FFFFFFFFFFFFFFFULL
    );


    expect_u64(
        "X1 == INT64_MIN bits",
        reg[1],
        0x8000000000000000ULL
    );


    expect_nzcv(
        "ADDS overflow",
        1,
        0,
        0,
        1
    );
}


/* =========================================================
 * ADD / SUB IMMEDIATE
 * =========================================================
 */

static void test_add_sub_imm(void)
{
    printf(
        "\n== ADD/SUB immediate ==\n"
    );


    if (!assemble_and_run(
            "tests/asm/add_sub_imm.s",
            "tests/bin/add_sub_imm.bin"))
    {
        return;
    }


    expect_u64(
        "ADD immediate",
        reg[1],
        15
    );

    expect_u64(
        "SUB immediate",
        reg[2],
        12
    );

    expect_u64(
        "ADDS immediate",
        reg[3],
        13
    );

    expect_u64(
        "SUBS immediate",
        reg[4],
        0
    );


    expect_nzcv(
        "SUBS immediate flags",
        0,
        1,
        1,
        0
    );
}


/* =========================================================
 * ADD / SUB SHIFTED REGISTER
 * =========================================================
 */

static void test_add_sub_shift(void)
{
    printf(
        "\n== ADD/SUB shifted ==\n"
    );


    if (!assemble_and_run(
            "tests/asm/add_sub_shift.s",
            "tests/bin/add_sub_shift.bin"))
    {
        return;
    }


    expect_u64(
        "ADD shifted",
        reg[2],
        11
    );

    expect_u64(
        "SUB shifted",
        reg[3],
        5
    );

    expect_u64(
        "ADDS shifted",
        reg[4],
        11
    );

    expect_u64(
        "SUBS shifted",
        reg[5],
        5
    );


    expect_nzcv(
        "SUBS shifted flags",
        0,
        0,
        1,
        0
    );
}


/* =========================================================
 * ADD / SUB EXTENDED REGISTER
 * =========================================================
 */

static void test_add_sub_ext(void)
{
    printf(
        "\n== ADD/SUB extended ==\n"
    );


    if (!assemble_and_run(
            "tests/asm/add_sub_ext.s",
            "tests/bin/add_sub_ext.bin"))
    {
        return;
    }


    expect_u64(
        "ADD extended",
        reg[2],
        14
    );

    expect_u64(
        "SUB extended",
        reg[3],
        10
    );

    expect_u64(
        "ADDS extended",
        reg[4],
        14
    );

    expect_u64(
        "SUBS extended",
        reg[5],
        10
    );


    expect_nzcv(
        "SUBS extended flags",
        0,
        0,
        1,
        0
    );
}


/* =========================================================
 * LOGICAL IMMEDIATE
 * =========================================================
 */

static void test_logical_imm(void)
{
    printf(
        "\n== logical immediate ==\n"
    );


    if (!assemble_and_run(
            "tests/asm/logical_imm.s",
            "tests/bin/logical_imm.bin"))
    {
        return;
    }


    expect_u64(
        "AND immediate",
        reg[1],
        15
    );

    expect_u64(
        "ORR immediate",
        reg[2],
        255
    );

    expect_u64(
        "EOR immediate",
        reg[3],
        0
    );

    expect_u64(
        "ANDS immediate",
        reg[4],
        15
    );


    expect_nzcv(
        "ANDS immediate flags",
        0,
        0,
        0,
        0
    );
}


/* =========================================================
 * LOGICAL SHIFTED REGISTER
 * =========================================================
 */

static void test_logical_shift(void)
{
    printf(
        "\n== logical shifted ==\n"
    );


    if (!assemble_and_run(
            "tests/asm/logical_shift.s",
            "tests/bin/logical_shift.bin"))
    {
        return;
    }


    expect_u64(
        "AND shifted",
        reg[2],
        6
    );

    expect_u64(
        "ORR shifted",
        reg[3],
        63
    );

    expect_u64(
        "EOR shifted",
        reg[4],
        15
    );

    expect_u64(
        "ANDS shifted",
        reg[5],
        6
    );


    expect_nzcv(
        "ANDS shifted flags",
        0,
        0,
        0,
        0
    );
}


/* =========================================================
 * MOVE WIDE
 * =========================================================
 */

static void test_move_wide(void)
{
    printf(
        "\n== move wide ==\n"
    );


    if (!assemble_and_run(
            "tests/asm/move_wide.s",
            "tests/bin/move_wide.bin"))
    {
        return;
    }


    expect_u64(
        "MOVZ + MOVK",
        reg[0],
        0x0000000056781234ULL
    );


    expect_u64(
        "MOVN",
        reg[1],
        UINT64_MAX
    );
}


/* =========================================================
 * LOAD / STORE
 * =========================================================
 */

static void test_load_store(void)
{
    printf(
        "\n== load/store ==\n"
    );


    if (!assemble_and_run(
            "tests/asm/load_store.s",
            "tests/bin/load_store.bin"))
    {
        return;
    }


    expect_u64(
        "X0 == base address",
        reg[0],
        8192
    );

    expect_u64(
        "stored value",
        reg[1],
        42
    );

    expect_u64(
        "LDR result",
        reg[2],
        42
    );
}


/* =========================================================
 * BR REGISTER
 * =========================================================
 */

static void test_br(void)
{
    printf(
        "\n== BR ==\n"
    );


    if (!assemble_and_run(
            "tests/asm/branch_reg.s",
            "tests/bin/branch_reg.bin"))
    {
        return;
    }


    expect_u64(
        "BR destination executed",
        reg[1],
        7
    );
}


/* =========================================================
 * BLR + RET
 * =========================================================
 */

static void test_blr_ret(void)
{
    printf(
        "\n== BLR + RET ==\n"
    );


    if (!assemble_and_run(
            "tests/asm/blr_ret.s",
            "tests/bin/blr_ret.bin"))
    {
        return;
    }


    expect_u64(
        "X1 == 12",
        reg[1],
        12
    );


    expect_u64(
        "X30 == return address",
        reg[30],
        0x100c
    );
}


/* =========================================================
 * MAIN
 * =========================================================
 */

int main(void)
{
    /*
     * Garante que a pasta de saída existe.
     */
    if (system("mkdir -p tests/bin") != 0)
    {
        fprintf(
            stderr,
            "failed to create tests/bin\n"
        );

        return EXIT_FAILURE;
    }


    /*
     * Integration tests.
     */
    test_loop();
    test_arithmetic();
    test_branch();
    test_memory();
    test_function_call();


    /*
     * NZCV.
     */
    test_flags_zero();
    test_flags_negative();
    test_flags_carry();
    test_flags_overflow();


    /*
     * ISA families.
     */
    test_add_sub_imm();
    test_add_sub_shift();
    test_add_sub_ext();

    test_logical_imm();
    test_logical_shift();

    test_move_wide();
    test_load_store();

    test_br();
    test_blr_ret();


    printf(
        "\n"
        "==============================\n"
        "TEST SUMMARY\n"
        "==============================\n"
        "%d assertions\n"
        "%d failed\n"
        "==============================\n",
        tests_run,
        tests_failed
    );


    if (tests_failed == 0)
    {
        printf(
            "ALL TESTS PASSED\n"
        );

        return EXIT_SUCCESS;
    }


    return EXIT_FAILURE;
}