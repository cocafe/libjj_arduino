#ifndef __LIBJJ_RACECHRONO_BLE_H__
#define __LIBJJ_RACECHRONO_BLE_H__

#include <stdint.h>

#include <esp_bt.h>

#include <NimBLEDevice.h>

#include "logging.h"
#include "esp32_utils.h"
#include "esp32_bt.h"
#include "can.h"

#ifndef BLE_DEFAULT_MAC
#define BLE_DEFAULT_MAC                 ESP_MAC_EFUSE_FACTORY
#endif

static const uint16_t RC_SERVICE_UUID = 0x1ff8;

// RaceChrono uses two BLE characteristics:
// 1) 0x02 to request which PIDs to send, and how frequently
// 2) 0x01 to be notified of data received for those PIDs
static const uint16_t RC_CHARACTERISTIC_UUID_CANBUS     = 0x1;
static const uint16_t RC_CHARACTERISTIC_DATALEN_CANBUS  = 12; // 4 id + 8 data

static const uint16_t RC_CHARACTERISTIC_UUID_PID        = 0x2;
static const uint16_t RC_CHARACTERISTIC_DATALEN_PID     = 8;

static const uint16_t RC_CHARACTERISTIC_UUID_GPS        = 0x3;
static const uint16_t RC_CHARACTERISTIC_DATALEN_GPS     = 20;

static const uint16_t RC_CHARACTERISTIC_UUID_GPSTIME    = 0x4;
static const uint16_t RC_CHARACTERISTIC_DATALEN_GPSTIME = 3;

struct ble_cfg_conn {
        uint16_t intv_min_us;   // scale to 1.25ms unit
        uint16_t intv_max_us;   // scale to 1.25ms unit
        uint16_t timeout_ms;    // need scale to 10ms unit
        uint8_t latency;        // number of intervals allowed to skip, power save purpose
        uint8_t dle;            // BLE4.2 DLE, 0 for default 27 bytes, 1 for 251 bytes
};

struct ble_cfg {
        char devname[30];
        uint8_t enabled;
        uint8_t update_hz;
        uint8_t tx_power;
        uint8_t tx_power_idle;
        uint8_t have_canbus;
        uint8_t have_gps;
        struct ble_cfg_conn conn;
};

struct ble_ctx {
        NimBLEServer *server;
        NimBLEService *service;
        NimBLEAdvertising *advertising;
        NimBLECharacteristic *char_canbus;
        NimBLECharacteristic *char_pid_req;
        NimBLECharacteristic *char_gps;
        NimBLECharacteristic *char_gpstime;
        uint16_t conn_hdl;
        char *devname;
        struct ble_cfg *cfg;
};

static struct ble_ctx rc_ble;

static uint8_t rc_deny_all = 0;
static uint8_t rc_allow_all = 0;

static uint8_t rc_ready_to_send = 0;

static unsigned rc_hz_overall = 0;
static unsigned rc_hz_counter = 0;

struct ble_ctx *rc_ble_get(void)
{
        return &rc_ble;
}

static void rc_ready_to_send_timer(void *arg)
{
        if (ble_is_connected) {
                rc_ready_to_send = 1;
        } else {
                rc_ready_to_send = 0;
                rc_hz_counter = 0;
                rc_hz_overall = 0;
        }

        pr_info("ready to send: %u\n", rc_ready_to_send);
}

static esp_timer_handle_t rc_ready_timer;
static const esp_timer_create_args_t rc_ready_timer_args = {
        .callback = rc_ready_to_send_timer,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "rc_ready",
};

static void rc_hz_counter_timer(void *arg)
{
        rc_hz_overall = rc_hz_counter;
        rc_hz_counter = 0;
}

static esp_timer_handle_t rc_hz_cnt_timer;
static const esp_timer_create_args_t rc_hz_cnt_timer_args = {
        .callback = rc_hz_counter_timer,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "rc_hzcnt",
};

