#pragma once
// DomeLayoutTemplateStore.h — SPIFFS-backed dome layout template selection.
//
// Custom templates are display/layout data only. This storage layer rejects
// backend fields such as commands, slots, channels, and targets, then composes
// the selected template with runtime status before it is served externally.

#include <Arduino.h>
#include <Preferences.h>
#include "SPIFFS.h"

#include "GeneratedDomeLayout.h"

#define DOME_LAYOUT_TEMPLATE_NS "dome_layout"
#define DOME_LAYOUT_TEMPLATE_USE_CUSTOM "use_custom"
#define DOME_LAYOUT_TEMPLATE_PATH "/dome-layout-template.json"
#define DOME_LAYOUT_TEMPLATE_TMP_PATH "/dome-layout-template.tmp"
#define DOME_LAYOUT_TEMPLATE_MAX_BYTES 32768
#define DOME_LAYOUT_TEMPLATE_MAX_ELEMENTS 64

struct DomeLayoutTemplateInfo
{
    bool valid;
    bool customInstalled;
    bool customSelected;
    String templateId;
    String templateName;
    int schemaRevision;
    int templateRevision;
    size_t sizeBytes;
    String error;
};

static void domeLayoutTemplateSkipWs(const char *&p)
{
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
}

static bool domeLayoutTemplateParseJsonString(const char *&p, String &out,
                                              String &errMsg)
{
    domeLayoutTemplateSkipWs(p);
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
            errMsg = "unicode escapes are not supported in dome templates";
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

static bool domeLayoutTemplateExpectChar(const char *&p, char expected,
                                         String &errMsg)
{
    domeLayoutTemplateSkipWs(p);
    if (*p != expected)
    {
        errMsg = String("expected '") + expected + "'";
        return false;
    }
    p++;
    return true;
}

static bool domeLayoutTemplateParseInt(const char *&p, int &out)
{
    domeLayoutTemplateSkipWs(p);
    bool negative = false;
    if (*p == '-')
    {
        negative = true;
        p++;
    }
    if (*p < '0' || *p > '9') return false;
    long value = 0;
    while (*p >= '0' && *p <= '9')
    {
        value = (value * 10) + (*p - '0');
        if (value > 1000000L) return false;
        p++;
    }
    out = negative ? -(int)value : (int)value;
    return true;
}

static int domeLayoutTemplateFindKnownId(const String &id)
{
    for (size_t i = 0; i < DomeLayout::kElementCount; i++)
    {
        if (id == DomeLayout::kElements[i].id) return (int)i;
    }
    return -1;
}

static bool domeLayoutTemplateIsCommandableId(const String &id)
{
    for (size_t i = 0; i < DomeLayout::kElementCount; i++)
    {
        const DomeLayout::DomeLayoutElement &element = DomeLayout::kElements[i];
        if (id == element.id) return element.commandable;
    }
    return false;
}

static bool domeLayoutTemplateHasForbiddenBackendKey(const String &key)
{
    return key == "cmd" ||
           key == "command" ||
           key == "command_target" ||
           key == "target" ||
           key == "slot" ||
           key == "channel" ||
           key == "bus" ||
           key == "address" ||
           key == "spi_chain" ||
           key == "pca9685_channel" ||
           key == "servo_channel";
}

static bool domeLayoutTemplateSkipJsonValue(const char *&p, String &errMsg);

static bool domeLayoutTemplateSkipJsonObject(const char *&p, String &errMsg)
{
    if (!domeLayoutTemplateExpectChar(p, '{', errMsg)) return false;
    domeLayoutTemplateSkipWs(p);
    if (*p == '}') { p++; return true; }
    while (*p)
    {
        String key;
        if (!domeLayoutTemplateParseJsonString(p, key, errMsg)) return false;
        if (domeLayoutTemplateHasForbiddenBackendKey(key))
        {
            errMsg = "template contains backend field: " + key;
            return false;
        }
        if (key == "runtime_state_ts" || key == "layout_source")
        {
            errMsg = "template must not define composed runtime field: " + key;
            return false;
        }
        if (!domeLayoutTemplateExpectChar(p, ':', errMsg)) return false;
        if (!domeLayoutTemplateSkipJsonValue(p, errMsg)) return false;
        domeLayoutTemplateSkipWs(p);
        if (*p == ',') { p++; continue; }
        if (*p == '}') { p++; return true; }
        errMsg = "expected ',' or '}'";
        return false;
    }
    errMsg = "unterminated object";
    return false;
}

static bool domeLayoutTemplateSkipJsonArray(const char *&p, String &errMsg)
{
    if (!domeLayoutTemplateExpectChar(p, '[', errMsg)) return false;
    domeLayoutTemplateSkipWs(p);
    if (*p == ']') { p++; return true; }
    while (*p)
    {
        if (!domeLayoutTemplateSkipJsonValue(p, errMsg)) return false;
        domeLayoutTemplateSkipWs(p);
        if (*p == ',') { p++; continue; }
        if (*p == ']') { p++; return true; }
        errMsg = "expected ',' or ']'";
        return false;
    }
    errMsg = "unterminated array";
    return false;
}

static bool domeLayoutTemplateSkipJsonValue(const char *&p, String &errMsg)
{
    domeLayoutTemplateSkipWs(p);
    if (*p == '{') return domeLayoutTemplateSkipJsonObject(p, errMsg);
    if (*p == '[') return domeLayoutTemplateSkipJsonArray(p, errMsg);
    if (*p == '"')
    {
        String ignored;
        return domeLayoutTemplateParseJsonString(p, ignored, errMsg);
    }
    if (strncmp(p, "true", 4) == 0) { p += 4; return true; }
    if (strncmp(p, "false", 5) == 0) { p += 5; return true; }
    if (strncmp(p, "null", 4) == 0) { p += 4; return true; }
    if (*p == '-' || (*p >= '0' && *p <= '9'))
    {
        if (*p == '-') p++;
        if (*p < '0' || *p > '9') { errMsg = "invalid number"; return false; }
        while (*p >= '0' && *p <= '9') p++;
        if (*p == '.')
        {
            p++;
            if (*p < '0' || *p > '9') { errMsg = "invalid number"; return false; }
            while (*p >= '0' && *p <= '9') p++;
        }
        if (*p == 'e' || *p == 'E')
        {
            p++;
            if (*p == '+' || *p == '-') p++;
            if (*p < '0' || *p > '9') { errMsg = "invalid number"; return false; }
            while (*p >= '0' && *p <= '9') p++;
        }
        return true;
    }
    errMsg = "invalid JSON value";
    return false;
}

static bool domeLayoutTemplateFindRootString(const String &json, const char *wantedKey,
                                             String &out, String &errMsg)
{
    const char *p = json.c_str();
    if (!domeLayoutTemplateExpectChar(p, '{', errMsg)) return false;
    while (true)
    {
        domeLayoutTemplateSkipWs(p);
        if (*p == '}') return false;
        String key;
        if (!domeLayoutTemplateParseJsonString(p, key, errMsg)) return false;
        if (!domeLayoutTemplateExpectChar(p, ':', errMsg)) return false;
        if (key == wantedKey)
        {
            return domeLayoutTemplateParseJsonString(p, out, errMsg);
        }
        if (!domeLayoutTemplateSkipJsonValue(p, errMsg)) return false;
        domeLayoutTemplateSkipWs(p);
        if (*p == ',') { p++; continue; }
        if (*p == '}') return false;
        errMsg = "expected ',' or '}'";
        return false;
    }
}

static bool domeLayoutTemplateReadFile(String &out, String &errMsg)
{
    if (!SPIFFS.exists(DOME_LAYOUT_TEMPLATE_PATH))
    {
        errMsg = "custom template is not installed";
        return false;
    }
    File file = SPIFFS.open(DOME_LAYOUT_TEMPLATE_PATH, FILE_READ);
    if (!file)
    {
        errMsg = "custom template cannot be opened";
        return false;
    }
    size_t size = file.size();
    if (size == 0 || size > DOME_LAYOUT_TEMPLATE_MAX_BYTES)
    {
        file.close();
        errMsg = "custom template size is invalid";
        return false;
    }
    out = "";
    out.reserve(size + 1);
    while (file.available())
    {
        out += (char)file.read();
        if (out.length() > DOME_LAYOUT_TEMPLATE_MAX_BYTES)
        {
            file.close();
            errMsg = "custom template is too large";
            return false;
        }
    }
    file.close();
    return true;
}

static bool domeLayoutTemplateValidateElementObject(const char *&p, bool *seenIds,
                                                    int &seenCount, String &errMsg)
{
    if (!domeLayoutTemplateExpectChar(p, '{', errMsg)) return false;
    bool seenId = false;
    bool seenInLayout = false;
    bool seenCommandable = false;
    bool inLayout = false;
    bool commandable = false;
    String id;

    while (true)
    {
        domeLayoutTemplateSkipWs(p);
        if (*p == '}') { p++; break; }
        String key;
        if (!domeLayoutTemplateParseJsonString(p, key, errMsg)) return false;
        if (domeLayoutTemplateHasForbiddenBackendKey(key))
        {
            errMsg = "template contains backend field: " + key;
            return false;
        }
        if (key == "runtime_state_ts" || key == "layout_source")
        {
            errMsg = "template must not define composed runtime field: " + key;
            return false;
        }
        if (key == "active" || key == "disabled" || key == "disabled_reason")
        {
            errMsg = "template must not define runtime field: " + key;
            return false;
        }
        if (!domeLayoutTemplateExpectChar(p, ':', errMsg)) return false;

        if (key == "id")
        {
            if (!domeLayoutTemplateParseJsonString(p, id, errMsg)) return false;
            seenId = true;
        }
        else if (key == "in_layout")
        {
            domeLayoutTemplateSkipWs(p);
            if (strncmp(p, "true", 4) == 0) { inLayout = true; p += 4; }
            else if (strncmp(p, "false", 5) == 0) { inLayout = false; p += 5; }
            else { errMsg = "in_layout must be boolean"; return false; }
            seenInLayout = true;
        }
        else if (key == "commandable")
        {
            domeLayoutTemplateSkipWs(p);
            if (strncmp(p, "true", 4) == 0) { commandable = true; p += 4; }
            else if (strncmp(p, "false", 5) == 0) { commandable = false; p += 5; }
            else { errMsg = "commandable must be boolean"; return false; }
            seenCommandable = true;
        }
        else
        {
            if (!domeLayoutTemplateSkipJsonValue(p, errMsg)) return false;
        }

        domeLayoutTemplateSkipWs(p);
        if (*p == ',') { p++; continue; }
        if (*p == '}') { p++; break; }
        errMsg = "expected ',' or '}' in element";
        return false;
    }

    if (!seenId || !seenInLayout || !seenCommandable)
    {
        errMsg = "element requires id, in_layout, and commandable";
        return false;
    }
    int index = domeLayoutTemplateFindKnownId(id);
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
    if (commandable && !domeLayoutTemplateIsCommandableId(id))
    {
        errMsg = "commandable is only valid for known ring/pie panels: " + id;
        return false;
    }
    if (!inLayout && commandable)
    {
        errMsg = "excluded element cannot be commandable: " + id;
        return false;
    }
    seenIds[index] = true;
    seenCount++;
    return true;
}

static bool domeLayoutTemplateValidateElementsArray(const char *&p, String &errMsg)
{
    if (!domeLayoutTemplateExpectChar(p, '[', errMsg)) return false;
    bool seenIds[DOME_LAYOUT_TEMPLATE_MAX_ELEMENTS] = {false};
    int seenCount = 0;
    domeLayoutTemplateSkipWs(p);
    if (*p == ']')
    {
        errMsg = "elements array is empty";
        return false;
    }
    while (true)
    {
        if (seenCount >= DOME_LAYOUT_TEMPLATE_MAX_ELEMENTS)
        {
            errMsg = "too many layout elements";
            return false;
        }
        if (!domeLayoutTemplateValidateElementObject(p, seenIds, seenCount, errMsg))
        {
            return false;
        }
        domeLayoutTemplateSkipWs(p);
        if (*p == ',') { p++; continue; }
        if (*p == ']') { p++; break; }
        errMsg = "expected ',' or ']' in elements array";
        return false;
    }
    for (size_t i = 0; i < DomeLayout::kElementCount; i++)
    {
        if (!seenIds[i])
        {
            errMsg = String("template is missing explicit known identity: ") +
                     DomeLayout::kElements[i].id;
            return false;
        }
    }
    return true;
}

static bool domeLayoutTemplateValidateJson(const String &json,
                                           DomeLayoutTemplateInfo &info,
                                           String &errMsg)
{
    if (json.length() == 0 || json.length() > DOME_LAYOUT_TEMPLATE_MAX_BYTES)
    {
        errMsg = "template must be 1..32768 bytes";
        return false;
    }

    const char *p = json.c_str();
    if (!domeLayoutTemplateExpectChar(p, '{', errMsg)) return false;

    bool seenSchema = false;
    bool seenTemplateId = false;
    bool seenTemplateName = false;
    bool seenTemplateRevision = false;
    bool seenCoordinateSpace = false;
    bool seenElements = false;

    while (true)
    {
        domeLayoutTemplateSkipWs(p);
        if (*p == '}') { p++; break; }
        String key;
        if (!domeLayoutTemplateParseJsonString(p, key, errMsg)) return false;
        if (domeLayoutTemplateHasForbiddenBackendKey(key))
        {
            errMsg = "template contains backend field: " + key;
            return false;
        }
        if (key == "runtime_state_ts" || key == "layout_source")
        {
            errMsg = "template must not define composed runtime field: " + key;
            return false;
        }
        if (!domeLayoutTemplateExpectChar(p, ':', errMsg)) return false;

        if (key == "schema_revision")
        {
            if (!domeLayoutTemplateParseInt(p, info.schemaRevision))
            {
                errMsg = "schema_revision must be integer";
                return false;
            }
            if (info.schemaRevision != DomeLayout::kSchemaRevision)
            {
                errMsg = "unsupported schema_revision";
                return false;
            }
            seenSchema = true;
        }
        else if (key == "template_id")
        {
            if (!domeLayoutTemplateParseJsonString(p, info.templateId, errMsg)) return false;
            if (info.templateId.length() == 0)
            {
                errMsg = "template_id must not be empty";
                return false;
            }
            seenTemplateId = true;
        }
        else if (key == "template_name")
        {
            if (!domeLayoutTemplateParseJsonString(p, info.templateName, errMsg)) return false;
            if (info.templateName.length() == 0)
            {
                errMsg = "template_name must not be empty";
                return false;
            }
            seenTemplateName = true;
        }
        else if (key == "template_revision")
        {
            if (!domeLayoutTemplateParseInt(p, info.templateRevision))
            {
                errMsg = "template_revision must be integer";
                return false;
            }
            if (info.templateRevision < 1)
            {
                errMsg = "template_revision must be positive";
                return false;
            }
            seenTemplateRevision = true;
        }
        else if (key == "coordinate_space")
        {
            const char *space = p;
            String viewBox;
            if (!domeLayoutTemplateFindRootString(String(space), "viewBox", viewBox, errMsg))
            {
                return false;
            }
            if (viewBox != DomeLayout::kCoordinateSpaceViewBox)
            {
                errMsg = "coordinate_space.viewBox must be 0 0 480 480";
                return false;
            }
            if (!domeLayoutTemplateSkipJsonValue(p, errMsg)) return false;
            seenCoordinateSpace = true;
        }
        else if (key == "elements")
        {
            if (!domeLayoutTemplateValidateElementsArray(p, errMsg)) return false;
            seenElements = true;
        }
        else
        {
            if (!domeLayoutTemplateSkipJsonValue(p, errMsg)) return false;
        }

        domeLayoutTemplateSkipWs(p);
        if (*p == ',') { p++; continue; }
        if (*p == '}') { p++; break; }
        errMsg = "expected ',' or '}' in template root";
        return false;
    }
    domeLayoutTemplateSkipWs(p);
    if (*p != '\0')
    {
        errMsg = "trailing content after template JSON";
        return false;
    }
    if (!seenSchema || !seenTemplateId || !seenTemplateName ||
        !seenTemplateRevision || !seenCoordinateSpace || !seenElements)
    {
        errMsg = "template missing required root fields";
        return false;
    }
    info.valid = true;
    info.sizeBytes = json.length();
    return true;
}

static bool domeLayoutTemplateIsCustomSelected()
{
    Preferences prefs;
    if (!prefs.begin(DOME_LAYOUT_TEMPLATE_NS, true)) return false;
    bool selected = prefs.getBool(DOME_LAYOUT_TEMPLATE_USE_CUSTOM, false);
    prefs.end();
    return selected;
}

static bool domeLayoutTemplateSetCustomSelected(bool selected)
{
    Preferences prefs;
    if (!prefs.begin(DOME_LAYOUT_TEMPLATE_NS, false)) return false;
    bool ok = prefs.putBool(DOME_LAYOUT_TEMPLATE_USE_CUSTOM, selected) > 0;
    prefs.end();
    return ok;
}

static bool domeLayoutTemplateWriteCustom(const String &json, String &errMsg)
{
    if (SPIFFS.exists(DOME_LAYOUT_TEMPLATE_TMP_PATH)) SPIFFS.remove(DOME_LAYOUT_TEMPLATE_TMP_PATH);
    File file = SPIFFS.open(DOME_LAYOUT_TEMPLATE_TMP_PATH, FILE_WRITE);
    if (!file)
    {
        errMsg = "cannot open temporary template file";
        return false;
    }
    size_t written = file.print(json);
    file.close();
    if (written != json.length())
    {
        SPIFFS.remove(DOME_LAYOUT_TEMPLATE_TMP_PATH);
        errMsg = "template write was incomplete";
        return false;
    }
    if (SPIFFS.exists(DOME_LAYOUT_TEMPLATE_PATH)) SPIFFS.remove(DOME_LAYOUT_TEMPLATE_PATH);
    if (!SPIFFS.rename(DOME_LAYOUT_TEMPLATE_TMP_PATH, DOME_LAYOUT_TEMPLATE_PATH))
    {
        SPIFFS.remove(DOME_LAYOUT_TEMPLATE_TMP_PATH);
        errMsg = "cannot promote template file";
        return false;
    }
    return true;
}

static DomeLayoutTemplateInfo domeLayoutTemplateReadInfo()
{
    DomeLayoutTemplateInfo info = {};
    info.schemaRevision = DomeLayout::kSchemaRevision;
    info.templateRevision = DomeLayout::kTemplateRevision;
    info.templateId = DomeLayout::kTemplateId;
    info.templateName = DomeLayout::kTemplateName;
    info.customInstalled = SPIFFS.exists(DOME_LAYOUT_TEMPLATE_PATH);
    info.customSelected = domeLayoutTemplateIsCustomSelected();
    if (!info.customInstalled)
    {
        info.valid = true;
        return info;
    }
    String json;
    String errMsg;
    if (!domeLayoutTemplateReadFile(json, errMsg))
    {
        info.error = errMsg;
        return info;
    }
    info = {};
    info.customInstalled = true;
    info.customSelected = domeLayoutTemplateIsCustomSelected();
    if (!domeLayoutTemplateValidateJson(json, info, errMsg))
    {
        info.schemaRevision = DomeLayout::kSchemaRevision;
        info.templateRevision = DomeLayout::kTemplateRevision;
        info.templateId = DomeLayout::kTemplateId;
        info.templateName = DomeLayout::kTemplateName;
        info.error = errMsg;
        return info;
    }
    return info;
}

static String domeLayoutTemplateInfoJson(String (*escapeFn)(const String &))
{
    DomeLayoutTemplateInfo info = domeLayoutTemplateReadInfo();
    bool servingCustom = info.customInstalled && info.customSelected && info.valid;
    const char *servingId = servingCustom ? info.templateId.c_str() : DomeLayout::kTemplateId;
    const char *servingName = servingCustom ? info.templateName.c_str() : DomeLayout::kTemplateName;
    int servingRevision = servingCustom ? info.templateRevision : DomeLayout::kTemplateRevision;
    int servingSchema = servingCustom ? info.schemaRevision : DomeLayout::kSchemaRevision;
    String json = "{";
    json += "\"selected_source\":\"";
    json += servingCustom ? "custom" : "bundled";
    json += "\",\"custom_installed\":";
    json += info.customInstalled ? "true" : "false";
    json += ",\"custom_selected\":";
    json += info.customSelected ? "true" : "false";
    json += ",\"custom_valid\":";
    json += (info.customInstalled && info.valid) ? "true" : "false";
    json += ",\"template_id\":\"";
    json += escapeFn(String(servingId));
    json += "\",\"template_name\":\"";
    json += escapeFn(String(servingName));
    json += "\",\"template_revision\":";
    json += servingRevision;
    json += ",\"schema_revision\":";
    json += servingSchema;
    if (info.customInstalled && info.valid)
    {
        json += ",\"custom_template_id\":\"";
        json += escapeFn(info.templateId);
        json += "\",\"custom_template_name\":\"";
        json += escapeFn(info.templateName);
        json += "\",\"custom_template_revision\":";
        json += info.templateRevision;
    }
    json += ",\"size_bytes\":";
    json += (unsigned int)info.sizeBytes;
    json += ",\"error\":";
    if (info.error.length() > 0)
    {
        json += "\"";
        json += escapeFn(info.error);
        json += "\"";
    }
    else
    {
        json += "null";
    }
    json += "}";
    return json;
}
