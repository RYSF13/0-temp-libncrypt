# Testing and verification

## Warning

The commands below are review checks, not a complete validation suite. No official NIST ACVP corpus is stored in this temporary repository, and no independent side-channel or formal verification result is implied.

## Compiler checks

Normal warning build:

```sh
cc -std=c99 -Wall -Wextra -Wpedantic -Wshadow \
   -Wstrict-prototypes -Wmissing-prototypes -Werror \
   -c ncrypt.c ncrypt-pqc.c
```

This check passed during the review.

A stricter conversion check was also run:

```sh
cc -std=c99 -Wall -Wextra -Wpedantic \
   -Wconversion -Wsign-conversion \
   -c ncrypt-pqc.c
```

It compiled but emitted 532 diagnostics. These diagnostics are recorded as a release concern in `doc/CODE_REVIEW.md`; they should be addressed by checking arithmetic bounds rather than by adding blanket casts.

## Sanitizer round trip

A temporary harness exercised:

- ML-KEM-768 key generation, encapsulation, and decapsulation.
- ML-DSA-44 key generation, signing, verification, and modified-signature rejection.
- Hybrid encryption and decryption.
- Modified-MAC rejection.
- The guarantee that plaintext remains unchanged after an authentication failure.

The harness was compiled with:

```sh
cc -std=c99 -O2 -g \
   -fsanitize=address,undefined -fno-omit-frame-pointer \
   test_roundtrip.c ncrypt.c ncrypt-pqc.c -o test_roundtrip
./test_roundtrip
```

The check passed without AddressSanitizer or UndefinedBehaviorSanitizer findings. The temporary harness is not part of the repository deliverable.

## Reference comparisons

The fixed-seed ML-KEM-768 output was compared byte-for-byte with the current `pq-crystals/kyber` reference implementation for the same 64-byte key-generation input and 32-byte encapsulation input.

The deterministic ML-DSA-44 output was compared byte-for-byte with the current `pq-crystals/dilithium` reference implementation after disabling its randomized-signing build option and using the same 32-byte key-generation input.

These comparisons cover one deterministic vector for each algorithm. They do not replace the full ACVP test suite.

## Tests still needed

- Official ML-KEM-768 key-generation, encapsulation, and decapsulation vectors.
- Official ML-DSA-44 key-generation, deterministic signing, hedged signing, and verification vectors.
- Malformed public-key and secret-key cases.
- All-zero, empty, one-byte, block-sized, and multi-block messages.
- Associated-data changes, KEM ciphertext changes, MAC changes, and truncated inputs.
- Signature malleability and non-canonical encoding cases.
- Fuzzing with sanitizers.
- Cross-compiler and cross-platform checks.
- Compiler and assembly side-channel analysis for supported targets.
