#ifndef __LIBJJ_OBDII_H__
#define __LIBJJ_OBDII_H__

#include <stdint.h>

#include "can.h"

extern "C" {

static SemaphoreHandle_t obd_isotp_recv_done;
static uint8_t is_isotp_waiting;

struct obd_pid {
        const char             *name;
        uint8_t                 pid;
        uint8_t                 dlen;
        uint32_t                ts_last_recv;
#ifdef OBDII_DEBUG_QUERY_MS 
        uint32_t                ts_last_send;
        int32_t                 query_ms;
#endif
        double                  value;
        double                  (*compute)(uint8_t *);
};

typedef struct obd_pid obd_pid_t;

static unsigned obd_metric_expire_ms = 1000;

#define OBD_PID_DUMMY                   (0xff)

#ifdef OBDII_DEBUG_QUERY_MS
#define OBD_PID(_name, _pid, _len, _cb)                                 \
static __unused obd_pid_t obd_##_name = {                \
        .name = __stringify(_name),                                     \
        .pid = _pid,                                                    \
        .dlen = _len,                                                   \
        .ts_last_recv = 0,                                              \
        .ts_last_send = 0,                                              \
        .query_ms = 0,                                                  \
        .value = NAN,                                                   \
        .compute = _cb,                                                 \
}
#else
#define OBD_PID(_name, _pid, _len, _cb)                                 \
static __unused obd_pid_t obd_##_name = {                \
        .name = __stringify(_name),                                     \
        .pid = _pid,                                                    \
        .dlen = _len,                                                   \
        .ts_last_recv = 0,                                              \
        .value = NAN,                                                   \
        .compute = _cb,                                                 \
}
#endif

#define COMBINE_2BYTE(b)                (((uint32_t)((b)[0]) << 8) + b[1])

static double simple_one_byte(uint8_t *b)
{
        return b[0];
}

static double simple_two_byte(uint8_t *b)
{
        return COMBINE_2BYTE(b);
}

static double one_byte_percent(uint8_t *b)
{
        return 100.0 / 255.0 * b[0];
}

static double one_byte_tempC(uint8_t *b)
{
        return b[0] - 40;
}

static double two_byte_percent(uint8_t *b)
{
        return 100.0 / 255.0 * COMBINE_2BYTE(b);
}

// %
static double engine_load_compute(uint8_t *b)
{
        return ((double)b[0] * 100) / 255;
}

// %
static double fuel_trim_compute(uint8_t *b)
{
        return ((100.0 / 128.0) * b[0]) - 100;
}

// kPa
static double fuel_pressure_compute(uint8_t *b)
{
        return 3 * b[0];
}

// rpm
static double engine_rpm_compute(uint8_t *b)
{
        return COMBINE_2BYTE(b) / 4.0;
}

static double timing_advance_compute(uint8_t *b)
{
        return (((double)b[0]) / 2.0) - 64;
}

static double intake_air_tempC_compute(uint8_t *b)
{
        return b[0] - 40;
}

// grams/s
static double maf_air_flow_rate_compute(uint8_t *b)
{
        return COMBINE_2BYTE(b) / 100.0;
}

// kPa (relative to manifold vacuum)
static double fuel_rail_pressure_compute(uint8_t *b)
{
        return 0.079 * COMBINE_2BYTE(b);
}

// kPa
static double fuel_rail_gauge_pressure_compute(uint8_t *b)
{
        return 10.0 * COMBINE_2BYTE(b);
}

static double commanded_afr_compute(uint8_t *b)
{
        return (2.0 / 65536) * COMBINE_2BYTE(b);
}

static double catalyst_tempC_compute(uint8_t *b)
{
        return COMBINE_2BYTE(b) / 10.0 - 40;
}

// V
static double control_module_voltage_compute(uint8_t *b)
{
        return COMBINE_2BYTE(b) / 1000.0;
}

// kPa
static double abs_evap_pressure_compute(uint8_t *b)
{
        return COMBINE_2BYTE(b) / 1000.0;
}

// kPa
static double gpf_diff_pressure_compute(uint8_t *b)
{
        return COMBINE_2BYTE(b) / 256.0;
}

static double gpf_tempC_compute(uint8_t *b)
{
        return (COMBINE_2BYTE(b) / 10.0) - 40.0;
}

static double oxygen_sensor_stft_compute(uint8_t *b)
{
        if (b[1] == 0xFF)
                return NAN;

        return (100.0 / 128.0) * b[1] - 100.0;
}

static double oxygen_sensor_afr_compute(uint8_t *b)
{
        return (COMBINE_2BYTE(b) * 2) / 65536.0;
}

