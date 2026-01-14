#!/bin/sh
# ./embed.sh <source> <target> <variable>
set -eu
SIZE=$(($(cat "$1" | wc -c) + 1))

cat <<EOF > "$2.h"
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
extern const char $3[$SIZE];
#ifdef __cplusplus
}
#endif
EOF

cat <<EOF > "$2.c"
#include "$2.h"
const char $3[$SIZE] = {
$(xxd -i < "$1"), 0x00
};
EOF