static char *ble_device_name_generate(char *str)
{
        char buf[32] = "RaceChrono";
        uint8_t mac[6];
        static char dev_name[64];

        if (str) {
                memset(buf, '\0', sizeof(buf));
                strncpy(buf, str, sizeof(buf) < strlen(str) ? sizeof(buf) : strlen(str));
        }

        if (esp32_mac_get(BLE_DEFAULT_MAC, mac)) {
                pr_err("failed to get BT mac address\n");
                snprintf(dev_name, sizeof(dev_name), "%s ??:??:??", buf);
        } else {
                snprintf(dev_name, sizeof(dev_name), "%s %02x:%02x:%02x", buf, mac[3], mac[4], mac[5]);
        }

        return dev_name;
}

int rc_ble_tx_power_set(enum ble_txpwr_t tx_power)
{
        if (tx_power >= NUM_BLE_TXPWR_LEVELS || tx_power < 0 || tx_power >= ARRAY_SIZE(ble_txpwr_to_esp_value)) {
                pr_err("invalid tx power %d\n", tx_power);
                return -EINVAL;
        }

        if (ESP_OK != esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ble_txpwr_to_esp_value[tx_power])) {
                pr_err("esp_ble_tx_power_set() failed\n");
                return -EFAULT;
        }

        pr_info("BLE tx power set to %s\n", str_ble_txpwr[tx_power]);

        return 0;
}

void __unused rc_ble_gps_send(uint8_t *data, size_t len)
{
        struct ble_ctx *ctx = &rc_ble;

        ctx->char_gps->setValue(data, len);
        ctx->char_gps->notify();

        rc_hz_counter++;
}

void __unused rc_ble_gpstime_send(uint8_t *data, size_t len)
{
        struct ble_ctx *ctx = &rc_ble;

        ctx->char_gpstime->setValue(data, len);
        ctx->char_gpstime->notify();

        rc_hz_counter++;
}

void __unused rc_ble_gpstime_data_set(uint8_t *data, size_t len)
{
        struct ble_ctx *ctx = &rc_ble;

        ctx->char_gpstime->setValue(data, len);
}

static void __rc_ble_can_frame_send(struct ble_ctx *ctx, can_frame_t *f)
{
        static uint32_t lock = 0;
        uint8_t *buf;

        // CAN ID litten endian
        // buffer[0] = pid & 0xFF;
        // buffer[1] = (pid >> 8) & 0xFF;
        // buffer[2] = (pid >> 16) & 0xFF;
        // buffer[3] = (pid >> 24) & 0xFF;

        // payload format: 4bytes ID + data sequences

        buf = (uint8_t *)&f->id;

        while (!esp_cpu_compare_and_set(&lock, 0, 1)) {
                taskYIELD();
        }

        ctx->char_canbus->setValue(buf, 4 + f->dlc);
        ctx->char_canbus->notify();

        WRITE_ONCE(lock, 0);
}

void __unused rc_ble_can_frame_send(can_frame_t *f, struct can_rlimit_node *rlimit)
{
        if (unlikely(!rc_ready_to_send))
                return;

        if (unlikely(rc_deny_all))
                return;

        if (unlikely(rc_allow_all))
                goto send;

#ifdef CONFIG_HAVE_CAN_RLIMIT
        if (is_can_id_ratelimited(rlimit, CAN_RLIMIT_TYPE_RC, esp32_millis())) {
                return;
        }
#endif

send:
        __rc_ble_can_frame_send(&rc_ble, f);

        cnt_can_ble_send++;
        rc_hz_counter++;

        ble_activity = 1;
}

void __unused rc_ble_can_frame_send_ratelimited(can_frame_t *f)
{
        static struct can_rlimit_node *rlimit = NULL;

#ifdef CONFIG_HAVE_CAN_RLIMIT
        rlimit = can_ratelimit_get(f->id);
#endif

        rc_ble_can_frame_send(f, rlimit);
}

