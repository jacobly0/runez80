#!/bin/sh

for warning in implicit-int implicit-function-declaration return-type main-return-type uninitialized; do
    Werror="$Werror -Werror=$warning"
done
for warning in unused-function unused-variable unused-value constant-conversion constant-logical-operand tautological-constant-compare tautological-constant-out-of-range-compare tautological-pointer-compare compare-distinct-pointer-types pointer-sign parentheses-equality; do
    Wno="$Wno -Wno-$warning"
done
set -x

# native
clang $Werror $Wno $CFLAGS -o native.elf || exit
./native.elf > native.out
echo "exit code: $?" >> native.out

# ez80
ez80-clang $Wno -S $CFLAGS -o ez80.asm || exit 0
$FASMG -i 'srcs "ez80.asm"' ez80.bin || exit 0
$RUNEZ80 ez80.bin > ez80.out
echo "exit code: $?" >> ez80.out

! diff -U9999 -s native.out ez80.out
