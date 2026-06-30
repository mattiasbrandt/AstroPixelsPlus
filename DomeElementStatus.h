#pragma once
// DomeElementStatus.h — persistent operator status for dome layout elements.
//
// This stores only editor/operator availability metadata. It must not gate
// Marcduino command execution or servo routing; runtime no-op behavior remains
// owned by the existing command handlers and wiring configuration.

#include <Arduino.h>
#include <Preferences.h>

#if __has_include("GeneratedDomeLayout.h")
#include "GeneratedDomeLayout.h"
#define DOME_ELEMENT_STATUS_HAS_GENERATED_LAYOUT 1
#else
#define DOME_ELEMENT_STATUS_HAS_GENERATED_LAYOUT 0
#endif

#define DOME_ELEMENT_STATUS_NS "dome_status"
#define DOME_ELEMENT_STATUS_DISABLED_FMT "es_d%d"
#define DOME_ELEMENT_STATUS_REASON_FMT "es_r%d"
#define DOME_ELEMENT_STATUS_META_SCHEMA "es_schema"
#define DOME_ELEMENT_STATUS_META_TEMPLATE "es_tid"
#define DOME_ELEMENT_STATUS_META_REVISION "es_trev"
#define DOME_ELEMENT_STATUS_MAX_REASON_LEN 96
#define DOME_ELEMENT_STATUS_MAX_ELEMENTS 64

typedef String (*DomeElementStatusEscapeFn)(const String &);

struct DomeElementStatusUpdate
{
    int index;
    bool disabled;
    String reason;
};

struct DomeElementStatusSnapshot
{
    bool disabled;
    String reason;
};

#if DOME_ELEMENT_STATUS_HAS_GENERATED_LAYOUT
// The generated layout table is the canonical allowlist once available. The
// generator should keep kDomeLayoutElements in stable template order so NVS
// index keys do not drift between firmware builds for the same template.
static inline int domeElementStatusElementCount()
{
    return (int)DomeLayout::kElementCount;
}

static inline const char *domeElementStatusElementId(int index)
{
    return (index >= 0 && index < domeElementStatusElementCount())
        ? DomeLayout::kElements[index].id
        : nullptr;
}

static inline const char *domeElementStatusTemplateId()
{
    return DomeLayout::kTemplateId;
}

static inline int domeElementStatusSchemaRevision()
{
    return DomeLayout::kSchemaRevision;
}

static inline int domeElementStatusTemplateRevision()
{
    return DomeLayout::kTemplateRevision;
}
#else
// Temporary MK4 allowlist used until GeneratedDomeLayout.h lands. Keep this
// limited to stable canonical element IDs, not aliases, command targets, or
// wiring slots.
static const char *kDomeElementStatusFallbackIds[] = {
    "P1", "P2", "P3", "P4", "P5", "P6", "P7",
    "P8", "P9", "P10", "P11", "P12", "P13", "P14",
    "PP1", "PP2", "PP3", "PP4", "PP5", "PP6",
    "MP",
    "HP1", "HP2", "HP3",
    "FLD", "RLD", "FPSI", "RPSI",
};

static inline int domeElementStatusElementCount()
{
    return (int)(sizeof(kDomeElementStatusFallbackIds) /
                 sizeof(kDomeElementStatusFallbackIds[0]));
}

static inline const char *domeElementStatusElementId(int index)
{
    return (index >= 0 && index < domeElementStatusElementCount())
        ? kDomeElementStatusFallbackIds[index]
        : nullptr;
}

static inline const char *domeElementStatusTemplateId()
{
    return "mr-baddeley-complex-dome-mk4";
}

static inline int domeElementStatusSchemaRevision()
{
    return 1;
}

static inline int domeElementStatusTemplateRevision()
{
    return 1;
}
#endif

static int domeElementStatusIndexOf(const String &id)
{
    for (int i = 0; i < domeElementStatusElementCount(); i++)
    {
        const char *known = domeElementStatusElementId(i);
        if (known && id == known) return i;
    }
    return -1;
}

