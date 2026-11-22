// Build with mbedTLS 3.x: c++ -Iinclude tools/local_ca_test.cpp -lmbedx509 -lmbedcrypto -o local_ca_test
#include "LocalCertificates.h"
#include <mbedtls/entropy.h>
#include <arpa/inet.h>
#include <cstdio>
#include <cstring>

int main(int argc, char **argv) {
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context rng;
  mbedtls_entropy_init(&entropy);
  mbedtls_ctr_drbg_init(&rng);
  int rc = mbedtls_ctr_drbg_seed(&rng, mbedtls_entropy_func, &entropy, nullptr, 0);
  mbedtls_pk_context ca, leaf;
  mbedtls_pk_init(&ca);
  mbedtls_pk_init(&leaf);
  unsigned char pem[2048] = {}, key[512] = {};
  const char *caName = "CN=HIT-SunSpec-Proxy CA test,O=HIT-SunSpec-Proxy";
  if (!rc) rc = LocalCertificates::generate(true, caName, caName, "20260101000000", "20360101000000",
      &ca, nullptr, nullptr, &rng, pem, sizeof(pem), key, sizeof(key));
  auto save = [](const char *path, unsigned char *data) {
    FILE *f = fopen(path, "w");
    if (!f) return false;
    const bool ok = fwrite(data, 1, strlen((char *)data), f) == strlen((char *)data);
    return fclose(f) == 0 && ok;
  };
  if (!rc && !save("ca.pem", pem)) rc = -1;
  unsigned char caPem[2048];
  memcpy(caPem, pem, sizeof(pem));
  const auto *empty = reinterpret_cast<const unsigned char *>("");
  const auto *bad = reinterpret_cast<const unsigned char *>("invalid PEM");
  using LocalCertificates::CaPairStatus;
  using LocalCertificates::checkCaPair;
  if (!rc && (checkCaPair(caPem, key, &rng) != CaPairStatus::Valid ||
      checkCaPair(caPem, empty, &rng) != CaPairStatus::Corrupt ||
      checkCaPair(empty, key, &rng) != CaPairStatus::Corrupt ||
      checkCaPair(bad, key, &rng) != CaPairStatus::Corrupt ||
      checkCaPair(caPem, bad, &rng) != CaPairStatus::Corrupt)) rc = -1;
  const int transient[] = {MBEDTLS_ERR_PEM_ALLOC_FAILED, MBEDTLS_ERR_X509_ALLOC_FAILED,
      MBEDTLS_ERR_PK_ALLOC_FAILED, MBEDTLS_ERR_ECP_ALLOC_FAILED, MBEDTLS_ERR_ASN1_ALLOC_FAILED,
      MBEDTLS_ERR_X509_INVALID_EXTENSIONS + MBEDTLS_ERR_ASN1_ALLOC_FAILED,
      MBEDTLS_ERR_ECP_RANDOM_FAILED, MBEDTLS_ERR_PK_FEATURE_UNAVAILABLE, -1};
  for (int error : transient)
    if (LocalCertificates::caValidationStatus(error, true) != CaPairStatus::Retry ||
        LocalCertificates::caValidationStatus(error, false) != CaPairStatus::Retry) rc = -1;
  if (LocalCertificates::caValidationStatus(MBEDTLS_ERR_X509_INVALID_FORMAT + MBEDTLS_ERR_ASN1_OUT_OF_DATA,
      true) != CaPairStatus::Corrupt) rc = -1;
  printf("Transient CA validation errors preserve CA: %s\n", rc ? "FAIL" : "OK");
  mbedtls_x509_crt parsed;
  mbedtls_x509_crt_init(&parsed);
  if (!rc) rc = LocalCertificates::parseCa(&parsed, pem);
  mbedtls_x509_crt_free(&parsed);
  mbedtls_x509_crt_init(&parsed);
  if (!rc && LocalCertificates::parseCa(&parsed, (const unsigned char *)"") == 0) rc = -1;
  mbedtls_x509_crt_free(&parsed);
  mbedtls_x509_crt_init(&parsed);
  if (!rc && LocalCertificates::parseCa(&parsed, (const unsigned char *)"not a certificate") == 0) rc = -1;
  mbedtls_x509_crt_free(&parsed);
  const char *dns = argc > 1 ? argv[1] : "test.local";
  unsigned char ip[4];
  if (inet_pton(AF_INET, argc > 2 ? argv[2] : "192.168.1.42", ip) != 1) rc = -1;
  mbedtls_x509_san_list san[2] = {};
  san[0].node.type = MBEDTLS_X509_SAN_DNS_NAME;
  san[0].node.san.unstructured_name.p = (unsigned char *)dns;
  san[0].node.san.unstructured_name.len = strlen(dns);
  san[0].next = &san[1];
  san[1].node.type = MBEDTLS_X509_SAN_IP_ADDRESS;
  san[1].node.san.unstructured_name.p = ip;
  san[1].node.san.unstructured_name.len = sizeof(ip);
  if (!rc) rc = LocalCertificates::generate(false, "CN=test,O=HIT-SunSpec-Proxy", caName,
      "20260101000000", "20280331000000", &leaf, &ca, san, &rng, pem, sizeof(pem), key, sizeof(key));
  if (!rc && !save("leaf.pem", pem)) rc = -1;
  if (!rc && !save("leaf.key", key)) rc = -1;
  mbedtls_x509_crt_init(&parsed);
  if (!rc) rc = mbedtls_x509_crt_parse(&parsed, pem, strlen((char *)pem) + 1);
  printf("mbedtls_x509_crt_parse(leaf only): %s (rc=%d)\n", rc ? "FAIL" : "OK", rc);
  mbedtls_x509_crt_free(&parsed);
  if (!rc && checkCaPair(caPem, key, &rng) != CaPairStatus::Corrupt) rc = -1;
  printf("CA pair checks (valid, missing, corrupt, mismatched): %s\n", rc ? "FAIL" : "OK");
  mbedtls_pk_free(&leaf);
  mbedtls_pk_init(&leaf);
  if (!rc) rc = LocalCertificates::generate(false, "CN=test,O=HIT-SunSpec-Proxy", "CN=test,O=HIT-SunSpec-Proxy",
      "20260101000000", "20280331000000", &leaf, nullptr, san, &rng, pem, sizeof(pem), key, sizeof(key), true);
  if (!rc && !save("fallback.pem", pem)) rc = -1;
  printf("Plain self-signed fallback generation: %s (rc=%d)\n", rc ? "FAIL" : "OK", rc);
  mbedtls_pk_free(&leaf);
  mbedtls_pk_free(&ca);
  mbedtls_ctr_drbg_free(&rng);
  mbedtls_entropy_free(&entropy);
  printf("CA + leaf generation: %s (rc=%d)\n", rc ? "FAIL" : "OK", rc);
  return rc ? 1 : 0;
}
