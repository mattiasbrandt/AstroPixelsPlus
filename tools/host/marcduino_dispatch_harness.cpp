// Host regression harness for Marcduino command dispatch.
//
// Compiles ReelTwo's real core/Animation.h and core/Marcduino.h (from
// .pio/libdeps) against the minimal shims in tools/host/reeltwo_shim/, plus the
// firmware's real IngressAnimationPlayer class and drainMarcduinoCommandQueue()
// body, which tools/test_marcduino_dispatch_host.py extracts into
// firmware_extract.inc. Only the queue storage and logging are stubbed.
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

// Provides: class IngressAnimationPlayer, static void drainMarcduinoCommandQueue().
// The player object must exist before the drain body is compiled.
#include "firmware_player_class.inc"
static ServoSequencer servoSequencer;
static IngressAnimationPlayer player(servoSequencer);
#include "firmware_drain.inc"

static std::vector<std::string> sRan;
MARCDUINO_ACTION(TestHolo, @HP, ({ sRan.push_back(std::string("@HP ") + Marcduino::getCommand()); }))
MARCDUINO_ACTION(TestOpen, :OP, ({ sRan.push_back(std::string(":OP ") + Marcduino::getCommand()); }))

// Stand-in for the rest of mainLoop() reusing the stack after the drain returns.
__attribute__((noinline)) static void otherLoopWork()
{
    volatile char scratch[1024];
    for (size_t i = 0; i < sizeof(scratch); i++) scratch[i] = 'Z';
}

static void tick()
{
    sNow++;
    player.animate();
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
    sQueue = burst;
    sQueueIdx = 0;
    sRan.clear();
    drainMarcduinoCommandQueue();
    otherLoopWork();
    for (int i = 0; i < 4; i++) tick();
    return report(name, expect);
}

static int legacySerialScenario()
{
    sRan.clear();
    StringStream stream(":OP01\r@HPF005\r");
    MarcduinoSerial<> legacy(player);
    legacy.setStream(&stream);
    // AnimatedEvent::process() order: the serial reader and the player each
    // get one animate() call per loop pass.
    for (int i = 0; i < 64; i++)
    {
        legacy.animate();
        tick();
    }
    return report("legacy MarcduinoSerial path (bypasses ingress)", {":OP 01", "@HP F005"});
}

int main()
{
    int fails = 0;
    fails += queuedScenario("queued @HP command keeps its argument", {"@HPF005"}, {"@HP F005"});
    fails += queuedScenario("queued burst runs every command in order",
                            {":OP01", ":OP02", "@HPF0040"}, {":OP 01", ":OP 02", "@HP F0040"});
    fails += legacySerialScenario();
    printf("%d failing\n", fails);
    return fails ? 1 : 0;
}
