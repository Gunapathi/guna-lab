#include "ble_link.h"

#include <Arduino.h>
#include <NimBLEDevice.h>

#include "config.h"

// ---------- Bluetooth link ----------
// Callbacks run on the NimBLE task: they copy bytes into a queue or set a flag and return. They
// never touch the motors (spec/core/architecture.md#threading).
struct Write {
  uint8_t len;
  uint8_t data[BLE_WRITE_MAX];
};

static QueueHandle_t writes;
static NimBLECharacteristic* tx;
static volatile bool connected = false;
static volatile bool subscribed = false;
static volatile uint16_t connHandle = BLE_HS_CONN_HANDLE_NONE;

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer*, NimBLEConnInfo& info) override {
    connHandle = info.getConnHandle();
    connected = true;
  }
  // advertising starts again by itself (advertiseOnDisconnect in bleBegin)
  void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int) override {
    connected = false;
    connHandle = BLE_HS_CONN_HANDLE_NONE;
  }
};

class RxCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo&) override {
    const NimBLEAttValue& v = c->getValue();
    Write w;
    w.len = v.length() < (size_t)BLE_WRITE_MAX ? v.length() : BLE_WRITE_MAX;
    memcpy(w.data, v.data(), w.len);
    xQueueSend(writes, &w, 0);   // full: dropped rather than stall the Bluetooth task
  }
};

class TxCallbacks : public NimBLECharacteristicCallbacks {
  void onSubscribe(NimBLECharacteristic*, NimBLEConnInfo&, uint16_t subValue) override {
    if (subValue) subscribed = true;
  }
};

void bleBegin() {
  writes = xQueueCreate(BLE_WRITE_QUEUE, sizeof(Write));
  NimBLEDevice::init(DEVICE_NAME);
  NimBLEDevice::setPower(BLE_TX_DBM);

  NimBLEServer* server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());
  server->advertiseOnDisconnect(true);   // NimBLE 2.x leaves this off
  NimBLEService* nus = server->createService(NUS_SERVICE_UUID);
  nus->createCharacteristic(NUS_RX_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR)
      ->setCallbacks(new RxCallbacks());
  tx = nus->createCharacteristic(NUS_TX_UUID, NIMBLE_PROPERTY::NOTIFY);
  tx->setCallbacks(new TxCallbacks());

  // flags + the 128-bit service UUID fill most of the advert, so the name goes in the scan
  // response; enableScanResponse must come first or setName puts it in the advert
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->enableScanResponse(true);
  adv->setName(DEVICE_NAME);
  adv->addServiceUUID(NUS_SERVICE_UUID);
  adv->start();
}

bool bleConnected() {
  return connected;
}

size_t bleRead(uint8_t* buf, size_t max) {
  Write w;
  if (xQueueReceive(writes, &w, 0) != pdTRUE) return 0;
  size_t n = w.len < max ? w.len : max;
  memcpy(buf, w.data, n);
  return n;
}

bool bleTakeSubscribed() {
  bool s = subscribed;
  subscribed = false;
  return s;
}

void bleSend(const char* line) {
  uint16_t handle = connHandle;
  if (!connected || handle == BLE_HS_CONN_HANDLE_NONE) return;
  uint16_t mtu = NimBLEDevice::getServer()->getPeerMTU(handle);
  size_t chunk = mtu > 3 ? mtu - 3 : 20;   // ATT header; 20 = the default MTU's payload
  size_t len = strlen(line);
  for (size_t at = 0; at < len; at += chunk)
    tx->notify((const uint8_t*)line + at, len - at < chunk ? len - at : chunk);
}
