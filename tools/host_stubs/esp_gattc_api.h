#pragma once
#include <cstdint>

using esp_err_t = int;
constexpr esp_err_t ESP_OK = 0;
constexpr int ESP_GATT_WRITE_TYPE_NO_RSP = 1;
constexpr int ESP_GATT_AUTH_REQ_NONE = 0;

esp_err_t esp_ble_gattc_write_char(uint8_t, uint16_t, uint16_t, uint16_t len, uint8_t *data, int, int);
