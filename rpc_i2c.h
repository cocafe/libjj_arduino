#ifndef __LIBJJ_RPC_I2C_H__
#define __LIBJJ_RPC_I2C_H__

void rpc_i2c_add(void)
{
        http_rpc.on("/i2cdetect", HTTP_GET, [](){
                char buf[512] = { };
                unsigned bus = 0;

                if (http_rpc.hasArg("bus")) {
                        String data = http_rpc.arg("bus");

                        if (1 != sscanf(data.c_str(), "%u", &bus)) {
                                http_rpc.send(500, "text/plain", "-EINVAL\n");
                                return;
                        }
                }

                // FIXME: did not lock here
                i2cdetect(bus, buf, sizeof(buf));

                http_rpc.send(200, "text/plain", buf);
        });
}

#endif // __LIBJJ_RPC_I2C_H__
