#pragma once
// WiringConfig.h — NVS key schema, MK4 defaults, slot labels, firmware-pin
// formula, and the parse/validate/persist functions for the dynamic panel and
// holo channel-assignment feature.
//
// Included by both AstroPixelsPlus.ino (panelConfigLoad / holoConfigLoad at
// boot) and AsyncWebInterface.h (REST handlers at runtime).  Any translation
// unit including this file must have NUM_PANEL_SLOTS and NUM_HOLO_SLOTS
// already defined — they live alongside servoSettings[] in AstroPixelsPlus.ino.

#include <Preferences.h>

// ---------------------------------------------------------------
// NVS key schema
// Keys are kept short (<= 15 chars) to satisfy ESP32 NVS's key-length limit.
// ---------------------------------------------------------------
#define PREFERENCE_PANELS_NS       "panels"
#define PREFERENCE_PANELS_CH_FMT   "pc_ch%d"   // pc_ch0..pc_ch12 — physical silkscreen channel
#define PREFERENCE_PANELS_ACT_FMT  "pc_act%d"  // pc_act0..pc_act12 — bool active
#define PREFERENCE_HOLOS_NS        "holos"
#define PREFERENCE_HOLOS_CH_FMT    "hc_ch%d"   // hc_ch0..hc_ch5
#define PREFERENCE_HOLOS_ACT_FMT   "hc_act%d"  // hc_act0..hc_act5

// ---------------------------------------------------------------
// MK4 default channel assignments
// Channels are physical silkscreen numbers (0–15) on each PCA9685 board.
// Active=false marks a slot as having no servo wired — setServo() is called
// with pin=0, group=0 to exclude it from all I2C writes and command-mask
// routing.  These are just defaults — builders whose physical wiring differs
// override per-slot via the Wiring Config UI, persisted to NVS.
// ---------------------------------------------------------------
static const uint8_t defaultPanelCh[NUM_PANEL_SLOTS] = {
    0,   // slot 0:  P1   (ring)
    1,   // slot 1:  P2   (ring)
    2,   // slot 2:  P3   (ring)
    3,   // slot 3:  P4   (ring)
    4,   // slot 4:  P7   (ring upper)
    5,   // slot 5:  P11  (ring lower)
    6,   // slot 6:  P13  (ring front)
    0,   // slot 7:  PP5  — unserviced by default; channel value ignored when active=false
    8,   // slot 8:  PP1  (pie)
    9,   // slot 9:  PP2  (pie)
    10,  // slot 10: PP4  (pie)
    11,  // slot 11: PP6  (pie, :OP12 / :OPP6)
    0,   // slot 12: PP3  — unserviced by default; channel value ignored when active=false
};
static const bool defaultPanelActive[NUM_PANEL_SLOTS] = {
    true,  // P1
    true,  // P2
    true,  // P3
    true,  // P4
    true,  // P7
    true,  // P11
    true,  // P13
    false, // PP5 — inactive by default; flip to true via Wiring Config UI if a servo is wired
    true,  // PP1
    true,  // PP2
    true,  // PP4
    true,  // PP6
    false, // PP3 — inactive by default; flip to true via Wiring Config UI if a servo is wired
};

// Holo defaults — physical silkscreen channels on the 0x41 board.
static const uint8_t defaultHoloCh[NUM_HOLO_SLOTS] = {
    0,  // slot 13 (holo 0): FHP — front holo horizontal
    1,  // slot 14 (holo 1): FHP — front holo vertical
    2,  // slot 15 (holo 2): THP — top holo horizontal
    3,  // slot 16 (holo 3): THP — top holo vertical
    4,  // slot 17 (holo 4): RHP — rear holo vertical
    5,  // slot 18 (holo 5): RHP — rear holo horizontal
};
static const bool defaultHoloActive[NUM_HOLO_SLOTS] = {
    true, true, true, true, true, true,
};

// ---------------------------------------------------------------
// Slot labels
// kPanelSlotLabels: MK4 printed-droid identifiers, indexed by slot.  Used in
// boot logs (panelConfigLoad) and referenced by WiringCommissioning.h for
// the GET /api/panels/config response.
// kHoloSlotLabels: short axis format for boot logs.  WiringCommissioning.h uses
// a more verbose format (with community HP equivalents) for GET responses.
// ---------------------------------------------------------------
static const char *kPanelSlotLabels[NUM_PANEL_SLOTS] = {
    "P1", "P2", "P3", "P4", "P7", "P11", "P13",
    "PP5", "PP1", "PP2", "PP4", "PP6", "PP3",
};
static const char *kHoloSlotLabels[NUM_HOLO_SLOTS] = {
    "FHP horizontal", "FHP vertical",
    "THP horizontal", "THP vertical",
    "RHP vertical",   "RHP horizontal",
};

