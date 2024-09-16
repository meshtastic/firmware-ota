/*
    Meshtastic Factory Firmware
    Based on chegewara example for IDF:
   https://github.com/chegewara/esp32-OTA-over-BLE Ported to Arduino ESP32 by
   Claes Hallberg Licence: MIT OTA Bluetooth example between ESP32 (using NimBLE
   Bluetooth stack) and iOS swift (CoreBluetooth framework) Tested withh NimBLE
   v 1.3.1, iOS 14, ESP32 core 1.06
*/
#include "NimBLEDevice.h"
#include "esp_ota_ops.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <Preferences.h>
#include <esp_task_wdt.h>
Preferences preferences;

/*------------------------------------------------------------------------------
  BLE instances & variables
  ----------------------------------------------------------------------------*/
BLEServer *pServer = NULL;
BLECharacteristic *pTxCharacteristic;
BLECharacteristic *pOtaCharacteristic;

bool deviceConnected = false;
bool oldDeviceConnected = false;

String fileExtension = "";

#define SERVICE_UUID "4FAFC201-1FB5-459E-8FCC-C5C9C331914B"
#define CHARACTERISTIC_TX_UUID "62ec0272-3ec5-11eb-b378-0242ac130003"
#define CHARACTERISTIC_OTA_UUID "62ec0272-3ec5-11eb-b378-0242ac130005"

// #define MAX_BLOCKSIZE_FOR_BT 512

/*------------------------------------------------------------------------------
  OTA instances & variables
  ----------------------------------------------------------------------------*/
static esp_ota_handle_t otaHandler = 0;
static const esp_partition_t *update_partition = NULL;

uint8_t txValue = 0;
int bufferCount = 0;
size_t ota_size = 0;
size_t total_received = 0;
uint8_t chunkCount = 1;
bool downloadFlag = false;

class MyServerCallbacks : public BLEServerCallbacks {

  void onConnect(NimBLEServer *pServer, ble_gap_conn_desc *desc) {
    Serial.println("*** App connected");
    // BLE Power settings. P9 = max power +9db
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_CONN_HDL0, ESP_PWR_LVL_P9);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_CONN_HDL1, ESP_PWR_LVL_P9);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9);

    Serial.println(NimBLEAddress(desc->peer_ota_addr).toString().c_str());
    /*    We can use the connection handle here to ask for different connection
       parameters. Args: connection handle, min connection interval, max
       connection interval latency, supervision timeout. Units; Min/Max
       Intervals: 1.25 millisecond increments. Latency: number of intervals
       allowed to skip. Timeout: 10 millisecond increments, try for 5x interval
       time for best results.
    */
    pServer->updateConnParams(desc->conn_handle, 12, 12, 2, 100);
    deviceConnected = true;
  }

  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
    downloadFlag = false;
    Serial.println("*** App disconnected");
  }
};

/*------------------------------------------------------------------------------
  BLE Peripheral callback(s)
  ----------------------------------------------------------------------------*/

class otaCallback : public BLECharacteristicCallbacks {

  void cleanUp() {
    downloadFlag = false;
    ota_size = 0;
    total_received = 0;
  }

  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string rxData = pCharacteristic->getValue();

    if (!downloadFlag && ota_size == 0) {
      // Check if the string starts with "OTA_SIZE:"
      Serial.printf("Waiting for OTA size: %s\n\r", rxData.c_str());
      if (rxData.rfind("OTA_SIZE:", 0) == 0) {
        // Strip away "OTA_SIZE:"
        std::string sizeStr = rxData.substr(9);

        // Convert the remaining string to an integer
        ota_size = std::stoul(sizeStr);
        Serial.printf("OTA size: %zd\n\r", ota_size);
      }
      return;
    }

    bufferCount++;
    total_received += rxData.length() * chunkCount;
    u_int8_t firstByte = rxData[0]; // print first byte as hex
    Serial.printf("Progress R: %d T: %d C: %d B: %x\n\r", total_received,
                  ota_size, rxData.length(), firstByte);