static void rc_can_rlimit_set_all(int update_hz)
{
#ifdef CONFIG_HAVE_CAN_RLIMIT
        can_ratelimit_set_all(CAN_RLIMIT_TYPE_RC, update_hz);
#endif
}

static void nimble_conn_param_update(NimBLEServer *server, struct ble_cfg_conn *cfg, uint16_t conn_hdl)
{
        unsigned intv_min = 12;
        unsigned intv_max = 24;
        unsigned latency = 0;
        unsigned timeout = 180;

        if (cfg->intv_min_us) {
                intv_min = cfg->intv_min_us * 100 / 125 / 1000;
        }

        if (cfg->intv_max_us) {
                intv_max = cfg->intv_max_us * 100 / 125 / 1000;
        }

        if (cfg->latency) {
                latency = cfg->latency;
        }

        if (cfg->timeout_ms) {
                timeout = cfg->timeout_ms / 10;
        }

        pr_info("try conn params: intv_min: %u intv_max: %u latency: %u timeout: %u\n",
                intv_min, intv_max, latency, timeout);
        server->updateConnParams(conn_hdl, intv_min, intv_max, latency, timeout);

        if (cfg->dle) {
                unsigned dle = cfg->dle;

                if (dle >= 251)
                        dle = 251;

                pr_info("set DLE: %u\n", dle);
                server->setDataLen(conn_hdl, dle);
        }
}

static void rc_conn_param_update_timer_cb(void *arg)
{
        if (!ble_is_connected)
                return;

        nimble_conn_param_update(rc_ble.server, &rc_ble.cfg->conn, rc_ble.conn_hdl);
}

static esp_timer_handle_t rc_conn_param_update_timer;
static const esp_timer_create_args_t rc_conn_param_update_timer_args = {
        .callback = rc_conn_param_update_timer_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "ble_conn_param_timer",
};

class ServerCallbacks : public NimBLEServerCallbacks
{
        void onConnect(NimBLEServer *server, NimBLEConnInfo &connInfo) override
        {
                ble_is_connected = 1;
                rc_ble.conn_hdl = connInfo.getConnHandle();

                pr_info("ble client (%s) connected, conn: %u\n", connInfo.getAddress().toString().c_str(), connInfo.getConnHandle());

                ble_event_clear(BLE_EVENT_DISCONNECTED);
                ble_event_post(BLE_EVENT_CONNECTED);

                if (esp_timer_is_active(rc_ready_timer)) {
                        if (ESP_OK != esp_timer_stop(rc_ready_timer))
                                pr_err("rc ready timer failed to stop\n");
                }

                esp_timer_start_once(rc_ready_timer, 6ULL * 1000 * 1000);
                pr_dbg("rc ready timer started\n");

                if (esp_timer_is_active(rc_conn_param_update_timer)) {
                        if (ESP_OK != esp_timer_stop(rc_conn_param_update_timer))
                                pr_err("rc conn param update timer failed to stop\n");
                }

                esp_timer_start_once(rc_conn_param_update_timer, 5ULL * 1000 * 1000);
                pr_dbg("rc conn param update timer started\n");

                if (rc_ble.cfg->tx_power_idle < ARRAY_SIZE(ble_txpwr_to_esp_value) && 
                    rc_ble.cfg->tx_power < ARRAY_SIZE(ble_txpwr_to_esp_value)) {
                        rc_ble_tx_power_set((enum ble_txpwr_t)rc_ble.cfg->tx_power);
                }
        }

