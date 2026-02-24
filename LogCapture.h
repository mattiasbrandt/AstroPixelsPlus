#ifndef LOG_CAPTURE_H
#define LOG_CAPTURE_H

// LogCapture.h — Ring buffer log capture for WebSocket broadcast
// Wraps Serial output, tees to both hardware UART and a circular buffer.
// The buffer can be read by the web server to push log lines to the browser.

#include <Arduino.h>

// Ring buffer for captured log lines
// ~4KB: 50 lines x 80 chars average
#define LOG_CAPTURE_MAX_LINES 50
#define LOG_CAPTURE_LINE_LEN  120

class LogCapture : public Print
{
public:
    LogCapture(Print &target) : fTarget(target), fWriteIdx(0), fCount(0), fLinePos(0)
    {
        memset(fLineBuf, 0, sizeof(fLineBuf));
        memset(fRing, 0, sizeof(fRing));
    }

    // Print interface — write single byte
    size_t write(uint8_t c) override
    {
        // Always forward to the real Serial
        fTarget.write(c);

        // Accumulate into current line
        if (c == '\n' || fLinePos >= LOG_CAPTURE_LINE_LEN - 1)
        {
            fLineBuf[fLinePos] = '\0';
            if (fLinePos > 0) // Don't store empty lines
            {
                storeLine(fLineBuf);
            }
            fLinePos = 0;
        }
        else if (c != '\r') // Skip CR
        {
            fLineBuf[fLinePos++] = (char)c;
        }
        return 1;
    }

    // Print interface — write buffer
    size_t write(const uint8_t *buf, size_t size) override
    {
        for (size_t i = 0; i < size; i++)
        {
            write(buf[i]);
        }
        return size;
    }

    // Get count of available lines (up to LOG_CAPTURE_MAX_LINES)
    int lineCount() const
    {
        return fCount < LOG_CAPTURE_MAX_LINES ? fCount : LOG_CAPTURE_MAX_LINES;
    }

    // Get line by index (0 = oldest available)
    // Returns pointer to internal buffer — copy before next write if needed
    const char *getLine(int index) const
    {
        if (index < 0 || index >= lineCount()) return "";
        int total = lineCount();
        // fWriteIdx points to next write slot
        // Oldest line is at (fWriteIdx - total + MAX) % MAX
        int ringIdx = (fWriteIdx - total + index + LOG_CAPTURE_MAX_LINES) % LOG_CAPTURE_MAX_LINES;
        return fRing[ringIdx];
    }

    // Get the latest line index (for detecting new lines)
    uint32_t writeIndex() const { return fWriteIdx; }

    uint32_t totalCount() const { return fCount; }

    const char *getLineByCount(uint32_t count) const
    {
        if (count == 0 || count > fCount) return "";
        if ((fCount - count) >= LOG_CAPTURE_MAX_LINES) return "";
        int ringIdx = (int)((count - 1) % LOG_CAPTURE_MAX_LINES);
        return fRing[ringIdx];
    }

    // Flag: new line was added since last check
    bool hasNewLine(uint32_t &lastIdx) const
    {
        if (fCount != lastIdx)
        {
            lastIdx = fCount;
            return true;
        }
        return false;
    }

    // Get the newest line (last written)
    const char *getNewestLine() const
    {
        if (fCount == 0) return "";
        int idx = (fWriteIdx - 1 + LOG_CAPTURE_MAX_LINES) % LOG_CAPTURE_MAX_LINES;
        return fRing[idx];
    }

private:
    void storeLine(const char *line)
    {
        strncpy(fRing[fWriteIdx], line, LOG_CAPTURE_LINE_LEN - 1);
        fRing[fWriteIdx][LOG_CAPTURE_LINE_LEN - 1] = '\0';
        fWriteIdx = (fWriteIdx + 1) % LOG_CAPTURE_MAX_LINES;
        fCount++;
    }

    Print &fTarget;
    int fWriteIdx;
    uint32_t fCount;

    // Current line accumulator
    char fLineBuf[LOG_CAPTURE_LINE_LEN];
    int fLinePos;

    // Circular buffer of captured lines
    char fRing[LOG_CAPTURE_MAX_LINES][LOG_CAPTURE_LINE_LEN];
};

#endif // LOG_CAPTURE_H