OBD_PID(engine_load,                    0x04, 1, engine_load_compute);
OBD_PID(engine_coolant_tempC,           0x05, 1, one_byte_tempC);
OBD_PID(fuel_trim_short_term1,          0x06, 1, fuel_trim_compute);
OBD_PID(fuel_trim_long_term1,           0x07, 1, fuel_trim_compute);
OBD_PID(fuel_trim_short_term2,          0x08, 1, fuel_trim_compute);
OBD_PID(fuel_trim_long_term2,           0x09, 1, fuel_trim_compute);
OBD_PID(fuel_pressure,                  0x0a, 1, fuel_pressure_compute);
OBD_PID(intake_manifold_abs_pressure,   0x0b, 1, simple_one_byte);
OBD_PID(engine_rpm,                     0x0c, 2, engine_rpm_compute);
OBD_PID(vehicle_speed,                  0x0d, 1, simple_one_byte);
OBD_PID(timing_advance,                 0x0e, 1, timing_advance_compute);
OBD_PID(intake_air_tempC,               0x0f, 1, intake_air_tempC_compute);

OBD_PID(maf_air_flow_rate,              0x10, 2, maf_air_flow_rate_compute);
OBD_PID(throttle_position,              0x11, 1, one_byte_percent);
OBD_PID(oxygen_sensor2_stft,            0x15, 2, oxygen_sensor_stft_compute);
OBD_PID(engine_run_time_sec,            0x1f, 2, simple_two_byte);
OBD_PID(fuel_rail_pres,                 0x22, 2, fuel_rail_pressure_compute);
OBD_PID(fuel_rail_gauge_pres,           0x23, 2, fuel_rail_gauge_pressure_compute);
OBD_PID(oxygen_sensor1_afr,             0x24, 4, oxygen_sensor_afr_compute);
OBD_PID(commanded_egr,                  0x2c, 1, one_byte_percent);
OBD_PID(commanded_evap_purge,           0x2e, 1, one_byte_percent);
OBD_PID(fuel_tank_level,                0x2f, 1, one_byte_percent);

OBD_PID(catalyst_tempC,                 0x3c, 2, catalyst_tempC_compute);

OBD_PID(control_module_voltage,         0x42, 2, control_module_voltage_compute);
OBD_PID(engine_abs_load,                0x43, 2, two_byte_percent);
OBD_PID(commanded_afr,                  0x44, 2, commanded_afr_compute);
OBD_PID(relative_throttle_pos,          0x45, 1, one_byte_percent);
OBD_PID(ambient_air_tempC,              0x46, 1, one_byte_tempC);
OBD_PID(abs_throttle_pos_B,             0x47, 1, one_byte_percent);
OBD_PID(abs_throttle_pos_D,             0x49, 1, one_byte_percent);
OBD_PID(abs_throttle_pos_E,             0x4a, 1, one_byte_percent);
OBD_PID(commanded_throttle_actuator,    0x4c, 1, one_byte_percent);

OBD_PID(abs_evap_pressure,              0x53, 2, abs_evap_pressure_compute);
OBD_PID(throttle_pedal_pos,             0x5a, 1, one_byte_percent);
OBD_PID(engine_oil_tempC,               0x5c, 1, one_byte_tempC);

OBD_PID(gpf_diff_pressure,              0x7a, 2, gpf_diff_pressure_compute);
OBD_PID(gpf_tempC,                      0x7c, 2, gpf_tempC_compute);

static obd_pid_t *obd_pid_bkt[0xff];

static int can_fc_frame_send(uint32_t can_id, uint8_t fs, uint8_t bs, uint8_t st)
{
        uint8_t buf[8] = { };
        struct can_hdr_fc *hdr = (struct can_hdr_fc *)buf;

        hdr->frame_type = CAN_FRAME_FC;
        hdr->status = fs;
        hdr->blk_size = bs;
        hdr->st = st;

        return can_dev->send(can_id, sizeof(buf), buf);
}

static int can_fc_cts_send(uint32_t can_id)
{
        return can_fc_frame_send(can_id, CAN_FC_STATUS_CTS, CAN_FC_BS_ALL, CAN_FC_ST_FAST);
}

static int obd_pid_can_frame_input(can_frame_t *f)
{
        uint32_t now = esp32_millis();
        obd_pid_t *p;
        uint32_t can_id = f->id;
        uint32_t dlc = f->dlc;
        uint8_t mode = f->data[0];
        uint8_t pid = f->data[1];
        uint8_t *pdata = &f->data[2];
        uint8_t dlc_want;

        if (mode != 0x41)
                return -EINVAL;

        if (pid == OBD_PID_DUMMY) {
                pr_err("receive PID 0x%02x, which is not expected\n", pid);
                return -ENOTSUP;
        }

        p = obd_pid_bkt[pid];
        if (!p) {
                pr_verbose("unsupported or disabled pid 0x%02x\n", pid);
                return -ENOTSUP;
        }

        //
        // OBD-II PAYLOAD: <MODE> <PID> <DATAn...>
        //

        dlc_want = 1 + 1 + p->dlen;
        if (dlc != dlc_want) {
                pr_dbg("metric %s: from 0x%04lx length %lu != %hhu\n",
                       p->name,
                       can_id,
                       dlc,
                       dlc_want);
                return -EINVAL;
        }

        if (p->compute)
                p->value = p->compute(pdata);

        p->ts_last_recv = now;
#ifdef OBDII_DEBUG_QUERY_MS
        p->query_ms = now - p->ts_last_send;
#endif

        return 0;
}

