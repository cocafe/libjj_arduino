#ifndef __LIBJJ_RACECHRONO_BLE_H__
#define __LIBJJ_RACECHRONO_BLE_H__

#include <stdint.h>

#include <esp_bt.h>

#include <NimBLEDevice.h>

#include "logging.h"
#include "esp32_utils.h"
#include "esp32_bt.h"
#include "can.h"

static const uint16_t RACECHRONO_SERVICE_UUID = 0x1ff8;

// RaceChrono uses two BLE characteristics:
// 1) 0x02 to request which PIDs to send, and how frequently
// 2) 0x01 to be notified of data received for those PIDs
static const uint16_t PID_CHARACTERISTIC_UUID = 0x2;
static const uint16_t CAN_BUS_CHARACTERISTIC_UUID = 0x1;

#ifndef BLE_DEFAULT_MAC
#define BLE_DEFAULT_MAC                 ESP_MAC_EFUSE_FACTORY
#endif

#ifndef BLE_DEFAULT_UPDATE_RATE_HZ
#define BLE_DEFAULT_UPDATE_RATE_HZ      10
#endif

#define BLE_UPDATE_HZ_TO_MS(hz)         (((hz) == 0) ? 0 : (1000 / (hz)))

struct ble_cfg {
        char devname[16];
        uint8_t enabled;
        uint8_t update_hz;
        uint8_t tx_power;
};

struct ble_ctx {
        NimBLEServer *server;
        NimBLEService *service;
        NimBLEAdvertising *advertising;
        NimBLECharacteristic *char_canbus;
        NimBLECharacteristic *char_pid_req;
        char *devname;
        struct ble_cfg *cfg;
};

static struct ble_ctx rc_ble;

static SemaphoreHandle_t lck_ble_send;

static uint8_t racechrono_ble_update_rate_hz = BLE_DEFAULT_UPDATE_RATE_HZ;

// TODO: hash map for PIDs

static char *ble_device_name_generate(char *str)
{
        char buf[32] = "RaceChrono";
        uint8_t mac[6];
        static char dev_name[64];

        if (str) {
                strncpy(buf, str, sizeof(buf) < strlen(str) ? sizeof(buf) : strlen(str));
        }

        if (esp32_mac_get(BLE_DEFAULT_MAC, mac)) {
                pr_err("failed to get BT mac address\n");
                snprintf(dev_name, sizeof(dev_name), "%s ??:??:??", buf);
        } else {
                snprintf(dev_name, sizeof(dev_name), "%s %x:%x:%x", buf, mac[3], mac[4], mac[5]);
        }

        return dev_name;
}

static void __rc_ble_can_frame_send(struct ble_ctx *ctx, uint32_t pid, uint8_t *data, uint8_t len)
{
        if (len > 8)
                len = 8;

        uint8_t buffer[8 + 4] = { 0 };
        buffer[0] = pid & 0xFF;
        buffer[1] = (pid >> 8) & 0xFF;
        buffer[2] = (pid >> 16) & 0xFF;
        buffer[3] = (pid >> 24) & 0xFF;
        memcpy(&buffer[4], data, len);
        ctx->char_canbus->setValue(buffer, 4 + len);
        ctx->char_canbus->notify();
}

static void rc_ble_can_frame_send(can_frame_t *f)
{
        if (!ble_is_connected)
                return;

        xSemaphoreTake(lck_ble_send, portMAX_DELAY);
        __rc_ble_can_frame_send(&rc_ble, f->id, f->data, f->dlc);
        xSemaphoreGive(lck_ble_send);

        cnt_can_ble_send++;

#ifdef CAN_BLE_LED_BLINK
        can_ble_txrx = 1;
#endif
}

class ServerCallbacks : public NimBLEServerCallbacks
{
        void onConnect(NimBLEServer *server, NimBLEConnInfo &connInfo) override
        {
                pr_info("ble client connected (%s)\n", connInfo.getAddress().toString().c_str());
                ble_is_connected = 1;

                /**
                 *  We can use the connection handle here to ask for different connection parameters.
                 *  Args: connection handle, min connection interval, max connection interval
                 *  latency, supervision timeout.
                 *  Units; Min/Max Intervals: 1.25 millisecond increments.
                 *  Latency: number of intervals allowed to skip.
                 *  Timeout: 10 millisecond increments.
                 */
                server->updateConnParams(connInfo.getConnHandle(), 24, 48, 0, 180);
        }

        void onDisconnect(NimBLEServer *server, NimBLEConnInfo &connInfo, int reason) override
        {
                pr_info("ble client disconnected, start advertising\n");
                ble_is_connected = 0;
                NimBLEDevice::startAdvertising();
                // TODO: reset hash map
        }

        void onMTUChange(uint16_t MTU, NimBLEConnInfo &connInfo) override
        {
                pr_info("ble MTU updated: %u for connection ID: %u\n", MTU, connInfo.getConnHandle());
        }

#if 0   // we do not need secure now
        /********************* Security handled here *********************/
        uint32_t onPassKeyDisplay() override
        {
                pr_info("Server Passkey Display\n");
                /**
                 * This should return a random 6 digit number for security
                 *  or make your own static passkey as done here.
                 */
                return 123456;
        }

