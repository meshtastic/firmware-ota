/*
    Meshtastic Factory Firmware
    Based on chegewara example for IDF: https://github.com/chegewara/esp32-OTA-over-BLE
    Ported to Arduino ESP32 by Claes Hallberg
    Licence: MIT
    OTA Bluetooth example between ESP32 (using NimBLE Bluetooth stack) and iOS swift (CoreBluetooth framework)
    Tested withh NimBLE v 1.3.1, iOS 14, ESP32 core 1.06
*/
#include "NimBLEDevice.h"
#include "esp_ota_ops.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <Preferences.h>
#include <esp_task_wdt.h>
Preferences preferences;

/*------------------------------------------------------------------------------
  BLE instances & variables
  ----------------------------------------------------------------------------*/
BLEServer* pServer = NULL;
BLECharacteristic * pTxCharacteristic;
BLECharacteristic * pOtaCharacteristic;

bool deviceConnected = false;
bool oldDeviceConnected = false;

String fileExtension = "";

#define SERVICE_UUID                  "4FAFC201-1FB5-459E-8FCC-C5C9C331914B"
#define CHARACTERISTIC_TX_UUID        "62ec0272-3ec5-11eb-b378-0242ac130003"
#define CHARACTERISTIC_OTA_UUID       "62ec0272-3ec5-11eb-b378-0242ac130005"

// #define MAX_BLOCKSIZE_FOR_BT 512

/*------------------------------------------------------------------------------
  OTA instances & variables
  ----------------------------------------------------------------------------*/
static esp_ota_handle_t otaHandler = 0;
static const esp_partition_t *update_partition = NULL;

uint8_t     txValue = 0;
int         bufferCount = 0;
bool        downloadFlag = false;

class MyServerCallbacks: public BLEServerCallbacks {
    
    void onConnect(NimBLEServer* pServer, ble_gap_conn_desc* desc) {
#ifdef DEBUG
      Serial.println("*** App connected");
#endif
      // BLE Power settings. P9 = max power +9db
      esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_CONN_HDL0, ESP_PWR_LVL_P9);
      esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_CONN_HDL1, ESP_PWR_LVL_P9);
      esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);
      esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9);
#ifdef DEBUG

      Serial.println(NimBLEAddress(desc->peer_ota_addr).toString().c_str());
#endif
      /*    We can use the connection handle here to ask for different connection parameters.
            Args: connection handle, min connection interval, max connection interval
            latency, supervision timeout.
            Units; Min/Max Intervals: 1.25 millisecond increments.
            Latency: number of intervals allowed to skip.
            Timeout: 10 millisecond increments, try for 5x interval time for best results.
      */
      pServer->updateConnParams(desc->conn_handle, 12, 12, 2, 100);
      deviceConnected = true;
    }

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      downloadFlag    = false;
#ifdef DEBUG 
      Serial.println("*** App disconnected");
#endif
    }
};

/*------------------------------------------------------------------------------
  BLE Peripheral callback(s)
  ----------------------------------------------------------------------------*/
  
class otaCallback: public BLECharacteristicCallbacks {
    