static inline void obd_can_frame_input(can_frame_t *f)
{
        obd_pid_can_frame_input(f);
}

static __unused void obd_can_isotp_frame_input(can_frame_t *f)
{
        static union {
                uint8_t data[sizeof(can_frame_t) + 256];
                can_frame_t mf; // assembled multi-frame
        } assemble = { };
        static size_t mf_pos = 0;
        static size_t mf_dlc_want = 0;
        uint8_t frame_type;

        frame_type = f->data[0] >> 4;
        switch (frame_type) {
        case CAN_FRAME_SF: {
                struct can_hdr_sf *sf = (struct can_hdr_sf *)&f->data;
                uint8_t obd_frame_dlc = sf->dlc; // not can dlc here
                memmove(&f->data[0], &f->data[1], obd_frame_dlc);
                f->dlc = obd_frame_dlc;

                obd_can_frame_input(f);

                break;
        }

        case CAN_FRAME_FF: {
                __le16 _hdr = be16toh(*(__be16 *)(&f->data[0]));
                struct can_hdr_ff *hdr = (struct can_hdr_ff *)&_hdr;
                can_frame_t *mf = &assemble.mf;
                uint8_t dlc = CAN_FRAME_DLC - sizeof(struct can_hdr_ff);

                memset(&assemble, 0x00, sizeof(assemble));

                mf_dlc_want = hdr->dlc;
                mf_pos = 0;

                mf->id = f->id;
                memcpy(&mf->data[mf_pos], &f->data[sizeof(struct can_hdr_ff)], dlc);
                mf_pos += dlc;

                can_fc_cts_send(f->id - 8);

                break;
        }

        case CAN_FRAME_CF: {
                can_frame_t *mf = &assemble.mf;
                uint8_t dlc = CAN_FRAME_DLC - sizeof(struct can_hdr_cf);

                if (mf_dlc_want == 0) {
                        pr_err("not in multiple frame receiving\n");
                        break;
                }

                if (f->id != mf->id) {
                        pr_err("different id received in CF\n");
                        break;
                }

                memcpy(&mf->data[mf_pos], &f->data[sizeof(struct can_hdr_cf)], dlc);
                mf_pos += dlc;

                if (mf_pos >= mf_dlc_want) {
                        mf->dlc = mf_pos;
                        obd_can_frame_input(mf);

                        if (READ_ONCE(is_isotp_waiting)) {
                                xSemaphoreGive(obd_isotp_recv_done);
                        }

                        mf_dlc_want = 0;
                        mf_pos = 0;
                }

                break;
        }

        case CAN_FRAME_FC:
                break;

        default:
                pr_err("unknown can frame type: 0x%02x\n", frame_type);
                break;
        }
}

static void obd_can_frame_recv(can_frame_t *f)
{
        switch (f->id) {
        case 0x7E8 ... 0x7EF:
                obd_can_isotp_frame_input(f);
                break;

        default:
                break;
        }
}

static int obd_isotp_flow_wait(unsigned ms)
{
        int err = 0;

        WRITE_ONCE(is_isotp_waiting, 1);
        if (xSemaphoreTake(obd_isotp_recv_done, pdMS_TO_TICKS(ms)) == pdFALSE) {
                pr_dbg("failed to wait isotp flow done\n");
                err = -ETIMEDOUT;
        }
        WRITE_ONCE(is_isotp_waiting, 0);

        return err;
}

// XXX: need lock if access by multiple threads
static __unused int obd_pid_query_send(obd_pid_t *p)
{
        uint32_t now = esp32_millis();
        uint8_t payload[8] = { };
        int err;

        if (unlikely(p->pid == OBD_PID_DUMMY))
                return 0;

        payload[0] = 0x02;
        payload[1] = 0x01;
        payload[2] = p->pid;

        if (obd_metric_expire_ms && !isnan(p->value)) {
                if (now - p->ts_last_recv >= obd_metric_expire_ms) {
                        pr_dbg("pid 0x%02x expired\n", p->pid);
                        p->value = NAN;
                }
        }

#ifdef OBDII_DEBUG_QUERY_MS
        p->ts_last_send = now;
#endif

        err = can_dev->send(CAN_ID_BROADCAST, 8, payload);
        if (unlikely(err))
                return err;

        if (unlikely(p->dlen > 6)) {
                obd_isotp_flow_wait(75);
        }

        return 0;
}

static __unused int obd_pid_init(obd_pid_t **list, size_t cnt)
{
        obd_isotp_recv_done = xSemaphoreCreateBinary();

        for (size_t i = 0; i < cnt; i++) {
                obd_pid_t *p = list[i];
                obd_pid_bkt[p->pid] = p;
                pr_info("pid 0x%02x enabled\n", p->pid);
        }

        if (can_dev) {
                can_recv_cb_register(obd_can_frame_recv);
        }

        return 0;
}

} // C

#endif // __LIBJJ_OBDII_H__