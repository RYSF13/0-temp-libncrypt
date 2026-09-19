# Code review

## Review scope

The base repository commit is `bad2767313809233b180e563e5aa7b0b8cff2f86`. This review covers that source plus the working-tree changes to the PQC comments, public header, and documentation. It includes the Monocypher-derived classical layer, the PQC translation unit, build behavior, and memory handling.

The review is a source review, not a cryptographic proof or an independent audit. The temporary repository contains no build system, CI configuration, committed test suite, or license file.

## Executive summary

The PQC implementation is internally coherent and its deterministic ML-KEM and deterministic ML-DSA results matched the corresponding CRYSTALS reference implementations for the fixed inputs used during this review. Basic round-trip and tamper tests also passed under AddressSanitizer and UndefinedBehaviorSanitizer.

The code is not ready for release. The most important concerns are the fixed AEAD nonce, the absence of an internal randomness interface, deterministic-only ML-DSA signing, incomplete stack cleanup in the signing routine, the lack of official KAT and fuzz infrastructure, and unresolved source licensing and provenance.

## Findings

### CR-01 - High: the hybrid API makes seed freshness a nonce-safety requirement

`ncrypt_pqc_encrypt` uses an all-zero XChaCha20-Poly1305 nonce. It derives the AEAD key from the ML-KEM shared secret and `kem_ct`, so a fresh encapsulation gives a fresh key and makes the fixed nonce usable for that record.

The caller controls the 32-byte encapsulation seed. Reusing that seed with the same recipient public key reproduces `kem_ct`, the shared secret, the derived message key, and the fixed nonce. That is AEAD nonce and key reuse. The API does not enforce freshness or expose a nonce field.

Recommendation:

- Keep the fixed nonce only if the API contract is intentionally deterministic and freshness is enforced by a higher layer.
- Prefer a design that obtains fresh randomness internally or accepts a unique nonce and binds it to the authenticated record.
- Add a misuse-resistant test and make seed uniqueness a prominent API requirement.

### CR-02 - High: ML-DSA exposes only deterministic signing

`ncrypt_mldsa44_sign` passes 32 zero bytes as `rnd`. FIPS 204 permits the deterministic variant, but its hedged variant uses fresh signing randomness to make side-channel and fault-attack countermeasures easier.

Recommendation:

- Add a signing entry point that accepts or obtains fresh `rnd` material.
- Keep the deterministic function explicitly named and documented as a special-purpose variant.
- Do not describe deterministic signing as the default FIPS 204 operational choice.

### CR-03 - Medium: ML-DSA stack cleanup is incomplete

`mldsa_signature_internal` wipes `seedbuf`, `s1`, `s2`, `t0`, `y`, `w0`, and `state` on the successful return path. It does not wipe all local polynomial objects, including `z`, `w1`, `h`, `cp`, and `mat`. The code currently uses some of these values as public or signature-derived data, but the cleanup policy is inconsistent and future changes could make the omission sensitive.

Recommendation:

- Wipe every local object that ever contains a secret or secret-derived value before returning.
- Keep cleanup on every return path, including malformed-key and future error paths.
- Add a test or static rule that checks secret-bearing local objects in key generation, signing, and decapsulation.

### CR-04 - Medium: there is no randomness or secure-key ownership layer

The library accepts raw seeds and does not obtain entropy, validate its quality, or separate key-generation domains. This is consistent with Monocypher's low-level style, but it is a serious integration hazard for an application-facing PQC API.

Recommendation:

- Document the required seed source and domain separation in the API contract.
- Provide a higher-level integration layer that obtains randomness from a platform-approved source.
- Consider accepting an explicit RNG callback or a cryptographic module interface rather than making every caller manage raw seed bytes.

### CR-05 - Medium: integer portability and warning hygiene need a dedicated pass

The reference code relies on assumptions common to two's-complement C implementations, including signed right shifts and narrow integer conversions. A normal warning build succeeds, but `-Wconversion -Wsign-conversion` produces 532 diagnostics in `ncrypt-pqc.c`.

These diagnostics are not proof of an algorithmic failure, but they make compiler portability and review harder. The code should not be made warning-clean by blindly adding casts; each arithmetic bound must be checked first.

Recommendation:

