#pragma once

namespace emb {

// Which contexts share an object, and so what has to order its accesses.
//   local: one core, the code on it and the interrupts that preempt that
//          code. Only the compiler reorders what they see, so signal
//          fences suffice and no barrier instruction is emitted.
//   smp:   contexts on several cores. Thread fences and acquire/release
//          operations: a dmb on ARMv7-M, lda/stl where ARMv8-M has them.
enum class memory_scope { local, smp };

} // namespace emb
