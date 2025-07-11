#ifndef __LIBJJ_RPC_SPIFFS_H__
#define __LIBJJ_RPC_SPIFFS_H__

static void spiffs_http_file_upload()
{
        static File file;
        HTTPUpload &upload = http_rpc.upload();

        if (upload.status == UPLOAD_FILE_START) {
                const char *filename = upload.filename.c_str();

                if (file) {
                        pr_err("last file did not close\n");
                        file.close();
                }

                file = SPIFFS.open("/" + upload.filename, FILE_WRITE);
                if (!file) {
                        pr_err("failed to open file: %s\n", filename);
                        http_rpc.send(500, "text/plain", "Failed to open file\n");
                        return;
                }

                pr_dbg("file: %s\n", filename);
        } else if (upload.status == UPLOAD_FILE_WRITE) {
                if (file) {
                        file.write(upload.buf, upload.currentSize);
                }
        } else if (upload.status == UPLOAD_FILE_END) {
                char buf[64] = { };

                if (!file) {
                        http_rpc.send(500, "text/plain", "File not opened\n");
                        return;
                }

                file.close();

                pr_dbg("finished, file %s (%u bytes)\n", upload.filename.c_str(), upload.totalSize);
                snprintf(buf, sizeof(buf), "OK, wrote %u bytes\n", upload.totalSize);
                http_rpc.send(200, "text/plain", buf);
        }
}

void rpc_spiffs_add(void)
{
        if (!have_spiffs)
                return;

        http_rpc.on("/spiffs_stats", HTTP_GET, [](){
                char buf[100] = { };
                size_t c = 0;

                c += snprintf(&buf[c], sizeof(buf) - c, "total_bytes: %zu\n", SPIFFS.totalBytes());
                c += snprintf(&buf[c], sizeof(buf) - c, "used_bytes: %zu\n", SPIFFS.usedBytes());

                http_rpc.send(200, "text/plain", buf);
        });

        http_rpc.on("/spiffs_format", HTTP_GET, [](){
                if (SPIFFS.format())
                        http_rpc.send(200, "text/plain", "OK\n");
                else
                        http_rpc.send(500, "text/plain", "Failed\n");
        });

        http_rpc.on("/spiffs_delete", HTTP_GET, [](){
                if (!http_rpc.hasArg("path")) {
                        http_rpc.send(500, "text/plain", "<path>\n");
                        return;
                }

                String filepath = http_rpc.arg("path");
                if (filepath == "/") {
                        http_rpc.send(500, "text/plain", "Not allow to delete '/'\n");
                        return;
                }

                if (!SPIFFS.exists(filepath)) {
                        http_rpc.send(500, "text/plain", "Not such file or directory\n");
                        return;
                }

                if (!SPIFFS.remove(filepath)) {
                        http_rpc.send(500, "text/plain", "Failed to delete\n");
                        return;
                }

                http_rpc.send(200, "text/plain", "OK\n"); 
        });

        // curl -F "file=@<file>" http://<ip>/spiffs_upload
        http_rpc.on("/spiffs_upload", HTTP_POST, [](){ http_rpc.send(200, "text/plain", ""); }, spiffs_http_file_upload);
}

#endif // __LIBJJ_RPC_SPIFFS_H__