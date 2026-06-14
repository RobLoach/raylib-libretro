/* SPDX-License-Identifier: Apache-2.0 */
/*
 * snkv_impl.c — the single translation unit that compiles the SNKV
 * (SQLite B-tree key-value store) amalgamation.
 *
 * snkv.h is generated from https://github.com/hash-anu/snkv via
 * `sh scripts/gen_snkv_header.sh`. Vector/usearch support is excluded by the
 * generator default; encryption (Monocypher) is inlined into the amalgamation
 * and compiled here but never used by raylib-libretro.
 *
 * Every other translation unit includes snkv.h header-only (without
 * SNKV_IMPLEMENTATION) for the declarations and links against this object.
 */
#define SNKV_IMPLEMENTATION
#include "snkv.h"