static inline void domeElementStatusKey(char *out, size_t outSize,
                                        const char *fmt, int index)
{
    snprintf(out, outSize, fmt, index);
}

static bool domeElementStatusValidateReason(String &reason, String &errMsg)
{
    reason.trim();
    if (reason.length() > DOME_ELEMENT_STATUS_MAX_REASON_LEN)
    {
        errMsg = "disabled_reason exceeds 96 characters";
        return false;
    }
    for (size_t i = 0; i < reason.length(); i++)
    {
        unsigned char c = (unsigned char)reason[i];
        if (c < 0x20 || c == 0x7F)
        {
            errMsg = "disabled_reason contains control characters";
            return false;
        }
    }
    return true;
}

static void domeElementStatusSkipWs(const char *&p)
{
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
}

static bool domeElementStatusParseJsonString(const char *&p, String &out,
                                             String &errMsg)
{
    domeElementStatusSkipWs(p);
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
            // IDs and operator reasons are ASCII-only for this endpoint. Refuse
            // unicode escapes instead of accepting text we cannot normalize.
            errMsg = "unicode escapes are not supported";
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

static bool domeElementStatusParseLiteral(const char *&p, const char *literal)
{
    domeElementStatusSkipWs(p);
    size_t len = strlen(literal);
    if (strncmp(p, literal, len) != 0) return false;
    p += len;
    return true;
}

static bool domeElementStatusParseBool(const char *&p, bool &out)
{
    if (domeElementStatusParseLiteral(p, "true"))
    {
        out = true;
        return true;
    }
    if (domeElementStatusParseLiteral(p, "false"))
    {
        out = false;
        return true;
    }
    return false;
}

static bool domeElementStatusExpectChar(const char *&p, char expected,
                                        String &errMsg)
{
    domeElementStatusSkipWs(p);
    if (*p != expected)
    {
        errMsg = String("expected '") + expected + "'";
        return false;
    }
    p++;
    return true;
}

static bool domeElementStatusParseOneElement(const char *&p,
                                             DomeElementStatusUpdate &out,
                                             bool *seenIds,
                                             String &errMsg)
{
    if (!domeElementStatusExpectChar(p, '{', errMsg)) return false;

    bool seenId = false;
    bool seenDisabled = false;
    bool seenReason = false;
    String id;
    String reason;
    bool disabled = false;

    while (true)
    {
        domeElementStatusSkipWs(p);
        if (*p == '}')
        {
            p++;
            break;
        }

        String key;
        if (!domeElementStatusParseJsonString(p, key, errMsg)) return false;
        if (!domeElementStatusExpectChar(p, ':', errMsg)) return false;

        if (key == "id")
        {
            if (seenId) { errMsg = "duplicate id field"; return false; }
            if (!domeElementStatusParseJsonString(p, id, errMsg)) return false;
            seenId = true;
        }
        else if (key == "disabled")
        {
            if (seenDisabled) { errMsg = "duplicate disabled field"; return false; }
            if (!domeElementStatusParseBool(p, disabled))
            {
                errMsg = "disabled must be true or false";
                return false;
            }
            seenDisabled = true;
        }
        else if (key == "disabled_reason")
        {
            if (seenReason) { errMsg = "duplicate disabled_reason field"; return false; }
            domeElementStatusSkipWs(p);
            if (domeElementStatusParseLiteral(p, "null"))
            {
                reason = "";
            }
            else if (!domeElementStatusParseJsonString(p, reason, errMsg))
            {
                return false;
            }
            seenReason = true;
        }
        else
        {
            errMsg = "unknown element-status field: " + key;
            return false;
        }

        domeElementStatusSkipWs(p);
        if (*p == ',')
        {
            p++;
            continue;
        }
        if (*p == '}')
        {
            p++;
            break;
        }
        errMsg = "expected ',' or '}' in element object";
        return false;
    }

    if (!seenId || !seenDisabled)
    {
        errMsg = "element requires id and disabled";
        return false;
    }
    int index = domeElementStatusIndexOf(id);
    if (index < 0)
    {
        errMsg = "unknown dome element id: " + id;
        return false;
    }
    if (seenIds[index])
    {
        errMsg = "duplicate dome element id: " + id;
        return false;
    }
    seenIds[index] = true;

    if (!disabled) reason = "";
    if (!domeElementStatusValidateReason(reason, errMsg)) return false;

    out.index = index;
    out.disabled = disabled;
    out.reason = reason;
    return true;
}

static bool domeElementStatusParseBody(const String &body,
                                       DomeElementStatusUpdate *updates,
                                       int maxUpdates,
                                       int &outCount,
                                       String &errMsg)
{
    outCount = 0;
    int elementCount = domeElementStatusElementCount();
    if (elementCount <= 0 || elementCount > maxUpdates)
    {
        errMsg = "dome element allowlist is unavailable";
        return false;
    }

    bool seenIds[DOME_ELEMENT_STATUS_MAX_ELEMENTS] = {false};
    const char *p = body.c_str();
    if (!domeElementStatusExpectChar(p, '{', errMsg)) return false;

    bool seenElements = false;
    while (true)
    {
        domeElementStatusSkipWs(p);
        if (*p == '}')
        {
            p++;
            break;
        }

        String key;
        if (!domeElementStatusParseJsonString(p, key, errMsg)) return false;
        if (!domeElementStatusExpectChar(p, ':', errMsg)) return false;
        if (key != "elements")
        {
            errMsg = "unknown element-status field: " + key;
            return false;
        }
        if (seenElements)
        {
            errMsg = "duplicate elements field";
            return false;
        }
        seenElements = true;

        if (!domeElementStatusExpectChar(p, '[', errMsg)) return false;
        domeElementStatusSkipWs(p);
        if (*p != ']')
        {
            while (true)
            {
                if (outCount >= maxUpdates)
                {
                    errMsg = "too many element status updates";
                    return false;
                }
                if (!domeElementStatusParseOneElement(p, updates[outCount],
                                                      seenIds, errMsg))
                {
                    return false;
                }
                outCount++;
                domeElementStatusSkipWs(p);
                if (*p == ',')
                {
                    p++;
                    continue;
                }
                if (*p == ']') break;
                errMsg = "expected ',' or ']' in elements array";
                return false;
            }
        }
        if (!domeElementStatusExpectChar(p, ']', errMsg)) return false;

        domeElementStatusSkipWs(p);
        if (*p == ',')
        {
            p++;
            continue;
        }
        if (*p == '}')
        {
            p++;
            break;
        }
        errMsg = "expected ',' or '}' in root object";
        return false;
    }

    if (!seenElements)
    {
        errMsg = "missing elements array";
        return false;
    }
    domeElementStatusSkipWs(p);
    if (*p != '\0')
    {
        errMsg = "trailing content after JSON object";
        return false;
    }
    return true;
}

static bool domeElementStatusSaveUpdates(const DomeElementStatusUpdate *updates,
                                         int updateCount)
{
    Preferences prefs;
    if (!prefs.begin(DOME_ELEMENT_STATUS_NS, false)) return false;
    bool allOk = true;
    // Status entries are keyed by generated element index to keep NVS keys
    // short. Persist the layout identity beside them so a future template/table
    // revision cannot accidentally read a stale flag for the wrong element.
    if (prefs.putInt(DOME_ELEMENT_STATUS_META_SCHEMA,
                     domeElementStatusSchemaRevision()) == 0) allOk = false;
    if (allOk && prefs.putInt(DOME_ELEMENT_STATUS_META_REVISION,
                              domeElementStatusTemplateRevision()) == 0) allOk = false;
    if (allOk && prefs.putString(DOME_ELEMENT_STATUS_META_TEMPLATE,
                                 domeElementStatusTemplateId()) == 0) allOk = false;
    for (int i = 0; i < updateCount && allOk; i++)
    {
        char disabledKey[12];
        char reasonKey[12];
        domeElementStatusKey(disabledKey, sizeof(disabledKey),
                             DOME_ELEMENT_STATUS_DISABLED_FMT, updates[i].index);
        domeElementStatusKey(reasonKey, sizeof(reasonKey),
                             DOME_ELEMENT_STATUS_REASON_FMT, updates[i].index);

        if (prefs.putBool(disabledKey, updates[i].disabled) == 0)
        {
            allOk = false;
            break;
        }
        if (!updates[i].disabled || updates[i].reason.length() == 0)
        {
            // Missing keys are acceptable; the important persisted truth is the
            // disabled boolean. Removing stale reasons keeps future GET output
            // from resurrecting an old maintenance note.
            if (prefs.isKey(reasonKey)) prefs.remove(reasonKey);
        }
        else if (prefs.putString(reasonKey, updates[i].reason) == 0)
        {
            allOk = false;
        }
    }
    prefs.end();
    return allOk;
}

static bool domeElementStatusMetadataMatches(Preferences &prefs)
{
    if (!prefs.isKey(DOME_ELEMENT_STATUS_META_SCHEMA) ||
        !prefs.isKey(DOME_ELEMENT_STATUS_META_REVISION) ||
        !prefs.isKey(DOME_ELEMENT_STATUS_META_TEMPLATE))
    {
        return false;
    }
    return prefs.getInt(DOME_ELEMENT_STATUS_META_SCHEMA, -1) ==
               domeElementStatusSchemaRevision() &&
           prefs.getInt(DOME_ELEMENT_STATUS_META_REVISION, -1) ==
               domeElementStatusTemplateRevision() &&
           prefs.getString(DOME_ELEMENT_STATUS_META_TEMPLATE, "") ==
               domeElementStatusTemplateId();
}

static bool domeElementStatusReadAll(DomeElementStatusSnapshot *out, int maxCount)
{
    Preferences prefs;
    if (!prefs.begin(DOME_ELEMENT_STATUS_NS, false)) return false;
    int count = domeElementStatusElementCount();
    if (count > maxCount)
    {
        prefs.end();
        return false;
    }
    bool metadataOk = domeElementStatusMetadataMatches(prefs);
    for (int i = 0; i < count; i++)
    {
        out[i].disabled = false;
        out[i].reason = "";
        if (!metadataOk) continue;

        char disabledKey[12];
        char reasonKey[12];
        domeElementStatusKey(disabledKey, sizeof(disabledKey),
                             DOME_ELEMENT_STATUS_DISABLED_FMT, i);
        domeElementStatusKey(reasonKey, sizeof(reasonKey),
                             DOME_ELEMENT_STATUS_REASON_FMT, i);
        out[i].disabled = prefs.getBool(disabledKey, false);
        out[i].reason = out[i].disabled ? prefs.getString(reasonKey, "") : "";
    }
    prefs.end();
    return true;
}

static String domeElementStatusBuildJson(DomeElementStatusEscapeFn escapeFn)
{
    DomeElementStatusSnapshot statuses[DOME_ELEMENT_STATUS_MAX_ELEMENTS];
    int count = domeElementStatusElementCount();
    bool statusOk = domeElementStatusReadAll(statuses, DOME_ELEMENT_STATUS_MAX_ELEMENTS);
    String json = "{\"elements\":[";
    json.reserve(32 + (count * 72));
    for (int i = 0; i < count; i++)
    {
        if (i > 0) json += ',';
        const char *id = domeElementStatusElementId(i);
        bool disabled = statusOk ? statuses[i].disabled : false;
        String reason = statusOk ? statuses[i].reason : "";
        json += "{\"id\":\"";
        json += escapeFn(String(id ? id : ""));
        json += "\",\"disabled\":";
        json += disabled ? "true" : "false";
        json += ",\"disabled_reason\":";
        if (disabled && reason.length() > 0)
        {
            json += "\"";
            json += escapeFn(reason);
            json += "\"";
        }
        else
        {
            json += "null";
        }
        json += "}";
    }
    json += "]}";
    return json;
}
