// libncrypt-pqc (ML-KEM-768 and ML-DSA-44)
// Post-quantum extension for libncrypt.
//
// The implementation is self-contained and uses ncrypt.c for BLAKE2b,
// XChaCha20-Poly1305, constant-time comparison, and memory wiping.

#ifndef NCRYPT_PQC_H
#define NCRYPT_PQC_H

#include "ncrypt.h"

////////////////////
/// Buffer sizes ///
////////////////////

// All sizes are in bytes.
//
//                       public key  secret key  ciphertext  signature
// ML-KEM-768                1184        2400        1088         -
// ML-DSA-44                 1312        2560           -        2420
//
// ML-KEM shared secrets are 32 bytes. Key-pair generation takes a
// 64-byte seed. Encapsulation and ML-DSA key-pair generation take
// 32-byte seeds. Every seed is wiped before the function returns.


///////////////////////////////
/// ML-KEM-768 key exchange ///
///////////////////////////////

// Generate an ML-KEM-768 key pair from seed. The seed is wiped.
void ncrypt_mlkem768_key_pair(uint8_t secret_key[2400],
                              uint8_t public_key[1184],
                              uint8_t seed      [64]);

// Encapsulate a fresh 32-byte shared secret for public_key. The seed is
// wiped. Returns 0 on success and -1 if public_key is not a valid
// ML-KEM-768 encapsulation key. On failure, ciphertext and
// shared_secret are left untouched.
int ncrypt_mlkem768_encapsulate(uint8_t       ciphertext   [1088],
                                uint8_t       shared_secret[32],
                                const uint8_t public_key   [1184],
                                uint8_t       seed         [32]);

// Decapsulate ciphertext with secret_key. Returns 0 on success and -1
// if the key does not contain a consistent copy of its public key.
// A malformed ciphertext is not reported. It produces an unrelated
// shared secret through implicit rejection, as required by FIPS 203.
int ncrypt_mlkem768_decapsulate(uint8_t       shared_secret[32],
                                const uint8_t ciphertext   [1088],
                                const uint8_t secret_key   [2400]);


////////////////////////////
/// ML-DSA-44 signatures ///
////////////////////////////

// Generate an ML-DSA-44 key pair from seed. The seed is wiped.
void ncrypt_mldsa44_key_pair(uint8_t secret_key[2560],
                             uint8_t public_key[1312],
                             uint8_t seed      [32]);

// Sign a message with the deterministic ML-DSA-44 variant. The context
// string is empty and the signing randomness is 32 zero bytes.
void ncrypt_mldsa44_sign(uint8_t        signature [2420],
                         const uint8_t  secret_key[2560],
                         const uint8_t *message, size_t message_size);

// Return 0 if signature is valid and -1 otherwise.
int ncrypt_mldsa44_check(const uint8_t  signature [2420],
                         const uint8_t  public_key[1312],
                         const uint8_t *message, size_t message_size);


//////////////////////
/// High-level API ///
//////////////////////

// The aliases below select ML-KEM-768 for encryption and ML-DSA-44
// for signatures. Do not reuse a key pair or a seed for both purposes.

void ncrypt_pqc_key_pair(uint8_t secret_key[2400],
                         uint8_t public_key[1184],
                         uint8_t seed      [64]);

void ncrypt_pqc_sign_key_pair(uint8_t secret_key[2560],
                              uint8_t public_key[1312],
                              uint8_t seed      [32]);

// Encapsulate a message key for their_public_key and encrypt plain_text
// with XChaCha20-Poly1305. Send kem_ct, mac, and cipher_text to the
// recipient. ad is authenticated but is not included in the output.
// The seed is wiped. Returns 0 on success and -1 if the public key is
// malformed. On failure, no output buffer is modified.
//
// The implementation uses a fixed AEAD nonce. This is safe only when
// seed is fresh for every encryption and kem_ct is not reused.
int ncrypt_pqc_encrypt(uint8_t       *cipher_text,
                       uint8_t        mac       [16],
                       uint8_t        kem_ct    [1088],
                       const uint8_t  their_public_key[1184],
                       uint8_t        seed      [32],
                       const uint8_t *ad,         size_t ad_size,
                       const uint8_t *plain_text, size_t text_size);

// Decrypt a message produced by ncrypt_pqc_encrypt. Returns 0 on
// success and -1 if the secret key is inconsistent or authentication
// fails. On failure, plain_text is left untouched.
int ncrypt_pqc_decrypt(uint8_t       *plain_text,
                       const uint8_t  kem_ct    [1088],
                       const uint8_t  mac       [16],
                       const uint8_t  secret_key[2400],
                       const uint8_t *ad,          size_t ad_size,
                       const uint8_t *cipher_text, size_t text_size);

// Detached ML-DSA-44 signatures using the deterministic variant above.
void ncrypt_pqc_sign(uint8_t        signature [2420],
                     const uint8_t  secret_key[2560],
                     const uint8_t *message, size_t message_size);
int ncrypt_pqc_check(const uint8_t  signature [2420],
                     const uint8_t  public_key[1312],
                     const uint8_t *message, size_t message_size);

#endif // NCRYPT_PQC_H
