#ifndef __LIBJJ_MODBUS_H__
#define __LIBJJ_MODBUS_H__

struct modbus_read_req_t {
        union {
                struct {
                        uint8_t slave;
                        uint8_t func;
                        __be16 reg_start;
                        __be16 reg_count;
                        __le16 crc;
                } __attribute__((packed));
                uint8_t raw[8];
        };
} __attribute__((packed));

struct modbus_read_resp_t {
        uint8_t slave;
        uint8_t func;
        uint8_t dlen;
        uint8_t data[];
        // __le16 crc;
} __attribute__((packed));

__le16 modbus_crc16(const uint8_t *buf, int pktlen)
{
        uint16_t crc = 0xFFFF;

        while (pktlen--) {
                crc ^= *buf++;

                for (int i = 0; i < 8; i++)
                {
                        if (crc & 1)
                                crc = (crc >> 1) ^ 0xA001;
                        else
                                crc >>= 1;
                }
        }

        return htole16(crc);
}

int modbus_crc16_check(const uint8_t *buf, int buflen)
{
        uint16_t crc_orig = (buf[buflen - 1] << 8) | (buf[buflen - 2]);
        uint16_t crc_calc = le16toh(modbus_crc16(buf, buflen - 2));
        if (crc_calc != crc_orig) {
                pr_info("crc_calc %04x != crc_orig %04x\n", crc_calc, crc_orig);
                return -EINVAL;
        }

        return 0;
}
#endif // __LIBJJ_MODBUS_H__