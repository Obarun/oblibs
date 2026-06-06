/*
 * attributes.h
 *
 * Copyright (c) 2026 Eric Vidal <eric@obarun.org>
 *
 * All rights reserved.
 *
 * This file is part of Obarun. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this
 * distribution.
 * This file may not be copied, modified, propagated, or distributed
 * except according to the terms contained in the LICENSE file.
 *
 * Compiler attribute macros, oblibs-native (replaces skalibs/gccattributes.h).
 * They expand to GNU __attribute__ on gcc/clang (both define __GNUC__) and to
 * nothing elsewhere, so they are always safe to use.
 */

#ifndef OB_ATTRIBUTES_H
#define OB_ATTRIBUTES_H

#ifdef __GNUC__

#define attribute_noreturn   __attribute__((noreturn))
#define attribute_unused     __attribute__((unused))
#define attribute_pure       __attribute__((pure))
#define attribute_const      __attribute__((const))
#define attribute_malloc     __attribute__((malloc))
#define attribute_deprecated __attribute__((deprecated))
#define attribute_weak       __attribute__((weak))

#else

#define attribute_noreturn
#define attribute_unused
#define attribute_pure
#define attribute_const
#define attribute_malloc
#define attribute_deprecated
#define attribute_weak

#endif

#endif