- Define and document the supported C implementation model.
- Review every narrowing conversion and signed shift against the FIPS or reference bounds.
- Add compiler jobs for GCC and Clang with the supported warning sets.
- Consider a verified or type-safe implementation for a release build.

### CR-06 - Medium: no official KAT, fuzz, or negative-test suite is committed

The repository has no tests for NIST ACVP vectors, serialization edge cases, malformed keys, implicit rejection, signature malleability, context handling, or long messages. The lack of tests is especially risky for a cryptographic implementation merged from several reference sources.

Recommendation:

- Add ML-KEM and ML-DSA ACVP or equivalent official known-answer tests.
- Add round-trip tests for every public function and boundary message length.
- Add negative tests for malformed public keys, corrupted secret keys, modified KEM ciphertexts, modified associated data, modified MACs, and modified signatures.
- Run fuzzing against decapsulation, verification, and serialization helpers with sanitizers enabled.

### CR-07 - Medium: source provenance and licensing are unresolved

The source includes code derived from the CRYSTALS Kyber and Dilithium reference implementations and a FIPS 202 implementation. The temporary repository does not include a license file or a complete attribution and provenance record.

This is a release blocker even if the implementation is technically correct. The requested MIT license has intentionally not been added to this temporary repository.

Recommendation:

- Record the exact upstream commits, license texts, and any required notices.
- Verify that the intended project license is compatible with every imported component.
- Add the final license and attribution files before publication.

### CR-08 - Low: public size constants are repeated magic numbers

The public prototypes repeat values such as `1184`, `2400`, `1088`, and `2420`. Applications cannot use a named library constant to allocate a matching buffer, so future parameter changes can silently create mismatches.

Recommendation:

- Add public `NCRYPT_MLKEM768_*` and `NCRYPT_MLDSA44_*` size macros.
- Use those macros in both declarations and implementation checks.
- Keep the serialized formats versioned if a future release changes a parameter set.

### CR-09 - Low: buffer and overlap contracts are not enforced

The PQC header describes array sizes but C cannot enforce them. The implementation also does not state an overlap policy for all input and output pairs. The classical layer inherits Monocypher's low-level precondition model and does not validate lengths or pointers.

Recommendation:

- State the non-overlap requirement in every public API section.
- Add debug-only assertions in a wrapper layer if appropriate.
- Keep the low-level functions small, but do not present them as safe against arbitrary pointer or length misuse.

### CR-10 - Low: the single translation unit is difficult to maintain

Keeping FIPS 202, ML-KEM, and ML-DSA in one file avoids link-name collisions, but it makes provenance tracking, review, test isolation, and compiler diagnostics harder. The original reference file comments also mixed several naming and formatting conventions.

Recommendation:

- Keep the single-file option only for a deliberately small distribution target.
- Otherwise split the code into namespaced modules and keep an update record for each upstream component.
- Preserve the current static symbol discipline when splitting.

## Classical layer observations

The classical implementation follows Monocypher 4.0.3 and should be reviewed against its manual. The port keeps the low-level behavior, including the absence of general input validation and the caller-managed randomness model. The most important release tasks for this layer are provenance verification, test coverage, compiler portability, and checking that every public function's preconditions are carried into the new documentation.

The review did not identify an intentional change to the classical cryptographic algorithms. The PQC work therefore should not be used as evidence that the classical layer has been independently audited.

## Positive observations

- The PQC public symbols are prefixed with `ncrypt_` and internal symbols are static.
- Seeds are wiped by the public key-generation and encapsulation wrappers.
- ML-KEM decapsulation uses implicit rejection instead of exposing a ciphertext-validity oracle.
- The hybrid decrypt path checks authentication before copying plaintext, and the plaintext buffer remains unchanged on failure.
- The public header now documents the deterministic ML-DSA choice and the fixed-nonce freshness requirement.
- The PQC comments were reduced to short, factual comments in the `ncrypt.c` section style rather than retaining repetitive template blocks.

## Recommended release order

1. Resolve license and upstream provenance.
2. Decide whether the fixed-nonce hybrid API and deterministic-only signing API are acceptable. If not, change the interfaces.
3. Add official KAT, negative, fuzz, and sanitizer tests.
4. Complete the stack-cleanup and integer-portability passes.
5. Run independent review and side-channel analysis.
6. Add a supported build matrix and only then publish a license-bearing release.
