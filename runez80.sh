#!/bin/sh

for warning in array-bounds implicit-function-declaration implicit-int incompatible-pointer-types int-conversion main-return-type return-type strict-prototypes uninitialized unsequenced; do
    Werror="$Werror -Werror=$warning"
done
for warning in unused-function unused-variable unused-value constant-conversion bool-operation constant-logical-operand empty-body tautological-compare tautological-constant-compare tautological-constant-out-of-range-compare tautological-pointer-compare compare-distinct-pointer-types pointer-sign parentheses-equality; do
    Wno="$Wno -Wno-$warning"
done
#set -x

# native
$(readlink -f $(which ez80-clang)) -Xclang -test-ez80-hack $Werror $Wno $CFLAGS -O0 -fsanitize=memory,bounds -fsanitize-trap=signed-integer-overflow,unreachable,return,bounds,builtin,bool,shift -o native.elf || exit
timeout ${NATIVE_TIMEOUT}s ./native.elf >native.out 2>native.err
ec=$?
cat native.err >&2
test -s native.err -o $ec -eq 124 && exit $ec
echo "exit code: $ec" >> native.out

# ez80
ez80-clang $Wno -S $CFLAGS -o ez80.asm || exit 0
$FASMG -i 'source "ez80.asm"' ez80.bin || exit 0
$RUNEZ80 ez80.bin > ez80.out
ec=$?
test $ec -eq 124 && exit $ec
echo "exit code: $ec" >> ez80.out

! diff -U9999 -s native.out ez80.out