        void onDisconnect(NimBLEServer *server, NimBLEConnInfo &connInfo, int reason) override
        {
                pr_info("ble client disconnected, start advertising\n");

                ble_is_connected = 0;
                rc_ble.conn_hdl = 0;

                ble_event_clear(BLE_EVENT_CONNECTED);
                ble_event_post(BLE_EVENT_DISCONNECTED);

                // delay to cancel ready
                if (!esp_timer_is_active(rc_ready_timer)) {
                        esp_timer_start_once(rc_ready_timer, 2ULL * 1000 * 1000);
                }

                NimBLEDevice::startAdvertising();

                rc_can_rlimit_set_all(-1);

                if (rc_ble.cfg->tx_power_idle < ARRAY_SIZE(ble_txpwr_to_esp_value)) {
                        rc_ble_tx_power_set((enum ble_txpwr_t)rc_ble.cfg->tx_power_idle);
                }
        }

        void onMTUChange(uint16_t MTU, NimBLEConnInfo &connInfo) override
        {
                pr_info("ble MTU updated: conn: %u MTU: %u\n", connInfo.getConnHandle(), MTU);
        }

        void onConnParamsUpdate(NimBLEConnInfo& connInfo) override 
        {
                pr_info("ble conn params updated: intv: %u latency: %u timeout: %u MTU: %u\n",
                        connInfo.getConnInterval(),
                        connInfo.getConnLatency(),
                        connInfo.getConnTimeout(),
                        connInfo.getMTU());
        }

