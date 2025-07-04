#ifndef __LIBJJ_CAN_FRAME_H__
#define __LIBJJ_CAN_FRAME_H__

#include <stdint.h>

extern "C" {

// single frame
#define CAN_FRAME_SF                            (0x00)
// first frame
#define CAN_FRAME_FF                            (0x01)
// consecutive frame
#define CAN_FRAME_CF                            (0x02)
// flow control frame
#define CAN_FRAME_FC                            (0x03)

#define CAN_FLAG_EXTENDED_ID                    BIT_ULL(31)
#define CAN_FLAG_REMOTE_TRANSMISSION_REQ        BIT_ULL(30)
#define CAN_FLAG_ERROR_MSG                      BIT_ULL(29)

#define CAN_ID_BROADCAST                        (0x7df)

#define CAN_FRAME_DLC                           (8)

#define CAN_FC_STATUS_CTS                       (0x00)
#define CAN_FC_STATUS_WAIT                      (0x01)
#define CAN_FC_STATUS_ABORT                     (0x02)

#define CAN_FC_BS_ALL                           (0x00)

#define CAN_FC_ST_FAST                          (0x00)
#define CAN_FC_ST_10MS                          (0x0a)

struct can_hdr_sf {
        uint8_t dlc             : 4,
                frame_type      : 4;
} __attribute__((packed));

struct can_hdr_ff {
        __le16  dlc             : 12,
                frame_type      : 4;
} __attribute__((packed));

struct can_hdr_cf {
        uint8_t index           : 4,
                frame_type      : 4;
} __attribute__((packed));

struct can_hdr_fc {
        uint8_t status          : 4,
                frame_type      : 4;
        uint8_t blk_size;
        uint8_t st;
} __attribute__((packed));

#define CAN_DATA_MAGIC                          (0xC0CAFEEE)

struct can_frame_t {
        __le32 magic;
        __le32 id;
        union {
                struct {
                        uint8_t heartbeat : 1;
                        uint8_t extd      : 1;
                        uint8_t rtr       : 1;
                        uint8_t reserved  : 5;
                };
                uint8_t flags;
        };
        uint8_t dlc;
        uint8_t data[];
} __attribute__((packed));

typedef struct can_frame_t can_frame_t;

} // C

#endif // __LIBJJ_CAN_FRAME_H__