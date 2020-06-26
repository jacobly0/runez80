#!/bin/sh

for warning in array-bounds conditional-uninitialized division-by-zero implicit-function-declaration implicit-int incompatible-pointer-types int-conversion main-return-type return-type strict-prototypes uninitialized unsequenced; do
    Werror="$Werror -Werror=$warning"
done
for warning in bool-operation compare-distinct-pointer-types constant-conversion constant-logical-operand empty-body implicit-int-conversion parentheses-equality pointer-sign tautological-compare tautological-constant-compare tautological-constant-out-of-range-compare tautological-pointer-compare unused-function unused-value unused-variable; do
    Wno="$Wno -Wno-$warning"
done
#set -x

# native
$(readlink -f $(which ez80-clang)) -Xclang -test-ez80-hack $Werror $Wno $CFLAGS -O0 -glldb -fsanitize=memory,bounds -fsanitize-trap=signed-integer-overflow,unreachable,return,bounds,builtin,bool,shift -o native.elf 2>native.diag || exit
cat native.diag >&2
grep "too \(few\|many\) arguments in call to '\|tentative array definition assumed to have one element\|expected ';' at end of declaration list" native.diag && exit 1
timeout ${NATIVE_TIMEOUT}s ./native.elf >native.out 2>native.err
ec=$?
cat native.err >&2
test -s native.err -o $ec -eq 124 -o $ec -eq 132 && exit $ec
echo "exit code: $ec" >> native.out

# ez80
ez80-clang $Wno -S $CFLAGS -o ez80.asm || exit 0
$FASMG -i 'source "ez80.asm"' ez80.bin || exit 0
$RUNEZ80 ez80.bin > ez80.out
ec=$?
test $ec -eq 124 && exit $ec
echo "exit code: $ec" >> ez80.out

! diff -U9999 -s native.out ez80.out
