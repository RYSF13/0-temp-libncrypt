# Public API

This document describes the public functions in `ncrypt-pqc.h`. The classical functions in `ncrypt.h` follow the Monocypher 4.0.3 manual and are summarized in the repository review.

## General rules

- The array bounds in the prototypes describe the required object sizes. C does not check them at runtime.
- Pointers must refer to valid readable or writable storage for the complete length used by the function.
- Do not pass a null pointer for a non-empty input or output.
- Empty messages and associated data may use a null input pointer only when the corresponding size is zero and the caller has verified that the underlying API contract permits it.
- Do not assume that input and output buffers may overlap. Use separate buffers unless an API contract explicitly says otherwise.
- The functions do not allocate memory.
- The PQC functions do not obtain randomness. The caller supplies seed material.
- Every seed argument is wiped before the function returns, including the failure path of encapsulation.
- Secret keys and intermediate secrets should be kept in protected memory and wiped when no longer needed.

Return values use the convention already used by the Monocypher-derived layer: zero means success, and a negative value means failure.

## Sizes

| Object | ML-KEM-768 | ML-DSA-44 |
| --- | ---: | ---: |
| Public key | 1184 | 1312 |
| Secret key | 2400 | 2560 |
| Ciphertext | 1088 | - |
| Signature | - | 2420 |
| Shared secret | 32 | - |
| Key-pair seed | 64 | 32 |
| Encapsulation seed | 32 | - |

The header repeats these values in array declarations. A future release should expose named size macros to reduce the chance of application-side mismatches.

## ML-KEM-768

### `ncrypt_mlkem768_key_pair`

```c
void ncrypt_mlkem768_key_pair(uint8_t secret_key[2400],
                              uint8_t public_key[1184],
                              uint8_t seed[64]);
```

The function deterministically derives an ML-KEM-768 key pair from the 64-byte seed and wipes the seed. The first 32 bytes are the key-generation input and the second 32 bytes are retained in the encoded secret key for implicit rejection.

The caller must fill the seed with a cryptographically secure random value for normal key generation. Deterministic seeds are useful for tests and reproducible vectors only.

### `ncrypt_mlkem768_encapsulate`

```c
int ncrypt_mlkem768_encapsulate(uint8_t ciphertext[1088],
                                uint8_t shared_secret[32],
                                const uint8_t public_key[1184],
                                uint8_t seed[32]);
```

The function validates the serialized public polynomial coefficients, then derives a ciphertext and a 32-byte shared secret. The seed is wiped. It returns `0` on success and `-1` for a malformed public key. On that failure path, the ciphertext and shared-secret outputs are left untouched.

The encapsulation seed should be fresh for every encryption operation. Reusing it makes encapsulation deterministic.

### `ncrypt_mlkem768_decapsulate`

```c
int ncrypt_mlkem768_decapsulate(uint8_t shared_secret[32],
                                const uint8_t ciphertext[1088],
                                const uint8_t secret_key[2400]);
```

The function first checks the public-key hash stored inside the secret key. An inconsistent secret key returns `-1`. A damaged or forged ciphertext is not reported as an error. Instead, ML-KEM implicit rejection returns a pseudorandom shared secret. An application must authenticate data derived from the shared secret.

## ML-DSA-44

### `ncrypt_mldsa44_key_pair`

```c
void ncrypt_mldsa44_key_pair(uint8_t secret_key[2560],
                             uint8_t public_key[1312],
                             uint8_t seed[32]);
```

The function deterministically derives an ML-DSA-44 key pair from a 32-byte seed and wipes the seed.

### `ncrypt_mldsa44_sign`

```c
void ncrypt_mldsa44_sign(uint8_t signature[2420],
                         const uint8_t secret_key[2560],
                         const uint8_t *message, size_t message_size);
```

This is pure ML-DSA-44 with an empty context string. It selects the deterministic FIPS 204 signing variant by passing 32 zero bytes as `rnd`. Signing the same message with the same key therefore produces the same signature.

The deterministic variant is permitted by FIPS 204, but the standard describes the hedged variant as the default and recommends fresh signing randomness for protection against some side-channel and fault attacks. This API does not currently expose the hedged variant.

### `ncrypt_mldsa44_check`

```c
int ncrypt_mldsa44_check(const uint8_t signature[2420],
                         const uint8_t public_key[1312],
                         const uint8_t *message, size_t message_size);
```

The function returns `0` for a valid signature and `-1` otherwise. Verification uses no random input.

## Hybrid encryption

### `ncrypt_pqc_encrypt`

```c
int ncrypt_pqc_encrypt(uint8_t *cipher_text,
                       uint8_t mac[16],
                       uint8_t kem_ct[1088],
                       const uint8_t their_public_key[1184],
                       uint8_t seed[32],
                       const uint8_t *ad, size_t ad_size,
                       const uint8_t *plain_text, size_t text_size);
```

The function performs these steps:

1. Encapsulate a fresh ML-KEM shared secret for `their_public_key`.
2. Derive a 32-byte message key with keyed BLAKE2b using the shared secret as the key and `kem_ct` as the message.
3. Encrypt `plain_text` with XChaCha20-Poly1305 using a fixed all-zero 24-byte nonce.
4. Authenticate `ad` and the ciphertext and return `kem_ct`, `mac`, and `cipher_text`.

The KEM ciphertext is not a separate AEAD associated-data input, but it is bound to the derived key. Changing it causes decryption authentication to fail.

The fixed nonce is safe only under the freshness assumption that the KEM seed is never reused. The caller must use a cryptographically secure, fresh 32-byte seed for every call. If the same seed is reused with the same public key, the key, nonce, and KEM ciphertext repeat. This can expose the plaintext relationship through stream-cipher nonce reuse.

On a malformed recipient public key, the function returns `-1`, wipes the seed, and leaves its output buffers untouched.

### `ncrypt_pqc_decrypt`

```c
int ncrypt_pqc_decrypt(uint8_t *plain_text,
                       const uint8_t kem_ct[1088],
                       const uint8_t mac[16],
                       const uint8_t secret_key[2400],
                       const uint8_t *ad, size_t ad_size,
                       const uint8_t *cipher_text, size_t text_size);
```

The function decapsulates `kem_ct`, derives the same message key, and verifies and decrypts the AEAD record. The associated data must be byte-for-byte identical to the value used for encryption.

It returns `0` on success and `-1` for an inconsistent secret key or an authentication failure. The plaintext buffer is not modified when the function fails.

The caller must not treat a successful ML-KEM decapsulation as proof that a ciphertext was valid. The AEAD result is the final authenticity check for this wrapper.

## Signature aliases

`ncrypt_pqc_sign` calls `ncrypt_mldsa44_sign`, and `ncrypt_pqc_check` calls `ncrypt_mldsa44_check`. They use the same deterministic, empty-context ML-DSA-44 behavior.

## Classical layer

The `ncrypt.c` and `ncrypt.h` functions are a namespace-renamed port of Monocypher. Consult the Monocypher manual for the detailed preconditions, return values, nonce rules, and security caveats:

- <https://monocypher.org/manual/>
- <https://monocypher.org/manual/aead>
- <https://monocypher.org/manual/blake2b>
- <https://monocypher.org/manual/argon2>
- <https://monocypher.org/manual/x25519>
- <https://monocypher.org/manual/eddsa>
- <https://monocypher.org/manual/ed25519>
- <https://monocypher.org/manual/sha512>
