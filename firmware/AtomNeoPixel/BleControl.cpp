#include "BleControl.h"

#include <Arduino.h>
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLESecurity.h>
#include <atomic>

namespace {
constexpr char SERVICE_ID[] = "251c86a0-3990-4b1e-bb96-5ed47d413b60";
constexpr char COMMAND_ID[] = "251c86a1-3990-4b1e-bb96-5ed47d413b60";
constexpr char STATE_ID[] = "251c86a2-3990-4b1e-bb96-5ed47d413b60";
constexpr uint32_t WINDOW_MS = 60000;
std::atomic<bool> connected{false};
std::atomic<bool> advertising{false};
std::atomic<uint32_t> windowStart{0};
std::atomic<bool> windowEnabled{false};
uint32_t nextAdvertisingTry = 0;
BLECharacteristic *stateCharacteristic = nullptr;
QueueHandle_t commands = nullptr;
LedState currentState = DEFAULT_STATE;

void publishState(const LedState &state) {
  uint8_t bytes[STATE_SIZE];
  encodeState(state, bytes);
  stateCharacteristic->setValue(bytes, sizeof(bytes));
}

class ConnectionCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *) override {
    connected = true;
    advertising = false;
  }
  void onDisconnect(BLEServer *) override {
    connected = false;
  }
};

class SecurityCallbacks : public BLESecurityCallbacks {
  bool onSecurityRequest() override {
    return BleControl::windowOpen();
  }
};

class CommandCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *characteristic) override {
    const String value = characteristic->getValue();
    if (decodeState(reinterpret_cast<const uint8_t *>(value.c_str()), value.length(), currentState)) {
      xQueueOverwrite(commands, &currentState);
      // Reads, writes and subscription updates are serialized by the GATT task.
      publishState(currentState);
      stateCharacteristic->notify();
    }
  }
};

ConnectionCallbacks connectionCallbacks;
SecurityCallbacks securityCallbacks;
CommandCallbacks commandCallbacks;
}

void BleControl::begin(const char *name, const LedState &initial) {
  commands = xQueueCreate(1, sizeof(LedState));
  configASSERT(commands);
  BLEDevice::init(name);
  BLESecurity::setAuthenticationMode(true, false, true);
  BLESecurity::setCapability(ESP_IO_CAP_NONE);
  BLESecurity::setInitEncryptionKey();
  BLESecurity::setRespEncryptionKey();
  BLEDevice::setSecurityCallbacks(&securityCallbacks);

  auto *server = BLEDevice::createServer();
  server->setCallbacks(&connectionCallbacks);
  server->advertiseOnDisconnect(false);
  auto *service = server->createService(SERVICE_ID);
  auto *command = service->createCharacteristic(COMMAND_ID, BLECharacteristic::PROPERTY_WRITE);
  command->setAccessPermissions(ESP_GATT_PERM_WRITE_ENCRYPTED);
  command->setCallbacks(&commandCallbacks);
  stateCharacteristic = service->createCharacteristic(
      STATE_ID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  stateCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED);
  auto *subscription = new BLE2902;
  subscription->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);
  stateCharacteristic->addDescriptor(subscription);
  currentState = initial;
  publishState(currentState);
  service->start();
  auto *advertisement = BLEDevice::getAdvertising();
  BLEAdvertisementData payload;
  payload.setFlags(ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);
  payload.setCompleteServices(BLEUUID(SERVICE_ID));
  advertisement->setAdvertisementData(payload);
  BLEAdvertisementData scanResponse;
  scanResponse.setName(name);
  advertisement->setScanResponseData(scanResponse);
  advertisement->setScanResponse(true);
}

void BleControl::openWindow() {
  windowStart = millis();
  nextAdvertisingTry = windowStart;
  windowEnabled = true;
}

bool BleControl::windowOpen() {
  return windowEnabled && millis() - windowStart < WINDOW_MS;
}

bool BleControl::isConnected() {
  return connected;
}

void BleControl::tick() {
  if (!windowOpen()) {
    windowEnabled = false;
    if (advertising.exchange(false) && !connected) {
      BLEDevice::getAdvertising()->stop();
    }
  } else if (!connected && static_cast<int32_t>(millis() - nextAdvertisingTry) >= 0
             && !advertising.exchange(true)) {
    if (!BLEDevice::getAdvertising()->start()) {
      advertising = false;
      nextAdvertisingTry = millis() + 500;
    }
  }
}

bool BleControl::takeCommand(LedState &state) {
  return xQueueReceive(commands, &state, 0) == pdTRUE;
}
