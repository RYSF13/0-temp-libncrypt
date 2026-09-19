# Design notes

## Scope

The repository contains two layers:

- The classical layer is a namespace-renamed port of Monocypher 4.0.3. Its implementation and public API should be reviewed against the Monocypher manual rather than treated as a new cryptographic design.
- The PQC layer is a single C translation unit. It includes local FIPS 202 primitives, an ML-KEM-768 reference core, an ML-DSA-44 reference core, and small wrappers that use the classical layer.

All internal PQC symbols are `static`, which keeps the translation unit self-contained and avoids collisions with other copies of FIPS 202, Kyber, or Dilithium code in a link unit.

## FIPS 202 core

The file contains Keccak-f[1600], SHAKE128, SHAKE256, SHA3-256, and SHA3-512. The state is 25 little-endian 64-bit lanes. The incremental SHAKE interface separates absorb, finalize, and squeeze phases. The non-incremental helpers use the same state machine for one-shot hashing.

The PQC algorithms use SHAKE as follows:

- ML-KEM uses SHAKE128 to expand the public matrix and SHAKE256 for noise and rejection-key derivation.
- ML-DSA uses SHAKE128 for the public matrix and SHAKE256 for key expansion, signing, and verification.

## ML-KEM-768 data flow

The ML-KEM implementation uses the parameter set `k = 3`:

1. Key generation expands the 64-byte input into the public matrix seed, noise seed, and rejection value.
2. The IND-CPA key pair is generated with NTT-domain polynomial arithmetic.
3. The public key is the serialized polynomial vector followed by the public matrix seed.
4. The secret key contains the IND-CPA secret, the public key, a hash of the public key, and the rejection value.
5. Encapsulation hashes the message and public key, derives coins, performs IND-CPA encryption, and returns the first half of the derived key.
6. Decapsulation decrypts, re-encrypts for the consistency check, computes the rejection key, and conditionally selects the valid or rejection secret.

The wrapper adds an explicit public-key coefficient check before encapsulation. It also checks the stored public-key hash before decapsulation so that a corrupted serialized secret key can be reported.

## ML-DSA-44 data flow

The ML-DSA implementation uses the parameter set `K = 4`, `L = 4`, `eta = 2`, and a 32-byte challenge hash.

Key generation expands the input seed into `rho`, `rho_prime`, and the private signing seed. It expands the matrix, samples the short secret vectors, computes the public vector, rounds it into `t1` and `t0`, and serializes the public and secret keys.

Signing computes the message representative `mu`, derives `rho_prime` from the private signing seed, the signing randomness, and `mu`, and then repeats the rejection-sampling loop from the reference implementation:

1. Sample the intermediate vector `y`.
2. Compute and decompose `A*y`.
3. Derive the challenge polynomial.
4. Compute the response `z` and reject if its norm is too large.
5. Account for the secret vectors and reject if the low bits are unsafe.
6. Build the hint vector and reject if it exceeds the encoding limit.
7. Serialize the challenge, response, and hint.

The public wrapper uses an empty context and an all-zero `rnd`, so it selects the deterministic variant described by FIPS 204. There is no API for hedged signing with fresh per-signature randomness.

## Hybrid encryption

The hybrid wrapper uses ML-KEM as a public-key KEM and the classical XChaCha20-Poly1305 implementation as the data cipher.

The message key is:

```text
BLAKE2b_keyed(shared_secret, kem_ciphertext, 32)
```

The KEM ciphertext is included as the BLAKE2b message so that the AEAD key changes when the encapsulation changes. The AEAD nonce is a fixed all-zero 24-byte value and is not transmitted. This construction is safe only if each encapsulation creates a fresh key. The seed supplied to encapsulation is therefore a security-critical uniqueness input, not merely a test parameter.

Associated data is passed to the AEAD layer and is not serialized by the wrapper. The receiver must obtain the same associated data out of band.

## Memory handling

The public wrappers wipe caller-supplied seeds and wipe the local KEM shared secret and hybrid message key. The reference cores wipe most private polynomial buffers after use.

The ML-DSA signing routine does not currently wipe every local temporary before returning. In particular, the local polynomial vectors `z`, `w1`, and `h`, the challenge polynomial, and the matrix remain in stack storage after successful signing. Some of these values are public or signature-derived, but the cleanup policy is inconsistent and should be tightened before a security-sensitive release. See `doc/CODE_REVIEW.md`.

## Portability assumptions

The reference arithmetic uses signed right shifts, narrow integer assignments, and implicit conversions that are common on two's-complement C implementations. They compile cleanly with the normal warning set used in this repository, but enabling `-Wconversion` and `-Wsign-conversion` produces many diagnostics. A portability and integer-safety pass is still needed.

The code uses C99 declarations and fixed-width integer types. It does not provide a platform abstraction for secure memory, compiler barriers, hardware randomness, or fault resistance.

## External references

- NIST FIPS 203: <https://csrc.nist.gov/pubs/fips/203/final>
- NIST FIPS 204: <https://csrc.nist.gov/pubs/fips/204/final>
- Monocypher manual: <https://monocypher.org/manual/>
- CRYSTALS Kyber reference repository: <https://github.com/pq-crystals/kyber>
- CRYSTALS Dilithium reference repository: <https://github.com/pq-crystals/dilithium>
