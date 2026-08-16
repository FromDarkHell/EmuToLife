#include "fs/fs.h"

bool fs_init() {
    log_dbg("[fs_init] Initializing LittleFS");

    if (!LittleFS.begin()) {
        log_err("[fs_init] Failed to mount LittleFS, formatting...");
        LittleFS.format();
        return LittleFS.begin();
    }

    return true;
}

bool fs_exists(const char* name) {
    return LittleFS.exists(name);
}

const char* fs_read(const char* name, bool terminate) {
    if (name == nullptr) {
        log_err("[fs_read] Filename is NULL");
        return nullptr;
    }

    File file = LittleFS.open(name, "r");
    if (!file) {
        log_err("[fs_read] Failed to open file: %s", name);
        return nullptr;
    }

    size_t size = file.size();
    char* buffer = (char*)malloc(size + (terminate ? 1 : 0));

    size_t bytesRead = file.readBytes(buffer, size);

    if (terminate) {
        buffer[bytesRead] = '\0';
    }

    file.close();
    return buffer;
}

bool fs_write(const char* name, const char* content, bool unsafe) {
    File file = LittleFS.open(name, "w");
    if (!file) {
        return false;
    }

    const size_t size = strlen(content);
    if (!unsafe && size == 0) {
        log_warn("[fs_write] Failed to write %s due to file size safety", name);
        return false;
    }

    const size_t written = file.write(reinterpret_cast<const uint8_t*>(content), size);
    file.flush();

    file.close();

    log_dbg("[fs_write] Writing %d to %s (Written Size: %d)", size, name, written);

    return written == size;
}