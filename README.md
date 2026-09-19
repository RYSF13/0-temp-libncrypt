# libncrypt

libncrypt is a C99 cryptographic library derived from Monocypher 4.0.3. This checkout also contains a post-quantum extension with ML-KEM-768 and ML-DSA-44.

This repository is a temporary review snapshot. It is not a release and must not be treated as production-ready cryptographic software. Please do not open pull requests against the temporary repository.

## Contents

- `ncrypt.c` and `ncrypt.h`: the Monocypher-derived classical cryptography layer.
- `ncrypt-pqc.c` and `ncrypt-pqc.h`: ML-KEM-768, ML-DSA-44, and the hybrid encryption wrapper.
- `doc/API.md`: public API and buffer contracts.
- `doc/DESIGN.md`: implementation structure and cryptographic data flow.
- `doc/CODE_REVIEW.md`: review findings, priorities, and verification performed.
- `doc/TESTING.md`: reproducible build and test commands.

## Supported algorithms

The classical layer follows the Monocypher API and implementation for:

- XChaCha20-Poly1305 authenticated encryption.
- BLAKE2b hashing and keyed hashing.
- Argon2 password hashing.
- X25519 key exchange and EdDSA signatures.
- ChaCha20, Poly1305, Elligator 2, SHA-512, HMAC, HKDF, and Ed25519.

The PQC layer provides:

- ML-KEM-768, as specified by FIPS 203.
- ML-DSA-44, as specified by FIPS 204.
- A hybrid wrapper that combines ML-KEM-768 with XChaCha20-Poly1305.

The ML-DSA wrapper currently uses the deterministic signing variant with a 32-byte all-zero `rnd` value. The hybrid wrapper uses a fixed AEAD nonce and therefore requires a fresh encapsulation seed for every encryption.

## Build

The source has no build system yet. A minimal static library build is:

```sh
cc -std=c99 -Wall -Wextra -Wpedantic -Wshadow \
   -Wstrict-prototypes -Wmissing-prototypes -Werror \
   -c ncrypt.c ncrypt-pqc.c
ar rcs libncrypt.a ncrypt.o ncrypt-pqc.o
```

The code assumes a C99 compiler and the usual two's-complement integer representation used by the reference implementations. See `doc/DESIGN.md` and `doc/CODE_REVIEW.md` before integrating it into another project.

## Security warning

The API does not obtain randomness internally. Callers must provide cryptographically secure, domain-separated seed material and must not reuse a seed for a purpose where uniqueness is required. In particular, reusing the 32-byte seed passed to `ncrypt_pqc_encrypt` reuses the ML-KEM ciphertext, the derived message key, and the fixed AEAD nonce.

The PQC implementation has not yet undergone an independent cryptographic audit, formal verification, or the complete NIST ACVP test suite in this repository. Use an established, maintained cryptographic provider for production deployments.

## License status

A license file is intentionally not included in this temporary repository. Licensing and source provenance must be resolved before publication. No MIT license file has been added here.

## Documentation

Start with [`doc/API.md`](doc/API.md), then read [`doc/CODE_REVIEW.md`](doc/CODE_REVIEW.md) for the release blockers and recommended next steps.