    void onWrite(BLECharacteristic *pCharacteristic) 
    {
      std::string rxData = pCharacteristic->getValue();
      bufferCount++;
     
      if (!downloadFlag) 
      {
        // First BLE bytes have arrived
        


        update_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, "app");
        assert(update_partition != NULL);
#ifdef DEBUG 

        Serial.printf("Writing to partition subtype %d at offset 0x%x \n", update_partition->subtype, update_partition->address);
        
#endif        
        // esp_ota_begin can take a while to complete as it erase the flash partition (3-5 seconds) 
        // so make sure there's no timeout on the client side that triggers before that. 
        esp_task_wdt_init(10, false);
        vTaskDelay(5);
        
        if (esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &otaHandler) != ESP_OK) {
          downloadFlag = false;
          return;
        }
        downloadFlag = true;
      }
      
      if (bufferCount >= 1 || rxData.length() > 0) 
      { 
        if(esp_ota_write(otaHandler, (uint8_t *) rxData.c_str(), rxData.length()) != ESP_OK) {
#ifdef DEBUG
          Serial.println("Error: write to flash failed");
#endif          
          downloadFlag = false;
          return;
        } else {
          bufferCount = 1;
#ifdef DEBUG 
          Serial.println("--Data received---");
#endif          
          //Notify the app so next batch can be sent
          pTxCharacteristic->setValue(&txValue, 1);
          pTxCharacteristic->notify();
        }
        
        // check if this was the last data chunk? (normaly the last chunk is 
        // smaller than the maximum MTU size). For improvement: let iOS app send byte 
        // length instead of hardcoding "510"
        if (rxData.length() < 510) // TODO Asumes at least 511 data bytes (@BLE 4.2). 
        {
#ifdef DEBUG          
          Serial.println("Final byte arrived");
#endif          
          // Final chunk arrived. Now check that
          // the length of total file is correct
          if (esp_ota_end(otaHandler) != ESP_OK) 
          {
#ifdef DEBUG             
            Serial.println("OTA end failed ");
#endif            
            downloadFlag = false;
            return;
          }
          
          // Clear download flag and restart the ESP32 if the firmware
          // update was successful
#ifdef DEBUG 
          Serial.println("Set Boot partion");
#endif
          if (ESP_OK == esp_ota_set_boot_partition(update_partition)) 
          {
            esp_ota_end(otaHandler);
            downloadFlag = false;
            preferences.begin("meshtastic", false);
            // reset reboot counter to zero.
            preferences.putUInt("rebootCounter", 0);
            preferences.end();
#ifdef DEBUG 
            Serial.println("Restarting...");
#endif
            sleep(2);
            esp_restart();
            return;
          } else {
            // Something whent wrong, the upload was not successful
#ifdef DEBUG             
            Serial.println("Upload Error");
#endif
            downloadFlag = false;
            esp_ota_end(otaHandler);
            return;
          }
        }
      } else {
        downloadFlag = false;
      }
    }
};
const char *getDeviceName()
{
    uint8_t dmac[6];
    assert(esp_efuse_mac_get_default(dmac) == ESP_OK);
    // Meshtastic_ab3c
    static char name[20];
    sprintf(name, "Meshtastic_%02x%02x", dmac[4], dmac[5]);
    return name;
}



void setup() {
  uint32_t seed = esp_random();
#ifdef DEBUG
  Serial.begin(921600);
  Serial.print("\n\n//\\ E S H T /\\ S T / C Failsafe OTA\n\n");
  Serial.printf("ESP32 Chip model = %d\n", ESP.getChipRevision());
  Serial.printf("This chip has %d MHz\n", ESP.getCpuFreqMHz());
  Serial.printf("Setting random seed %u\n", seed);
  Serial.printf("Total heap: %d\n", ESP.getHeapSize());
  Serial.printf("Free heap: %d\n", ESP.getFreeHeap());
  Serial.printf("Total PSRAM: %d\n", ESP.getPsramSize());
  Serial.printf("Free PSRAM: %d\n", ESP.getFreePsram());
#endif
  randomSeed(seed); // ESP docs say this is fairly random
  nvs_stats_t nvs_stats;
  auto res = nvs_get_stats(NULL, &nvs_stats);
  assert(res == ESP_OK);
#ifdef DEBUG
  Serial.printf("NVS: UsedEntries %d, FreeEntries %d, AllEntries %d, NameSpaces %d\n", nvs_stats.used_entries, nvs_stats.free_entries,
              nvs_stats.total_entries, nvs_stats.namespace_count);
#endif
  preferences.begin("meshtastic", false);
  uint32_t rebootCounter = preferences.getUInt("rebootCounter", 0);
#ifdef DEBUG

  Serial.printf("Number of Device Reboots: %d\n", rebootCounter);
#endif
  preferences.end();
  NimBLEDevice::init(getDeviceName());
  NimBLEDevice::setMTU(517);
  
  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  NimBLEService *pService = pServer->createService(SERVICE_UUID);
  
  // Create BLE Characteristics inside the service(s)
  pTxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_TX_UUID,
                      NIMBLE_PROPERTY:: NOTIFY);

  pOtaCharacteristic = pService->createCharacteristic(CHARACTERISTIC_OTA_UUID,
                       NIMBLE_PROPERTY:: WRITE_NR);
  pOtaCharacteristic->setCallbacks(new otaCallback());

  pService->start();

  pServer->getAdvertising()->addServiceUUID(pService->getUUID());
  pServer->getAdvertising()->start();
  
  NimBLEDevice::startAdvertising();
#ifdef DEBUG 
  Serial.println("Waiting a client connection to notify...");
#endif
  downloadFlag = false;
}


void loop() {
  if (!deviceConnected && oldDeviceConnected) {
    delay(100);
    pServer->startAdvertising();
#ifdef DEBUG
    Serial.println("start advertising");
#endif
    oldDeviceConnected = deviceConnected;
  }
  if (deviceConnected && !oldDeviceConnected) {
#ifdef DEBUG    
    Serial.println("main loop started");
#endif
    oldDeviceConnected = deviceConnected;
  }
}
