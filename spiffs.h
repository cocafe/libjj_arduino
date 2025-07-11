#ifndef __LIBJJ_SPIFFS_H__
#define __LIBJJ_SPIFFS_H__

#include <FS.h>
#include <SPIFFS.h>

#include "logging.h"

/* You only need to format SPIFFS the first time you run a
   test or else use the SPIFFS plugin to create a partition
   https://github.com/me-no-dev/arduino-esp32fs-plugin */
#define CONFIG_FORMAT_SPIFFS_IF_FAILED                  true

static uint8_t have_spiffs = 0;

uint8_t *spiffs_file_read_with_malloc(const char *path, int *err, size_t *sz, void *(*alloc)(size_t))
{
        uint8_t *buf = NULL;
        size_t n = 0;
        int _err = 0;

        if (unlikely(!alloc)) {
                if (err)
                        *err = -EINVAL;

                return NULL;
        }

        if (unlikely(!have_spiffs)) {
                if (err)
                        *err = -ENODEV;

                return NULL;
        }

        File file = SPIFFS.open(path, FILE_READ);
        if (!file) {
                _err = -ENOENT;
                goto out;
        }

        if (file.size() == 0) {
                _err = -ENODATA;
                goto close;
        }

        buf = (uint8_t *)alloc(sizeof(uint8_t) * file.size());
        if (!buf) {
                _err = -ENOMEM;
                goto close;
        }

        n = file.read(buf, file.size());
        if (n != file.size()) {
                _err = -EFAULT;
                free(buf);
                buf = NULL;
        }

close:
        file.close();

out:
        if (err)
                *err = _err;

        if (sz)
                *sz = n;

        return buf;
}

uint8_t *spiffs_file_read(const char *path, int *err, size_t *sz)
{
        uint8_t *buf = NULL;
        size_t n = 0;
        int _err = 0;

        if (unlikely(!have_spiffs)) {
                if (err)
                        *err = -ENODEV;
                
                return NULL;
        }

        File file = SPIFFS.open(path, FILE_READ);
        if (!file) {
                _err = -ENOENT;
                goto out;
        }

        if (file.size() == 0) {
                _err = -ENODATA;
                goto close;
        }

        buf = (uint8_t *)malloc(sizeof(uint8_t) * file.size());
        if (!buf) {
                _err = -ENOMEM;
                goto close;
        }

        n = file.read(buf, file.size());
        if (n != file.size()) {
                _err = -EFAULT;
                free(buf);
                buf = NULL;
        }

close:
        file.close();

out:
        if (err)
                *err = _err;

        if (sz)
                *sz = n;

        return buf;
}

int spiffs_file_write(const char *path, uint8_t *buf, size_t bufsz)
{
        size_t n;
        int err = 0;

        if (unlikely(!have_spiffs)) {
                return -ENODEV;
        }

        File file = SPIFFS.open(path, FILE_WRITE);
        if (!file)
                return -ENOENT;
        
        n = file.write(buf, bufsz);
        if (n != bufsz)
                err = -ENOSPC;

        file.close();

        return err;
}

int spiffs_file_delete(const char *path)
{
        if (unlikely(!have_spiffs))
                return -ENODEV;
        
        if (SPIFFS.remove(path))
                return 0;
        
        return -EIO;
}

void spiffs_init(void)
{
        if (!SPIFFS.begin(CONFIG_FORMAT_SPIFFS_IF_FAILED)) {
                pr_err("failed to init spiffs\n");
                have_spiffs = 0;

                return;
        }

        have_spiffs = 1;
}

#endif // __LIBJJ_SPIFFS_H__