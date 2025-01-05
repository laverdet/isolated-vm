#!/bin/sh
# ./embed.sh <source> <target> <variable>
SIZE=$(($(stat -c %s "$1") + 1))

cat <<EOF > "$2.h"
#pragma once
extern const char $3[$SIZE];
EOF

cat <<EOF > "$2.c"
#include "$2.h"
const char $3[$SIZE] = {
$(xxd -i < "$1"), 0x00
};
EOF
