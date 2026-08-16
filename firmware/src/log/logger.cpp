#include "log/logger.h"

Logger logger;

void Logger::begin() {
    // USB serial - always init this first
    Serial.begin(BAUD_RATE);

    // Hardware UART on specified pins
    LOG_UART.begin(BAUD_RATE);
}

void Logger::log(const char* level, const char* fmt, va_list args) {
    char buffer[512];
    unsigned long now = millis();
    int n = snprintf(buffer, sizeof(buffer), "[%s][%lu ms] ", level, now);

    char expandedFmt[512];
    va_list argsCopy;
    va_copy(argsCopy, args);
    expandHexSpecifiers(expandedFmt, sizeof(expandedFmt), fmt, argsCopy);
    va_end(argsCopy);

    vsnprintf(buffer + n, sizeof(buffer) - n, expandedFmt, args);
    strncat(buffer, "\n", sizeof(buffer) - strlen(buffer) - 1);

    sendLogOutput(buffer);
    appendToLogBuffer(buffer);
}

void Logger::blinkStatus(int count, int interval_ms) {
    for (int i = 0; i < count; i++) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(interval_ms);
        digitalWrite(LED_BUILTIN, LOW);
        delay(interval_ms);
    }
}

void Logger::appendToLogBuffer(const char* msg) {
    size_t msgLen = strlen(msg);
    if (msgLen >= LOG_BUFFER_SIZE) {
        msg += (msgLen - LOG_BUFFER_SIZE + 1);
        msgLen = strlen(msg);
    }
    if (logBufferPos + msgLen >= LOG_BUFFER_SIZE) {
        size_t overflow = (logBufferPos + msgLen) - LOG_BUFFER_SIZE + 1;
        memmove(logBuffer, logBuffer + overflow, logBufferPos - overflow);
        logBufferPos -= overflow;
    }
    memcpy(logBuffer + logBufferPos, msg, msgLen);
    logBufferPos += msgLen;
    logBuffer[logBufferPos] = '\0';
}

void Logger::sendLogOutput(const char* msg) {
#ifdef DEBUG
    Serial.print(msg);    // USB CDC
    LOG_UART.print(msg);  // Hardware UART
#endif
}

void log_init() {
    logger.begin();
}
const char* log_get() {
    return logger.getBuffer();
}

void log_dbg(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logger.log("DEBUG", fmt, args);
    va_end(args);
}

void log_warn(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logger.log("WARN", fmt, args);
    va_end(args);
}

void log_err(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logger.log("ERROR", fmt, args);
    va_end(args);
}