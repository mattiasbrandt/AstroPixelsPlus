#pragma once
// Shared minimal JSON token helpers for small firmware-side request/template
// parsers. These helpers intentionally reject unicode escapes because the dome
// layout/status contracts use ASCII identifiers and operator text.

#include <Arduino.h>

static inline void domeJsonSkipWs(const char *&p)
{
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
}

static bool domeJsonParseString(const char *&p, String &out, String &errMsg,
                                const char *unicodeErrMsg)
{
    domeJsonSkipWs(p);
    if (*p != '"') { errMsg = "expected string"; return false; }
    p++;
    out = "";
    while (*p)
    {
        char c = *p++;
        if (c == '"') return true;
        if ((unsigned char)c < 0x20)
        {
            errMsg = "string contains control characters";
            return false;
        }
        if (c != '\\')
        {
            out += c;
            continue;
        }

        char esc = *p++;
        if (esc == '"' || esc == '\\' || esc == '/') out += esc;
        else if (esc == 'b') out += '\b';
        else if (esc == 'f') out += '\f';
        else if (esc == 'n') out += '\n';
        else if (esc == 'r') out += '\r';
        else if (esc == 't') out += '\t';
        else if (esc == 'u')
        {
            errMsg = unicodeErrMsg;
            return false;
        }
        else
        {
            errMsg = "invalid string escape";
            return false;
        }
    }
    errMsg = "unterminated string";
    return false;
}
