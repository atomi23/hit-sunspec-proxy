#pragma once
#include <Arduino.h>

constexpr int BLE_ADDR_TYPE_PUBLIC = 0;
constexpr int BLE_ADDR_TYPE_RANDOM = 1;

struct BLEAddress {
  explicit BLEAddress(const String &) {}
  String toString() const { return {}; }
};

struct BLEUUID {
  explicit BLEUUID(const char *) {}
};

struct BLEAdvertisedDevice {
  bool haveName() const { return false; }
  String getName() const { return {}; }
  BLEAddress getAddress() const { return BLEAddress(""); }
  int getAddressType() const { return BLE_ADDR_TYPE_PUBLIC; }
  int getRSSI() const { return 0; }
};

struct BLEScanResults {
  int getCount() const { return 0; }
  BLEAdvertisedDevice getDevice(int) const { return {}; }
};

struct BLEScan {
  void setActiveScan(bool) {}
  void setInterval(int) {}
  void setWindow(int) {}
  BLEScanResults *start(uint8_t, bool) { return nullptr; }
  void clearResults() {}
};

struct BLERemoteCharacteristic {
  uint16_t getHandle() const { return 0; }
};

struct BLERemoteService {
  BLERemoteCharacteristic characteristic;
  BLERemoteCharacteristic *getCharacteristic(BLEUUID) { return &characteristic; }
};

struct BLEClient {
  bool connected = false;
  BLERemoteService service;
  bool connect(BLEAddress, int, uint32_t) { return connected = true; }
  bool isConnected() const { return connected; }
  void disconnect() { connected = false; }
  BLERemoteService *getService(BLEUUID) { return &service; }
  uint8_t getGattcIf() const { return 0; }
  uint16_t getConnId() const { return 0; }
};

struct BLEDevice {
  static void init(const char *) {}
  static BLEScan *getScan() {
    static BLEScan scan;
    return &scan;
  }
  static BLEClient *createClient() { return new BLEClient; }
};