        void onPhyUpdate(NimBLEConnInfo& connInfo, uint8_t txPhy, uint8_t rxPhy) override
        {
                pr_info("ble phy updated: tx: %u rx: %u\n", txPhy, rxPhy);
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

static void rc_char_pid_on_write(const uint8_t *data, uint16_t len)
{
        // The protocol implemented in this file is based on
        // https://github.com/aollin/racechrono-ble-diy-device

        if (len < 1) {
                pr_err("received corrupted or unknown request\n");
                return;
        }

        switch (data[0]) {
        // will be called once before calling allow one or more pid
        case 0:
                if (len == 1) {
                        pr_info("deny all pid\n");
                        rc_deny_all = 1;
                        rc_can_rlimit_set_all(-1);
                } else {
                        pr_err("invalid deny all pid command\n");
                }

                break;

        case 1:
                if (len == 3) {
                        uint16_t update_intv_ms = data[1] << 8 | data[2];
                        pr_info("allow all pid, update_intv_ms: %u\n", update_intv_ms);
                        rc_deny_all = 0;
                        rc_allow_all = 1;
                } else {
                        pr_err("invalid allow all pid command\n");
                }

                break;

        // allow one more pid
        case 2:
                if (len == 7) {
                        uint16_t __attribute__((unused)) update_intv_ms = data[1] << 8 | data[2];
                        uint32_t pid = data[3] << 24 | data[4] << 16 | data[5] << 8 | data[6];

                        rc_deny_all = 0;
                        rc_allow_all = 0;

#ifdef CONFIG_HAVE_CAN_RLIMIT
                        struct can_rlimit_node *rlimit;
                        unsigned update_hz = rc_ble.cfg->update_hz;

                        rlimit = can_ratelimit_get(pid);

                        if (!rlimit) {
                                rlimit = can_ratelimit_add(pid);
                        }

                        can_ratelimit_set(rlimit, CAN_RLIMIT_TYPE_RC, update_hz);

                        pr_info("allow pid: 0x%03lx update_intv_ms: %u\n", pid, update_hz ? update_hz_to_ms(update_hz) : 0);
#else
                        pr_info("allow pid: 0x%03lx\n", pid);
#endif
                } else {
                        pr_err("invalid allow pid command\n");
                }

                break;
        }
}

class CharacteristicCallbacks : public NimBLECharacteristicCallbacks
{
        void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override
        {
                NimBLEAttValue value = pCharacteristic->getValue();
                rc_char_pid_on_write(value.data(), value.length());
        }

        void onSubscribe(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo& connInfo, uint16_t subValue) override
        {
                pr_info("conn %d subscribe state changed: %u\n", connInfo.getConnHandle(), subValue);
        }

        void onStatus(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo, int code) override
        {
                pr_info("conn %d status: %d\n", connInfo.getConnHandle(), code);
        }
} rc_char_pid_cbs;

static void racechrono_nimble_init(struct ble_ctx *ctx)
{
        NimBLECharacteristic *char_can = NULL;
        NimBLECharacteristic *char_pid = NULL;
        NimBLECharacteristic *char_gps = NULL;
        NimBLECharacteristic *char_gpstime = NULL;

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

        NimBLEService *service = server->createService((const NimBLEUUID)RC_SERVICE_UUID);

        if (ctx->cfg->have_canbus) {
                char_can = service->createCharacteristic((const NimBLEUUID)RC_CHARACTERISTIC_UUID_CANBUS, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
                char_pid = service->createCharacteristic((const NimBLEUUID)RC_CHARACTERISTIC_UUID_PID, NIMBLE_PROPERTY::WRITE);
                char_pid->setCallbacks(&rc_char_pid_cbs);
        }

        if (ctx->cfg->have_gps) {
                char_gps = service->createCharacteristic((const NimBLEUUID)RC_CHARACTERISTIC_UUID_GPS, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
                char_gpstime = service->createCharacteristic((const NimBLEUUID)RC_CHARACTERISTIC_UUID_GPSTIME, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
        }

        service->start();

        NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
        advertising->setName(ctx->devname);
        advertising->addServiceUUID(service->getUUID());
        advertising->enableScanResponse(true);
        advertising->start();

        ctx->server = server;
        ctx->service = service;
        ctx->advertising = advertising;

        if (ctx->cfg->have_canbus) {
                ctx->char_pid_req = char_pid;
                ctx->char_canbus = char_can;
        }

        if (ctx->cfg->have_gps) {
                ctx->char_gps = char_gps;
                ctx->char_gpstime = char_gpstime;
        }

        pr_info("done, devname: \"%s\"\n", ctx->devname);
}

static int __unused racechrono_ble_init(struct ble_cfg *cfg)
{
        struct ble_ctx *ctx = &rc_ble;

        if (!cfg)
                return -EINVAL;

        if (ctx->cfg)
                return -EALREADY;

        if (!cfg->enabled)
                return 0;

        if (!cfg->have_canbus && !cfg->have_gps) {
                pr_err("both canbus and gps are disabled\n");
                return -EINVAL;
        }

        ble_event_init();

        esp32_stack_print("before nimble init");

        ctx->cfg = cfg;
        ctx->devname = ble_device_name_generate(cfg->devname);

        if (esp_timer_create(&rc_ready_timer_args, &rc_ready_timer) != ESP_OK) {
                pr_err("failed to create timer\n");
                return -ENOMEM;
        }

        if (esp_timer_create(&rc_conn_param_update_timer_args, &rc_conn_param_update_timer) != ESP_OK) {
                pr_err("failed to create conn param update timer\n");
                return -ENOMEM;
        }

        if (esp_timer_create(&rc_hz_cnt_timer_args, &rc_hz_cnt_timer) == ESP_OK) {
                esp_timer_start_periodic(rc_hz_cnt_timer, 1ULL * 1000 * 1000);
        } else {
                pr_err("failed to create hz counter timer\n");
        }

        racechrono_nimble_init(ctx);

        if (cfg->tx_power_idle < ARRAY_SIZE(ble_txpwr_to_esp_value)) {
                rc_ble_tx_power_set((enum ble_txpwr_t)cfg->tx_power_idle);
        } else if (cfg->tx_power < ARRAY_SIZE(ble_txpwr_to_esp_value)) {
                rc_ble_tx_power_set((enum ble_txpwr_t)cfg->tx_power);
        }

        if (can_dev) {
                can_recv_cb_register(rc_ble_can_frame_send);
        }

        esp32_stack_print("after nimble init");

        return 0;
}

#endif // __LIBJJ_RACECHRONO_BLE_H__