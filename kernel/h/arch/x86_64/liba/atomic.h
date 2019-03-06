#ifndef ARCH_X86_64_LIBA_ATOMIC_H
#define ARCH_X86_64_LIBA_ATOMIC_H

#include <base.h>

static inline u32 atomic32_get (u32 * p) { u32 x; ASM("movl      (%1), %0" : "=r"(x) : "r"(p)); return x; }
static inline u64 atomic64_get (u64 * p) { u64 x; ASM("movq      (%1), %0" : "=r"(x) : "r"(p)); return x; }
static inline u32 thiscpu32_get(u32 * p) { u32 x; ASM("movl %%gs:(%1), %0" : "=r"(x) : "r"(p)); return x; }
static inline u64 thiscpu64_get(u64 * p) { u64 x; ASM("movq %%gs:(%1), %0" : "=r"(x) : "r"(p)); return x; }

extern u32 atomic32_set(u32 * p, u32 x);
extern u32 atomic32_inc(u32 * p);
extern u32 atomic32_dec(u32 * p);
extern u32 atomic32_add(u32 * p, u32 x);
extern u32 atomic32_sub(u32 * p, u32 x);
extern u32 atomic32_and(u32 * p, u32 x);
extern u32 atomic32_or (u32 * p, u32 x);
extern u32 atomic32_xor(u32 * p, u32 x);
extern u32 atomic32_cas(u32 * p, u32 x, u32 y); // return old value of `*p`

extern u64 atomic64_set(u64 * p, u64 x);
extern u64 atomic64_inc(u64 * p);
extern u64 atomic64_dec(u64 * p);
extern u64 atomic64_add(u64 * p, u64 x);
extern u64 atomic64_sub(u64 * p, u64 x);
extern u64 atomic64_and(u64 * p, u64 x);
extern u64 atomic64_or (u64 * p, u64 x);
extern u64 atomic64_xor(u64 * p, u64 x);
extern u64 atomic64_cas(u64 * p, u64 x, u64 y); // return old value of `*p`

extern u32 thiscpu32_set(u32 * p, u32 x);
extern u32 thiscpu32_inc(u32 * p);
extern u32 thiscpu32_dec(u32 * p);
extern u32 thiscpu32_add(u32 * p, u32 x);
extern u32 thiscpu32_sub(u32 * p, u32 x);
extern u32 thiscpu32_and(u32 * p, u32 x);
extern u32 thiscpu32_or (u32 * p, u32 x);
extern u32 thiscpu32_xor(u32 * p, u32 x);
extern u32 thiscpu32_cas(u32 * p, u32 x, u32 y);

extern u64 thiscpu64_set(u64 * p, u64 x);
extern u64 thiscpu64_inc(u64 * p);
extern u64 thiscpu64_dec(u64 * p);
extern u64 thiscpu64_add(u64 * p, u64 x);
extern u64 thiscpu64_sub(u64 * p, u64 x);
extern u64 thiscpu64_and(u64 * p, u64 x);
extern u64 thiscpu64_or (u64 * p, u64 x);
extern u64 thiscpu64_xor(u64 * p, u64 x);
extern u64 thiscpu64_cas(u64 * p, u64 x, u64 y);

// default atomic operand size must be same with machine word
// so we can perform atomic operations on pointer types
#define atomic_t            u64

#define atomic_set(p,x)     atomic64_set((u64 *) (p), (u64) (x))
#define atomic_inc(p)       atomic64_inc((u64 *) (p))
#define atomic_dec(p)       atomic64_dec((u64 *) (p))
#define atomic_add(p,x)     atomic64_add((u64 *) (p), (u64) (x))
#define atomic_sub(p,x)     atomic64_sub((u64 *) (p), (u64) (x))
#define atomic_and(p,x)     atomic64_and((u64 *) (p), (u64) (x))
#define atomic_or(p,x)      atomic64_or ((u64 *) (p), (u64) (x))
#define atomic_xor(p,x)     atomic64_xor((u64 *) (p), (u64) (x))
#define atomic_cas(p,x,y)   atomic64_cas((u64 *) (p), (u64) (x), (u64) (y))

#define thiscpu_get(p)      thiscpu64_get((u64 *) (p))
#define thiscpu_set(p,x)    thiscpu64_set((u64 *) (p), (u64) (x))
#define thiscpu_inc(p)      thiscpu64_inc((u64 *) (p))
#define thiscpu_dec(p)      thiscpu64_dec((u64 *) (p))
#define thiscpu_add(p,x)    thiscpu64_add((u64 *) (p), (u64) (x))
#define thiscpu_sub(p,x)    thiscpu64_sub((u64 *) (p), (u64) (x))
#define thiscpu_and(p,x)    thiscpu64_and((u64 *) (p), (u64) (x))
#define thiscpu_or(p,x)     thiscpu64_or ((u64 *) (p), (u64) (x))
#define thiscpu_xor(p,x)    thiscpu64_xor((u64 *) (p), (u64) (x))

#define thiscpu_set(p,x)    thiscpu64_set((u64 *) (p), (u64) (x))
#define thiscpu_inc(p)      thiscpu64_inc((u64 *) (p))
#define thiscpu_dec(p)      thiscpu64_dec((u64 *) (p))
#define thiscpu_add(p,x)    thiscpu64_add((u64 *) (p), (u64) (x))
#define thiscpu_sub(p,x)    thiscpu64_sub((u64 *) (p), (u64) (x))
#define thiscpu_and(p,x)    thiscpu64_and((u64 *) (p), (u64) (x))
#define thiscpu_or(p,x)     thiscpu64_or ((u64 *) (p), (u64) (x))
#define thiscpu_xor(p,x)    thiscpu64_xor((u64 *) (p), (u64) (x))
#define thiscpu_cas(p,x,y)  thiscpu64_cas((u64 *) (p), (u64) (x), (u64) (y))

#endif // ARCH_X86_64_LIBA_ATOMIC_H