        void onConfirmPassKey(NimBLEConnInfo &connInfo, uint32_t pass_key) override
        {
                pr_info("The passkey YES/NO number: %" PRIu32 "\n", pass_key);
                /** Inject false if passkeys don't match. */
                NimBLEDevice::injectConfirmPasskey(connInfo, true);
        }

        void onAuthenticationComplete(NimBLEConnInfo &connInfo) override
        {
                /** Check that encryption was successful, if not we disconnect the client */
                if (!connInfo.isEncrypted())
                {
                        NimBLEDevice::getServer()->disconnect(connInfo.getConnHandle());
                        pr_info("encrypt connection failed - disconnecting client\n");
                        return;
                }

                pr_info("secured connection to: %s\n", connInfo.getAddress().toString().c_str());
        }
#endif
} serverCallbacks;

static void racechrono_char_pid_on_write(const uint8_t *data, uint16_t len)
{
        // The protocol implemented in this file is based on
        // https://github.com/aollin/racechrono-ble-diy-device

        if (len < 1) {
                pr_err("received corrupted or unknown request\n");
                return;
        }

        switch (data[0]) {
        case 1: // Allow all CAN PIDs.
                if (len == 3) {
                        uint16_t update_intv_ms = data[1] << 8 | data[2];
                        // allowAllPids(update_intv_ms);
                        pr_info("allow all pid, update_intv_ms: %u\n", update_intv_ms);
                } else {
                        pr_err("invalid allow all pid command\n");
                }

                break;

        case 0: // Deny all CAN PIDs.
                if (len == 1) {
                        // denyAllPids();
                        pr_info("deny all pid\n");
                } else {
                        pr_err("invalid deny all pid command\n");
                }

                break;

        case 2: // Allow one more CAN PID.
                if (len == 7) {
                        uint16_t update_intv_ms = data[1] << 8 | data[2];
                        uint32_t pid = data[3] << 24 | data[4] << 16 | data[5] << 8 | data[6];
                        // allowPid(pid, update_intv_ms);
                        pr_info("allow pid: 0x%03lx update_intv_ms: %u\n", pid, update_intv_ms);
                } else {
                        pr_err("invalid allow pid command\n");
                }

                break;
        }

        // TODO: invalid command counter
}

class CharacteristicCallbacks : public NimBLECharacteristicCallbacks
{
        void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override
        {
                NimBLEAttValue value = pCharacteristic->getValue();
                racechrono_char_pid_on_write(value.data(), value.length());
        }
} rc_char_pid_cbs;

static void racechrono_nimble_init(struct ble_ctx *ctx)
{
        NimBLEDevice::init(ctx->devname);

        /**
         *  2 different ways to set security - both calls achieve the same result.
         *  no bonding, no man in the middle protection, BLE secure connections.
         *
         *  These are the default values, only shown here for demonstration.
         */
        // NimBLEDevice::setSecurityAuth(false, false, true);
        // NimBLEDevice::setSecurityAuth(BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_MITM | BLE_SM_PAIR_AUTHREQ_SC);

        NimBLEServer *server = NimBLEDevice::createServer();
        server->setCallbacks(&serverCallbacks);

        NimBLEService *service = server->createService((const NimBLEUUID)RACECHRONO_SERVICE_UUID);

        NimBLECharacteristic *char_pid = service->createCharacteristic((const NimBLEUUID)PID_CHARACTERISTIC_UUID, NIMBLE_PROPERTY::WRITE);
        char_pid->setCallbacks(&rc_char_pid_cbs);

        NimBLECharacteristic *char_can = service->createCharacteristic((const NimBLEUUID)CAN_BUS_CHARACTERISTIC_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

        service->start();

        NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
        advertising->setName(ctx->devname);
        advertising->addServiceUUID(service->getUUID());
        advertising->enableScanResponse(true);
        advertising->start();

        ctx->server = server;
        ctx->service = service;
        ctx->advertising = advertising;
        ctx->char_pid_req = char_pid;
        ctx->char_canbus = char_can;

        pr_info("done\n");
}

static int __unused racechrono_ble_init(struct ble_cfg *cfg)
{
        struct ble_ctx *ctx = &rc_ble;

        lck_ble_send = xSemaphoreCreateMutex();

        if (!cfg)
                return -EINVAL;

        if (ctx->cfg)
                return -EALREADY;

        if (!cfg->enabled)
                return 0;

        ctx->cfg = cfg;
        ctx->devname = ble_device_name_generate(cfg->devname);

        racechrono_nimble_init(ctx);

        if (cfg->tx_power < ARRAY_SIZE(ble_txpwr_to_esp_value)) {
                if (ESP_OK != esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ble_txpwr_to_esp_value[cfg->tx_power])) {
                        pr_err("esp_ble_tx_power_set() failed\n");
                }
        }

        if (can_dev) {
                can_recv_cb_register(rc_ble_can_frame_send);
        }

        return 0;
}

#endif // __LIBJJ_RACECHRONO_BLE_H__