// ---------------------------------------------------------------
// Firmware-pin formula (implements ADR 0004)
// Converts a physical silkscreen channel (0–15) on a given PCA9685 board
// to the 1-indexed firmware pin used by ServoDispatchPCA9685.
// General formula: (board * 16) + physCh + 1
//   board=0 (0x40 panel board): pin = physCh + 1
//   board=1 (0x41 holo board):  pin = 16 + physCh + 1  (holo board starts
//                                at firmware pin 17 — pin 16 still addresses
//                                0x40 CH15; see ADR 0004 for derivation)
// ---------------------------------------------------------------
static inline uint8_t wiringConfigFirmwarePin(uint8_t board, uint8_t physCh)
{
    return (uint8_t)((board * 16u) + physCh + 1u);
}

// ---------------------------------------------------------------
// Pure helpers and parse/validate/persist functions
// ---------------------------------------------------------------

// Returns true if the first non-whitespace character at or after p is a clean
// JSON value terminator: ',', '}', ']', or NUL.  Whitespace is a SEPARATOR,
// not a terminator.  Rejects "5bad", "5 garbage", "truejunk", etc.
static inline bool jsonValueEndsCleanly(const char *p)
{
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return *p == ',' || *p == '}' || *p == ']' || *p == '\0';
}

// Read the current persisted config for the given namespace into the caller's
// arrays, applying defaults for any keys not present in NVS.
static void wiringConfigRead(const char *ns, const char *chFmt, const char *actFmt,
                              int slotCount, const uint8_t *defaultCh,
                              const bool *defaultActive,
                              uint8_t *outCh, bool *outActive)
{
    Preferences prefs;
    // Read-write to suppress the first-time NOT_FOUND error log if the
    // namespace doesn't exist yet (e.g. GET fires before any save has run
    // on a fresh dome). No put*() calls below — no flash writes occur.
    prefs.begin(ns, false);
    for (int i = 0; i < slotCount; i++)
    {
        char keyC[12];
        char keyA[12];
        snprintf(keyC, sizeof(keyC), chFmt, i);
        snprintf(keyA, sizeof(keyA), actFmt, i);
        outCh[i]     = prefs.isKey(keyC) ? prefs.getUChar(keyC, defaultCh[i]) : defaultCh[i];
        outActive[i] = prefs.isKey(keyA) ? prefs.getBool(keyA, defaultActive[i]) : defaultActive[i];
    }
    prefs.end();
}

