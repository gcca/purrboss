# Argon2 Vendor Copy

- Upstream: https://github.com/P-H-C/phc-winner-argon2
- Commit: `f57e61e19229e23c4445b85494dbf7c07de721cb`
- Commit date: 2021-06-25 10:21:15 +0200
- License: CC0 1.0 Universal or Apache License 2.0, at your option. See `LICENSE`.

## Local Selection

This directory vendors the portable reference library implementation needed for
the public Argon2 APIs, including `argon2d_hash_raw`,
`argon2d_hash_encoded`, and `argon2d_verify`.

Copied source files:

- `include/argon2.h`
- `src/argon2.c`
- `src/core.c`
- `src/core.h`
- `src/encoding.c`
- `src/encoding.h`
- `src/ref.c`
- `src/thread.c`
- `src/thread.h`
- `src/blake2/blake2.h`
- `src/blake2/blake2-impl.h`
- `src/blake2/blake2b.c`
- `src/blake2/blamka-round-ref.h`

Not copied: upstream CLI, tests, benchmarks, KAT files, generated package files,
platform project files, and SIMD `opt.c` implementation.
