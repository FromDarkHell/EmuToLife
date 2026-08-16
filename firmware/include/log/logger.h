#pragma once

#include <Arduino.h>

#define LOG_BUFFER_SIZE 4096 * 2 * 4
#define BAUD_RATE 921600

#define LOG_UART Serial1
#define LOG_UART_TX 17
#define LOG_UART_RX 16

class Logger {
   public:
    void begin();
    void log(const char* level, const char* fmt, va_list args);

    void blinkStatus(int count, int interval_ms);

    const char* getBuffer() const { return logBuffer; }

   private:
    char logBuffer[LOG_BUFFER_SIZE];
    size_t logBufferPos = 0;

    // Expands %b[N] specifiers in fmt, consuming a const uint8_t* per specifier from args.
    // Remaining standard specifiers are passed through untouched for vsnprintf.
    // Returns the number of chars written to out, or -1 if out was too small.
    static int expandHexSpecifiers(char* out, size_t outLen, const char* fmt, va_list args) {
        size_t outPos = 0;

        auto write = [&](char c) -> bool {
            if (outPos + 1 >= outLen) {
                return false;
            }
            out[outPos++] = c;
            return true;
        };

        for (const char* p = fmt; *p != '\0'; ++p) {
            // Check for %b[N]
            if (p[0] == '%' && p[1] == 'b' && p[2] == '[') {
                const char* bracket = p + 3;
                char* end;
                long n = strtol(bracket, &end, 10);
                if (*end == ']' && n > 0) {
                    const uint8_t* buf = va_arg(args, const uint8_t*);
                    for (long i = 0; i < n; ++i) {
                        if (i > 0 && !write(' ')) {
                            return -1;
                        }
                        char hex[3];
                        snprintf(hex, sizeof(hex), "%02X", buf[i]);
                        if (!write(hex[0]) || !write(hex[1])) {
                            return -1;
                        }
                    }
                    p = end;  // advance past ']'
                    continue;
                }
            }

            // Pass everything else through as-is
            if (!write(*p)) {
                return -1;
            }
        }

        out[outPos] = '\0';
        return static_cast<int>(outPos);
    }

    void appendToLogBuffer(const char* msg);
    void sendLogOutput(const char* msg);
};

extern Logger logger;

#ifdef __cplusplus
extern "C" {
#endif

void log_init();
const char* log_get();
void log_dbg(const char* fmt, ...);
void log_warn(const char* fmt, ...);
void log_err(const char* fmt, ...);

#ifdef __cplusplus
}
#endif