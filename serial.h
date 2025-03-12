#ifndef __LIBJJ_SERIAL_H_
#define __LIBJJ_SERIAL_H_

#include <stdio.h>

#include <Arduino.h>

static void serial_init(unsigned baud_rate)
{
        setbuf(stdout, NULL);
        setbuf(stderr, NULL);

        Serial.begin(baud_rate);

        while(!Serial);
}


#endif // __LIBJJ_SERIAL_H_