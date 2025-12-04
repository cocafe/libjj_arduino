#ifndef __LIBJJ_ESP32_UART_H__
#define __LIBJJ_ESP32_UART_H__

#include <stdint.h>

#include <hal/uart_types.h>

enum {
        SERIAL_MODE_5N1,
        SERIAL_MODE_6N1,
        SERIAL_MODE_7N1,
        SERIAL_MODE_8N1,
        SERIAL_MODE_5N2,
        SERIAL_MODE_6N2,
        SERIAL_MODE_7N2,
        SERIAL_MODE_8N2,
        SERIAL_MODE_5E1,
        SERIAL_MODE_6E1,
        SERIAL_MODE_7E1,
        SERIAL_MODE_8E1,
        SERIAL_MODE_5E2,
        SERIAL_MODE_6E2,
        SERIAL_MODE_7E2,
        SERIAL_MODE_8E2,
        SERIAL_MODE_5O1,
        SERIAL_MODE_6O1,
        SERIAL_MODE_7O1,
        SERIAL_MODE_8O1,
        SERIAL_MODE_5O2,
        SERIAL_MODE_6O2,
        SERIAL_MODE_7O2,
        SERIAL_MODE_8O2,
        NUM_SERIAL_MODES,
};

enum {
        UART_TYPE_UART,
        UART_TYPE_UART_IRDA,
        UART_TYPE_RS485_HALF,
        UART_TYPE_RS485_COLLISION_DETECT,
        UART_TYPE_RS485_APP_CTRL,
        NUM_UART_TYPES,
};

uint32_t serial_mode_value_convert[NUM_SERIAL_MODES] = {
        [SERIAL_MODE_5N1] = SERIAL_5N1,
        [SERIAL_MODE_6N1] = SERIAL_6N1,
        [SERIAL_MODE_7N1] = SERIAL_7N1,
        [SERIAL_MODE_8N1] = SERIAL_8N1,
        [SERIAL_MODE_5N2] = SERIAL_5N2,
        [SERIAL_MODE_6N2] = SERIAL_6N2,
        [SERIAL_MODE_7N2] = SERIAL_7N2,
        [SERIAL_MODE_8N2] = SERIAL_8N2,
        [SERIAL_MODE_5E1] = SERIAL_5E1,
        [SERIAL_MODE_6E1] = SERIAL_6E1,
        [SERIAL_MODE_7E1] = SERIAL_7E1,
        [SERIAL_MODE_8E1] = SERIAL_8E1,
        [SERIAL_MODE_5E2] = SERIAL_5E2,
        [SERIAL_MODE_6E2] = SERIAL_6E2,
        [SERIAL_MODE_7E2] = SERIAL_7E2,
        [SERIAL_MODE_8E2] = SERIAL_8E2,
        [SERIAL_MODE_5O1] = SERIAL_5O1,
        [SERIAL_MODE_6O1] = SERIAL_6O1,
        [SERIAL_MODE_7O1] = SERIAL_7O1,
        [SERIAL_MODE_8O1] = SERIAL_8O1,
        [SERIAL_MODE_5O2] = SERIAL_5O2,
        [SERIAL_MODE_6O2] = SERIAL_6O2,
        [SERIAL_MODE_7O2] = SERIAL_7O2,
        [SERIAL_MODE_8O2] = SERIAL_8O2,
};

uint32_t uart_type_value_convert[NUM_UART_TYPES] = {
        [UART_TYPE_UART]                      = UART_MODE_UART,
        [UART_TYPE_UART_IRDA]                 = UART_MODE_IRDA,
        [UART_TYPE_RS485_HALF]                = UART_MODE_RS485_HALF_DUPLEX,
        [UART_TYPE_RS485_COLLISION_DETECT]    = UART_MODE_RS485_COLLISION_DETECT,
        [UART_TYPE_RS485_APP_CTRL]            = UART_MODE_RS485_APP_CTRL,
};

static const char *str_serial_modes[NUM_SERIAL_MODES] = {
        [SERIAL_MODE_5N1] = "5N1",
        [SERIAL_MODE_6N1] = "6N1",
        [SERIAL_MODE_7N1] = "7N1",
        [SERIAL_MODE_8N1] = "8N1",
        [SERIAL_MODE_5N2] = "5N2",
        [SERIAL_MODE_6N2] = "6N2",
        [SERIAL_MODE_7N2] = "7N2",
        [SERIAL_MODE_8N2] = "8N2",
        [SERIAL_MODE_5E1] = "5E1",
        [SERIAL_MODE_6E1] = "6E1",
        [SERIAL_MODE_7E1] = "7E1",
        [SERIAL_MODE_8E1] = "8E1",
        [SERIAL_MODE_5E2] = "5E2",
        [SERIAL_MODE_6E2] = "6E2",
        [SERIAL_MODE_7E2] = "7E2",
        [SERIAL_MODE_8E2] = "8E2",
        [SERIAL_MODE_5O1] = "5O1",
        [SERIAL_MODE_6O1] = "6O1",
        [SERIAL_MODE_7O1] = "7O1",
        [SERIAL_MODE_8O1] = "8O1",
        [SERIAL_MODE_5O2] = "5O2",
        [SERIAL_MODE_6O2] = "6O2",
        [SERIAL_MODE_7O2] = "7O2",
        [SERIAL_MODE_8O2] = "8O2",
};

static const char *str_uart_types[NUM_UART_TYPES] = {
        [UART_TYPE_UART]                      = "uart",
        [UART_TYPE_UART_IRDA]                 = "uart_irda",
        [UART_TYPE_RS485_HALF]                = "rs485_half_duplex",
        [UART_TYPE_RS485_COLLISION_DETECT]    = "rs485_collision_detect",
        [UART_TYPE_RS485_APP_CTRL]            = "rs485_app_ctrl",
};

struct uart_port_cfg { 
        uint8_t uart_type;
        uint8_t serial_mode;
        uint32_t baudrate;
        struct {
                int8_t tx;
                int8_t rx;
                int8_t rede;
        } pin;
};

int esp32_uart_port_init(HardwareSerial *ser, struct uart_port_cfg *cfg)
{
        if (cfg->serial_mode >= NUM_SERIAL_MODES)
                return -EINVAL;

        if (cfg->uart_type >= NUM_UART_TYPES)
                return -EINVAL;

        ser->begin(cfg->baudrate, serial_mode_value_convert[cfg->serial_mode], cfg->pin.rx, cfg->pin.tx);
        delay(10);

        if (cfg->pin.rede >= 0) {
                pinMode(cfg->pin.rede, OUTPUT);

                if (!ser->setPins(-1, -1, -1, cfg->pin.rede)) {
                        pr_err("failed to set rede pins\n");
                        return -EIO;
                }
        }

        if (!ser->setMode((SerialMode)uart_type_value_convert[cfg->uart_type])) {
                pr_err("failed to set duplex mode\n");
                return -EFAULT;
        }

        return 0;
}

#endif // __LIBJJ_ESP32_UART_H__