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
  if (!rc && (!LocalCertificates::validCaPair(caPem, key, &rng) ||
      LocalCertificates::validCaPair(caPem, empty, &rng) ||
      LocalCertificates::validCaPair(empty, key, &rng) ||
      LocalCertificates::validCaPair(bad, key, &rng) ||
      LocalCertificates::validCaPair(caPem, bad, &rng))) rc = -1;
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
  if (!rc && LocalCertificates::validCaPair(caPem, key, &rng)) rc = -1;
  printf("CA pair checks (valid, missing, corrupt, mismatched): %s\n", rc ? "FAIL" : "OK");
  mbedtls_pk_free(&leaf);
  mbedtls_pk_free(&ca);
  mbedtls_ctr_drbg_free(&rng);
  mbedtls_entropy_free(&entropy);
  printf("CA + leaf generation: %s (rc=%d)\n", rc ? "FAIL" : "OK", rc);
  return rc ? 1 : 0;
}
