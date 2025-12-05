* NGP_OPTIMIZATION_JUMPTABLE+NGP_OPTIMIZATION_JUMPTABLE_EMBEDDED_POSTOP (-.10ms/frame in reference title, Metal Slug 1 level 1)

This branch contains an experiemental alteration of the NGP core where the function table has been converted to a jump table. (uses flag NGP_OPTIMIZATION_JUMPTABLE)

Additionally there are two optional jumptable flags.
NGP_OPTIMIZATION_JUMPTABLE_EMBEDDED_POSTOP
NGP_OPTIMIZATION_JUMPTABLE_HYBRID

NGP_OPTIMIZATION_JUMPTABLE_EMBEDDED_POSTOP places the post operation handlers within each opcode handler, otherwise it jumps to a single post-op handler after the jump table. I got improved performance with this flag so it is enabled in the tlsc900h.c file, along with NGP_OPTIMIZATION_JUMPTABLE.

NGP_OPTIMIZATION_JUMPTABLE_HYBRID will use the original function table for opcodes 0x80 and higher, which for the most part use a secondary function table for further operation decoding with the next byte. This did not improve performance, so is disabled by default, but remains for further experimentation with this model. A faster decoding method would benefit the core greatly for these operations.

The NGP module must be compiled with -o2 in order to benefit from this design, otherwise the cache locality of instructions in the larger jump table from -o3 will have an adverse affect on performance.
