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

static bool domeJsonExpectChar(const char *&p, char expected, String &errMsg)
{
    domeJsonSkipWs(p);
    if (*p != expected)
    {
        errMsg = String("expected '") + expected + "'";
        return false;
    }
    p++;
    return true;
}

static bool domeJsonParseLiteral(const char *&p, const char *literal)
{
    domeJsonSkipWs(p);
    size_t len = strlen(literal);
    if (strncmp(p, literal, len) != 0) return false;
    p += len;
    return true;
}

static bool domeJsonParseBool(const char *&p, bool &out)
{
    if (domeJsonParseLiteral(p, "true"))
    {
        out = true;
        return true;
    }
    if (domeJsonParseLiteral(p, "false"))
    {
        out = false;
        return true;
    }
    return false;
}