// Minimal scanner over the POST body.  Looks for the bare keys "index",
// "channel", and "active" with their numeric/bool values — strict on shape
// but tolerant of whitespace.  On any structural failure or out-of-range
// value, returns false with errMsg populated and outCh/outActive untouched.
//
// The full POST body must be a JSON object with a "slots" array of exactly
// expectedSlots entries; each entry must carry index, channel (0–15), and
// active (true|false).  Indexes must match array position (slot 0 → index 0).
// Duplicate active channels are detected by the caller, not here.
static bool wiringConfigParseBody(const String &body, int expectedSlots,
                                   uint8_t *outCh, bool *outActive, String &errMsg)
{
    int slotsKey = body.indexOf("\"slots\"");
    if (slotsKey < 0) { errMsg = "missing slots array"; return false; }
    int arrStart = body.indexOf('[', slotsKey);
    if (arrStart < 0) { errMsg = "slots is not an array"; return false; }
    int arrEnd = body.indexOf(']', arrStart);
    if (arrEnd < 0) { errMsg = "unterminated slots array"; return false; }

    int parsed = 0;
    int pos = arrStart + 1;
    while (pos < arrEnd && parsed < expectedSlots)
    {
        int objStart = body.indexOf('{', pos);
        if (objStart < 0 || objStart > arrEnd) break;
        int objEnd = body.indexOf('}', objStart);
        if (objEnd < 0 || objEnd > arrEnd) { errMsg = "unterminated slot object"; return false; }
        String slot = body.substring(objStart, objEnd + 1);

        // Required fields: index, channel, active.
        int idxKey = slot.indexOf("\"index\"");
        int chKey  = slot.indexOf("\"channel\"");
        int actKey = slot.indexOf("\"active\"");
        if (idxKey < 0 || chKey < 0 || actKey < 0)
        {
            errMsg = "slot missing index/channel/active";
            return false;
        }
        int idxColon = slot.indexOf(':', idxKey);
        int chColon  = slot.indexOf(':', chKey);
        int actColon = slot.indexOf(':', actKey);
        // Use end-pointer form of strtol so we detect "not a number" inputs
        // — strtol returns 0 for both "0" and "bad". Then require the token
        // to end CLEANLY (skip any whitespace, then hit ',', '}', ']', or
        // NUL). This rejects "5bad", "5 garbage", and similar where the
        // parsed prefix would otherwise be silently accepted.
        const char *idxStart = slot.c_str() + idxColon + 1;
        const char *chStart  = slot.c_str() + chColon  + 1;
        char *idxEnd = nullptr;
        char *chEnd  = nullptr;
        long idxVal  = strtol(idxStart, &idxEnd, 10);
        long chVal   = strtol(chStart,  &chEnd,  10);
        if (idxEnd == idxStart || !jsonValueEndsCleanly(idxEnd))
        { errMsg = "index must be a number at slot " + String(parsed); return false; }
        if (chEnd  == chStart  || !jsonValueEndsCleanly(chEnd))
        { errMsg = "channel must be a number at slot " + String(parsed); return false; }

        // For active, startsWith("true"/"false") matches "truejunk" /
        // "falsejunk" / "true garbage" too. Require the keyword to end
        // CLEANLY at a structural separator (whitespace skipped first).
        String actStr = slot.substring(actColon + 1);
        actStr.trim();
        const char *aStr = actStr.c_str();
        bool actVal;
        if (actStr.startsWith("true")  && jsonValueEndsCleanly(aStr + 4))
            actVal = true;
        else if (actStr.startsWith("false") && jsonValueEndsCleanly(aStr + 5))
            actVal = false;
        else { errMsg = "active must be true or false at slot " + String(parsed); return false; }

        if (idxVal != parsed)
        {
            errMsg = "slot index mismatch (expected " + String(parsed) +
                     ", got " + String(idxVal) + ")";
            return false;
        }
        if (chVal < 0 || chVal > 15)
        {
            errMsg = "channel " + String(chVal) + " out of range (0-15) at slot " + String(parsed);
            return false;
        }
        outCh[parsed]     = (uint8_t)chVal;
        outActive[parsed] = actVal;
        parsed++;
        pos = objEnd + 1;
    }

    if (parsed != expectedSlots)
    {
        errMsg = "expected " + String(expectedSlots) + " slots, got " + String(parsed);
        return false;
    }
    // Reject extra slot objects beyond the expected count — the parse loop
    // stops once parsed == expectedSlots, but a malicious or buggy client
    // could send N+1 valid-looking rows and we'd silently keep only the
    // first N. Scan from the cursor to the array terminator for any more '{'.
    int trailing = body.indexOf('{', pos);
    if (trailing >= 0 && trailing < arrEnd)
    {
        errMsg = "too many slots (expected exactly " + String(expectedSlots) + ")";
        return false;
    }
    return true;
}

// Check for two active slots sharing the same physical channel — that would
// drive the same PCA9685 output from two ReelTwo slots and produce undefined
// behaviour at runtime.  Returns false on conflict with errMsg populated.
static bool wiringConfigCheckConflicts(int slotCount, const uint8_t *channels,
                                        const bool *actives, String &errMsg)
{
    for (int a = 0; a < slotCount; a++)
    {
        if (!actives[a]) continue;
        for (int b = a + 1; b < slotCount; b++)
        {
            if (!actives[b]) continue;
            if (channels[a] == channels[b])
            {
                errMsg = "channel " + String(channels[a]) +
                         " is assigned to slots " + String(a) + " and " + String(b);
                return false;
            }
        }
    }
    return true;
}

// Atomic-ish NVS write — opens the namespace read/write, writes all pairs,
// closes.  Each Preferences.put*() call returns the number of bytes written;
// 0 means the flash write failed (out of space, NVS corrupt, etc.).  We bail
// at the first failure so the caller can report a 500 instead of silently
// claiming success.  The namespace MAY be left partially updated on failure
// (NVS doesn't transact across keys) — the next successful save corrects it,
// and the GET endpoint always returns the current persisted state.
static bool wiringConfigSave(const char *ns, const char *chFmt, const char *actFmt,
                              int slotCount, const uint8_t *channels, const bool *actives)
{
    Preferences prefs;
    if (!prefs.begin(ns, false)) return false;
    bool allOk = true;
    for (int i = 0; i < slotCount && allOk; i++)
    {
        char keyC[12];
        char keyA[12];
        snprintf(keyC, sizeof(keyC), chFmt, i);
        snprintf(keyA, sizeof(keyA), actFmt, i);
        if (prefs.putUChar(keyC, channels[i]) == 0) allOk = false;
        if (allOk && prefs.putBool(keyA, actives[i]) == 0) allOk = false;
    }
    prefs.end();
    return allOk;
}
