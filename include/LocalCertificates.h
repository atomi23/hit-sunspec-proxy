#pragma once
#include <cstring>
#include <mbedtls/base64.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/oid.h>
#include <mbedtls/pk.h>
#include <mbedtls/pem.h>
#include <mbedtls/x509_crt.h>

namespace LocalCertificates {
inline int parseCa(mbedtls_x509_crt *crt, const unsigned char *certificate) {
  mbedtls_pem_context pem;
  mbedtls_pem_init(&pem);
  size_t used = 0;
  int rc = mbedtls_pem_read_buffer(&pem, "-----BEGIN CERTIFICATE-----", "-----END CERTIFICATE-----",
      certificate, nullptr, 0, &used);
  if (!rc) rc = mbedtls_x509_crt_parse_der_with_ext_cb(crt, pem.MBEDTLS_PRIVATE(buf),
      pem.MBEDTLS_PRIVATE(buflen), 1,
      [](void *, const mbedtls_x509_crt *, const mbedtls_x509_buf *oid, int,
         const unsigned char *, const unsigned char *) -> int {
        return MBEDTLS_OID_CMP(MBEDTLS_OID_NAME_CONSTRAINTS, oid) == 0 ? 0 : -1;
      }, nullptr);
  mbedtls_pem_free(&pem);
  return rc;
}

enum class CaPairStatus { Valid, Corrupt, Retry };

inline CaPairStatus caValidationStatus(int rc, bool parsing) {
  if (!rc) return CaPairStatus::Valid;
  if (!parsing) {
    return rc == MBEDTLS_ERR_PK_TYPE_MISMATCH || rc == MBEDTLS_ERR_PK_BAD_INPUT_DATA ||
           rc == MBEDTLS_ERR_ECP_BAD_INPUT_DATA || rc == MBEDTLS_ERR_ECP_INVALID_KEY ?
           CaPairStatus::Corrupt : CaPairStatus::Retry;
  }
  const int low = -((-rc) & 0x7f);
  if (low != 0 && low != MBEDTLS_ERR_ASN1_OUT_OF_DATA && low != MBEDTLS_ERR_ASN1_UNEXPECTED_TAG &&
      low != MBEDTLS_ERR_ASN1_INVALID_LENGTH && low != MBEDTLS_ERR_ASN1_LENGTH_MISMATCH &&
      low != MBEDTLS_ERR_ASN1_INVALID_DATA && low != MBEDTLS_ERR_BASE64_INVALID_CHARACTER)
    return CaPairStatus::Retry;
  switch (-((-rc) & ~0x7f)) {
    case MBEDTLS_ERR_PEM_NO_HEADER_FOOTER_PRESENT:
    case MBEDTLS_ERR_PEM_INVALID_DATA:
    case MBEDTLS_ERR_X509_INVALID_FORMAT:
    case MBEDTLS_ERR_X509_INVALID_VERSION:
    case MBEDTLS_ERR_X509_INVALID_SERIAL:
    case MBEDTLS_ERR_X509_INVALID_ALG:
    case MBEDTLS_ERR_X509_INVALID_NAME:
    case MBEDTLS_ERR_X509_INVALID_DATE:
    case MBEDTLS_ERR_X509_INVALID_SIGNATURE:
    case MBEDTLS_ERR_X509_INVALID_EXTENSIONS:
    case MBEDTLS_ERR_X509_SIG_MISMATCH:
    case MBEDTLS_ERR_X509_CERT_UNKNOWN_FORMAT:
    case MBEDTLS_ERR_PK_KEY_INVALID_VERSION:
    case MBEDTLS_ERR_PK_KEY_INVALID_FORMAT:
    case MBEDTLS_ERR_PK_INVALID_PUBKEY:
    case MBEDTLS_ERR_PK_INVALID_ALG:
      return CaPairStatus::Corrupt;
    default:
      return CaPairStatus::Retry;
  }
}

inline CaPairStatus checkCaPair(const unsigned char *certificate, const unsigned char *privateKey,
                        mbedtls_ctr_drbg_context *rng) {
  if (!certificate[0] || !privateKey[0]) return CaPairStatus::Corrupt;
  mbedtls_x509_crt crt;
  mbedtls_pk_context key;
  mbedtls_x509_crt_init(&crt);
  mbedtls_pk_init(&key);
  CaPairStatus status = caValidationStatus(parseCa(&crt, certificate), true);
  if (status == CaPairStatus::Valid && !crt.MBEDTLS_PRIVATE(ca_istrue)) status = CaPairStatus::Corrupt;
  if (status == CaPairStatus::Valid)
    status = caValidationStatus(mbedtls_pk_parse_key(&key, privateKey,
        strlen(reinterpret_cast<const char *>(privateKey)) + 1, nullptr, 0, mbedtls_ctr_drbg_random, rng), true);
  if (status == CaPairStatus::Valid)
    status = caValidationStatus(mbedtls_pk_check_pair(&crt.pk, &key, mbedtls_ctr_drbg_random, rng), false);
  mbedtls_pk_free(&key);
  mbedtls_x509_crt_free(&crt);
  return status;
}

inline int generate(bool ca, const char *subject, const char *issuer,
                    const char *notBefore, const char *notAfter,
                    mbedtls_pk_context *key, mbedtls_pk_context *issuerKey,
                    mbedtls_x509_san_list *san, mbedtls_ctr_drbg_context *rng,
                    unsigned char *pem, size_t pemSize, unsigned char *keyPem, size_t keySize,
                    bool selfSigned = false) {
  mbedtls_x509write_cert crt;
  mbedtls_x509write_crt_init(&crt);
  int rc = 0;
  do {
    if ((rc = mbedtls_pk_setup(key, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY)))) break;
    if ((rc = mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1, mbedtls_pk_ec(*key), mbedtls_ctr_drbg_random, rng))) break;
    mbedtls_x509write_crt_set_version(&crt, MBEDTLS_X509_CRT_VERSION_3);
    mbedtls_x509write_crt_set_md_alg(&crt, MBEDTLS_MD_SHA256);
    mbedtls_x509write_crt_set_subject_key(&crt, key);
    mbedtls_x509write_crt_set_issuer_key(&crt, ca || selfSigned ? key : issuerKey);
    if ((rc = mbedtls_x509write_crt_set_subject_name(&crt, subject))) break;
    if ((rc = mbedtls_x509write_crt_set_issuer_name(&crt, issuer))) break;
    unsigned char serial[16];
    if ((rc = mbedtls_ctr_drbg_random(rng, serial, sizeof(serial)))) break;
    serial[0] = (serial[0] & 0x7f) | 1;
    if ((rc = mbedtls_x509write_crt_set_serial_raw(&crt, serial, sizeof(serial)))) break;
    if ((rc = mbedtls_x509write_crt_set_validity(&crt, notBefore, notAfter))) break;
    if ((rc = mbedtls_x509write_crt_set_basic_constraints(&crt, ca, ca ? 0 : -1))) break;
    if ((rc = mbedtls_x509write_crt_set_key_usage(&crt, ca ?
        MBEDTLS_X509_KU_KEY_CERT_SIGN | MBEDTLS_X509_KU_CRL_SIGN :
        MBEDTLS_X509_KU_DIGITAL_SIGNATURE | MBEDTLS_X509_KU_KEY_AGREEMENT))) break;
    if ((rc = mbedtls_x509write_crt_set_subject_key_identifier(&crt))) break;
    if (ca) {
      // NameConstraints ::= SEQUENCE { permittedSubtrees [0] GeneralSubtrees }
      static const unsigned char constraints[] = {
        0x30,0x48,0xa0,0x46,
        0x30,0x07,0x82,0x05,'l','o','c','a','l',
        0x30,0x0b,0x82,0x09,'h','o','m','e','.','a','r','p','a',
        0x30,0x0a,0x87,0x08,10,0,0,0,255,0,0,0,
        0x30,0x0a,0x87,0x08,172,16,0,0,255,240,0,0,
        0x30,0x0a,0x87,0x08,192,168,0,0,255,255,0,0,
        0x30,0x0a,0x87,0x08,169,254,0,0,255,255,0,0
      };
      if ((rc = mbedtls_x509write_crt_set_extension(&crt, MBEDTLS_OID_NAME_CONSTRAINTS,
          MBEDTLS_OID_SIZE(MBEDTLS_OID_NAME_CONSTRAINTS), 1, constraints, sizeof(constraints)))) break;
    } else {
      if ((rc = mbedtls_x509write_crt_set_authority_key_identifier(&crt))) break;
      mbedtls_asn1_sequence eku = {};
      eku.buf.tag = MBEDTLS_ASN1_OID;
      eku.buf.p = (unsigned char *)MBEDTLS_OID_SERVER_AUTH;
      eku.buf.len = MBEDTLS_OID_SIZE(MBEDTLS_OID_SERVER_AUTH);
      if ((rc = mbedtls_x509write_crt_set_ext_key_usage(&crt, &eku))) break;
      if ((rc = mbedtls_x509write_crt_set_subject_alternative_name(&crt, san))) break;
    }
    if ((rc = mbedtls_x509write_crt_pem(&crt, pem, pemSize, mbedtls_ctr_drbg_random, rng))) break;
    rc = mbedtls_pk_write_key_pem(key, keyPem, keySize);
  } while (false);
  mbedtls_x509write_crt_free(&crt);
  return rc;
}
}