    if (!downloadFlag && ota_size > 0) {
      // First BLE bytes have arrived

      update_partition = esp_partition_find_first(
          ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, "app");
      assert(update_partition != NULL);

      Serial.printf("Writing to partition subtype %d at offset 0x%x \n\r",
                    update_partition->subtype, update_partition->address);

      // esp_ota_begin can take a while to complete as it erase the flash
      // partition (3-5 seconds) so make sure there's no timeout on the client
      // side that triggers before that.
      esp_task_wdt_init(10, false);
      vTaskDelay(5);

      if (esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &otaHandler) !=
          ESP_OK) {
        cleanUp();
        return;
      }
      downloadFlag = true;
    }

    if (bufferCount >= 1 || rxData.length() > 0) {
      if (esp_ota_write(otaHandler, (uint8_t *)rxData.c_str(),
                        rxData.length()) != ESP_OK) {
        Serial.println("Error: write to flash failed");
        cleanUp();
        return;
      } else {
        bufferCount = 1;
        // Notify the app so next batch can be sent
        pTxCharacteristic->setValue(&txValue, 1);
        pTxCharacteristic->notify();
      }

      // check if this was the last data chunk? If so, finish the OTA update
      if (ota_size == total_received) {
        Serial.println("Final byte arrived");
        // Final chunk arrived. Now check that
        // the length of total file is correct
        if (esp_ota_end(otaHandler) != ESP_OK) {
          Serial.println("OTA end failed ");
          cleanUp();
          return;
        }

        // Clear download flag and restart the ESP32 if the firmware
        // update was successful
        Serial.println("Set Boot partion");
        if (ESP_OK == esp_ota_set_boot_partition(update_partition)) {
          esp_ota_end(otaHandler);
          cleanUp();
          preferences.begin("meshtastic", false);
          // reset reboot counter to zero.
          preferences.putUInt("rebootCounter", 0);
          preferences.end();
          Serial.println("Restarting...");
          sleep(2);
          esp_restart();
          return;
        } else {
          // Something whent wrong, the upload was not successful
          Serial.println("Upload Error");
          cleanUp();
          esp_ota_end(otaHandler);
          return;
        }
      }
    } else {
      cleanUp();
    }
  }
};

const char *getDeviceName() {
  uint8_t dmac[6];
  assert(esp_efuse_mac_get_default(dmac) == ESP_OK);
  // Meshtastic_ab3c
  static char name[20];
  sprintf(name, "Meshtastic_%02x%02x", dmac[4], dmac[5]);
  Serial.printf("BLE name: %s\n", name);
  return name;
}

void setup() {
  /* We explicitly don't want to do call randomSeed,
  // as that triggers the esp32 core to use a less secure pseudorandom function.
  uint32_t seed = esp_random();
  randomSeed(seed);
  */
  Serial.begin(115200);
  Serial.print("\n\n//\\ E S H T /\\ S T / C Failsafe OTA\n\n");
  Serial.printf("ESP32 Chip model = %d\n", ESP.getChipRevision());
  Serial.printf("This chip has %d MHz\n", ESP.getCpuFreqMHz());
  Serial.printf("Total heap: %d\n", ESP.getHeapSize());
  Serial.printf("Free heap: %d\n", ESP.getFreeHeap());
  Serial.printf("Total PSRAM: %d\n", ESP.getPsramSize());
  Serial.printf("Free PSRAM: %d\n", ESP.getFreePsram());
  nvs_stats_t nvs_stats;
  auto res = nvs_get_stats(NULL, &nvs_stats);
  assert(res == ESP_OK);
  Serial.printf(
      "NVS: UsedEntries %d, FreeEntries %d, AllEntries %d, NameSpaces %d\n",
      nvs_stats.used_entries, nvs_stats.free_entries, nvs_stats.total_entries,
      nvs_stats.namespace_count);
  preferences.begin("meshtastic", false);
  uint32_t rebootCounter = preferences.getUInt("rebootCounter", 0);
  uint8_t hwven = preferences.getUInt("hwVendor", 0);
  String fwrev = preferences.getString("firmwareVersion", "");
  Serial.printf("Number of Device Reboots: %d\n", rebootCounter);
  preferences.end();

  // on next reboot, switch back to app partition in case something goes wrong
  // here
  esp_ota_set_boot_partition(esp_ota_get_next_update_partition(NULL));

  std::string manufacturerData = std::to_string(hwven) + "|" + fwrev.c_str();

  Serial.printf("Hardware Model: %d\n", hwven);
  Serial.printf("Main Firmware Version: %s\n", fwrev.c_str());

  NimBLEDevice::init(getDeviceName());
  NimBLEDevice::setMTU(517);

  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  NimBLEService *pService = pServer->createService(SERVICE_UUID);

  // Create BLE Characteristics inside the service(s)
  pTxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_TX_UUID,
                                                     NIMBLE_PROPERTY::NOTIFY);

  pOtaCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_OTA_UUID, NIMBLE_PROPERTY::WRITE_NR);
  pOtaCharacteristic->setCallbacks(new otaCallback());

  pService->start();

  pServer->getAdvertising()->addServiceUUID(pService->getUUID());
  pServer->getAdvertising()->setManufacturerData(manufacturerData);

  pServer->startAdvertising();
  Serial.println("Waiting a client connection to notify...");
  downloadFlag = false;
}

void loop() {
  if (!deviceConnected && oldDeviceConnected) {
    delay(100);
    pServer->startAdvertising();
    Serial.println("start advertising");
    oldDeviceConnected = deviceConnected;
  }
  if (deviceConnected && !oldDeviceConnected) {
    Serial.println("main loop started");
    oldDeviceConnected = deviceConnected;
  }
}
