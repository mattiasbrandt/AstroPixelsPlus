// Host regression harness for Marcduino command dispatch.
//
// Compiles ReelTwo's real core/Animation.h and core/Marcduino.h (from
// .pio/libdeps) against the minimal shims in tools/host/reeltwo_shim/, plus the
// firmware's real IngressAnimationPlayer class, dispatchPendingDomeAnimation()
// and drainMarcduinoCommandQueue(), which tools/test_marcduino_dispatch_host.py
// extracts from the sources into firmware_*.inc. Only the queue storage and
// logging are stubbed; the DM:* / dome-sequence handlers below mirror the
// shape of the real ones in DomeSequences.h.
//
// Exit status 0 means every scenario passed.

#include <cstdio>
#include <string>
#include <vector>

#include "core/Marcduino.h"

static unsigned long sNow = 0;
unsigned long millis() { return sNow; }

struct LogCaptureStub
{
    void printf(const char *, ...) {}
} logCapture;
static bool sSuppressBodyLinkEgress = false;
#define CONSOLE_BUFFER_SIZE 300

static std::vector<std::string> sQueue;
static size_t sQueueIdx = 0;

static bool dequeueMarcduinoCommand(char *source, size_t sourceSize, char *cmd, size_t cmdSize,
                                    bool *suppressBodyLinkEgress)
{
    if (sQueueIdx >= sQueue.size()) return false;
    snprintf(source, sourceSize, "host-test");
    snprintf(cmd, cmdSize, "%s", sQueue[sQueueIdx++].c_str());
    *suppressBodyLinkEgress = false;
    return true;
}

static bool dome_seqRunning = false;
static AnimationStep dome_pendingAnim = nullptr;

// Real firmware code, in the same order as AstroPixelsPlus.ino.
#include "firmware_player_class.inc"
static ServoSequencer servoSequencer;
static IngressAnimationPlayer player(servoSequencer);
#include "firmware_dome_dispatch.inc"
#include "firmware_drain.inc"

static std::vector<std::string> sRan;
MARCDUINO_ACTION(TestHolo, @HP, ({ sRan.push_back(std::string("@HP ") + Marcduino::getCommand()); }))
MARCDUINO_ACTION(TestOpen, :OP, ({ sRan.push_back(std::string(":OP ") + Marcduino::getCommand()); }))
MARCDUINO_ACTION(TestStop, :SE00, ({ sRan.push_back(":SE00"); }))
MARCDUINO_ANIMATION(TestMultiStep, :SE01)
{
    DO_START()
    DO_ONCE({ sRan.push_back(":SE01 step1"); })
    DO_WAIT_MILLIS(2)
    DO_ONCE({ sRan.push_back(":SE01 step2"); })
    DO_END()
}

// Same shape as DomeSequences.h: begin in the first step, end in DO_RESET
// (which runs both on completion and when another command pre-empts it).
ANIMATION(testDomeScream)
{
    DO_START()
    DO_ONCE({ dome_seqRunning = true; sRan.push_back("scream begin"); })
    DO_WAIT_MILLIS(50)   // longer than any serial line below takes to arrive
    DO_ONCE({ sRan.push_back("scream done"); })
    DO_RESET({ sRan.push_back("scream reset"); dome_seqRunning = false; })
    DO_END()
}
MARCDUINO_ACTION(TestDomeScream, DM:SCREAM, ({ if (!dome_seqRunning) dome_pendingAnim = Animation_testDomeScream; }))

// Stand-in for the rest of mainLoop() reusing the stack after the drain returns.
__attribute__((noinline)) static void otherLoopWork()
{
    volatile char scratch[1024];
    for (size_t i = 0; i < sizeof(scratch); i++) scratch[i] = 'Z';
}

// One mainLoop() pass after the drain: AnimatedEvent::process() then the
// pending dome-sequence handoff, as in AstroPixelsPlus.ino.
static void tick()
{
    sNow++;
    player.animate();
    dispatchPendingDomeAnimation();
}

static void resetState()
{
    for (int i = 0; i < 256; i++) tick();   // let any running animation finish
    sQueue.clear();
    sQueueIdx = 0;
    dome_pendingAnim = nullptr;
    dome_seqRunning = false;
    sRan.clear();
}

class StringStream : public Stream
{
public:
    explicit StringStream(const char *text) : fText(text) {}
    int available() override { return *fText != '\0'; }
    int read() override { return *fText ? *fText++ : -1; }

private:
    const char *fText;
};

// Prints a command, escaping bytes a dangling getCommand() may have produced.
static void printCommand(const std::string &s)
{
    printf(" [");
    for (size_t i = 0; i < s.size() && i < 24; i++)
    {
        unsigned char c = (unsigned char)s[i];
        if (c >= 0x20 && c < 0x7f) putchar(c);
        else printf("\\x%02x", c);
    }
    printf("]");
}

static int report(const char *name, const std::vector<std::string> &expect)
{
    bool ok = sRan == expect;
    printf("%s %s\n  expected:", ok ? "PASS" : "FAIL", name);
    for (const auto &s : expect) printCommand(s);
    printf("\n  ran:     ");
    for (const auto &s : sRan) printCommand(s);
    printf("\n");
    return ok ? 0 : 1;
}

static int queuedScenario(const char *name, std::vector<std::string> burst, std::vector<std::string> expect)
{
    resetState();
    sQueue = burst;
    sQueueIdx = 0;
    drainMarcduinoCommandQueue();
    otherLoopWork();
    for (int i = 0; i < 256; i++) tick();
    return report(name, expect);
}

static int legacySerialScenario(const char *name, const char *lines, std::vector<std::string> expect)
{
    resetState();
    StringStream stream(lines);
    MarcduinoSerial<> legacy(player);
    legacy.setStream(&stream);
    // AnimatedEvent::process() order: the serial reader and the player each
    // get one animate() call per loop pass, then mainLoop's dome handoff.
    for (int i = 0; i < 256; i++)
    {
        legacy.animate();
        tick();
    }
    return report(name, expect);
}

int main()
{
    int fails = 0;
    fails += queuedScenario("queued @HP command keeps its argument", {"@HPF005"}, {"@HP F005"});
    fails += queuedScenario("queued burst runs every command in order",
                            {":OP01", ":OP02", "@HPF0040"}, {":OP 01", ":OP 02", "@HP F0040"});
    fails += queuedScenario("queued DM:* sequence runs to completion",
                            {"DM:SCREAM"}, {"scream begin", "scream done", "scream reset"});
    fails += queuedScenario("queued :SE00 after DM:* stops it and it does not restart",
                            {"DM:SCREAM", ":SE00"}, {"scream begin", "scream reset", ":SE00"});
    fails += queuedScenario("queued multi-step sequence after DM:* is not overridden",
                            {"DM:SCREAM", ":SE01"}, {"scream begin", "scream reset", ":SE01 step1", ":SE01 step2"});
    fails += legacySerialScenario("legacy MarcduinoSerial path (bypasses ingress)",
                                  ":OP01\r@HPF005\r", {":OP 01", "@HP F005"});
    fails += legacySerialScenario("legacy MarcduinoSerial: :SE00 after DM:* stops it",
                                  "DM:SCREAM\r:SE00\r", {"scream begin", "scream reset", ":SE00"});
    printf("%d failing\n", fails);
    return fails ? 1 : 0;
}
