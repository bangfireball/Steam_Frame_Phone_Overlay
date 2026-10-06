#include "phonecast/core/audio/AudioPlayback.h"
#include "phonecast/core/streaming/DispatchBudget.h"
#include "phonecast/core/audio/MediaClock.h"
#include "phonecast/platform/network/TcpVideoServer.h"
#include "phonecast/vr/overlay/SettingsMenuController.h"
#include "phonecast/vr/overlay/OverlaySettingsStore.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
using SOCKET = int;
constexpr SOCKET INVALID_SOCKET = -1;
inline int closesocket(SOCKET s) { return close(s); }
#endif
#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace {
using namespace phonecast::core;
using namespace phonecast::core::audio;
using namespace phonecast::core::protocol;
using Clock = std::chrono::steady_clock;
void Check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
template<class F> bool Wait(F f, int milliseconds = 1500) {
    const auto end = Clock::now() + std::chrono::milliseconds(milliseconds);
    do { if (f()) return true; std::this_thread::sleep_for(std::chrono::milliseconds(2)); } while (Clock::now() < end);
    return false;
}
Message Config(std::uint64_t epoch = 9) { Message m; m.type = MessageType::AudioConfig; m.payload = Configuration(epoch); return m; }
Message Frame(std::uint64_t sequence = 0, std::uint64_t epoch = 9) {
    Message m; m.type = MessageType::AudioFrame; m.sequence = sequence; m.timestampMicros = 1000000 + sequence * 10000;
    Block b{epoch,sequence,m.timestampMicros,std::vector<std::uint8_t>(BytesPerBlock,0)};
    b.samples[0] = 0x34; b.samples[1] = 0x12;
    m.payload = EncodeBlock(b); return m;
}
void ProtocolTests() {
    Check(HasCapability(Capability()),"capability"); Check(!HasCapability({}),"old empty PONG");
    auto bad = Capability(); bad[4] = 2; Check(!HasCapability(bad),"new incompatible version");
    Check(Configuration(0x0102030405060708ULL) == std::vector<std::uint8_t>({1,1,2,0,0,0,0xbb,0x80,1,2,3,4,5,6,7,8}),"Java/C++ config golden");
    auto c = Config(); std::uint64_t epoch = 0; Check(ParseConfiguration(c,epoch) && epoch == 9,"parse config");
    for (std::size_t i : {0U,1U,2U,3U,4U,5U,6U,7U}) {
        auto invalid = c; invalid.payload[i] ^= 1; Check(!ParseConfiguration(invalid,epoch),"strict format validation");
    }
    c.flags = 1; Check(!ParseConfiguration(c,epoch),"reserved flags");
    auto m = Frame(); Block b;
    Check(ParseBlock(m,b) && b.samples.size() == 1920 && b.samples[0] == 0x34 && b.samples[1] == 0x12,"PCM golden");
    for (std::size_t i : {8U,9U,10U,11U}) {
        auto invalid = m; invalid.payload[i] ^= 1; Check(!ParseBlock(invalid,b),"strict frame metadata");
    }
    m.timestampMicros = UINT64_MAX; Check(!ParseBlock(m,b),"timestamp overflow");
    m = Frame(); m.width = 1; Check(!ParseBlock(m,b),"audio has no video dimensions");
    m = Frame(); m.payload.pop_back(); Check(!ParseBlock(m,b),"partial sample block");
    auto bytes = Serialize(m); Message header; std::uint32_t size = 0; std::string error;
    Check(!ParseHeader(bytes.data(),bytes.size(),header,size,error),"length rejected before allocation");
    Message status; status.type = MessageType::AudioStatus; status.payload = Status(9,CaptureStatus::Silent);
    CaptureStatus state{}; Check(ParseStatus(status,epoch,state) && state == CaptureStatus::Silent,"status roundtrip");
    status.payload[2] = 1; Check(!ParseStatus(status,epoch,state),"status reserved");
}
struct FakeState { std::atomic<int> opens{}, closes{}, writes{}, flushes{}; bool fail{false}; bool full{false}; std::mutex mutex; std::vector<std::uint8_t> samples; };
class FakeOutput : public IAudioOutput {
public:
    explicit FakeOutput(std::shared_ptr<FakeState> state) : s(std::move(state)) {}
    bool Open(const std::string&, std::string& error) override { ++s->opens; if (s->fail) { error="test unavailable"; return false; } return true; }
    bool Write(const std::vector<std::uint8_t>& samples,std::string& error) override {
        error.clear(); if (s->full) return false;
        std::lock_guard<std::mutex> lock(s->mutex); s->samples = samples; ++s->writes; return true;
    }
    bool Flush(std::string& error) override { error.clear(); ++s->flushes; return true; }
    std::uint64_t LatencyMicros() override { return 20000; }
    std::string Description() const override { return "Playing: fake"; }
    void Close() noexcept override { ++s->closes; }
private: std::shared_ptr<FakeState> s;
};
Block BlockFrom(std::uint64_t sequence = 0) { Block b; Check(ParseBlock(Frame(sequence),b),"fixture block"); return b; }
void PlaybackTests() {
    auto state = std::make_shared<FakeState>();
    AudioPlayback player([state] { return std::make_unique<FakeOutput>(state); });
    player.Configure(9); player.SetControls(false,0.5F,""); player.Enqueue(BlockFrom());
    Check(Wait([&] { return player.Stats().submitted == 1; }),"asynchronous submit");
    { std::lock_guard<std::mutex> lock(state->mutex); Check(state->samples[0] == 0x1a && state->samples[1] == 0x09,"phone-only PCM gain"); }
    Check(Wait([&] { return player.AudibleTimestamp() > 0; }),"bounded audio clock");
    const auto submitted = player.Stats().submitted;
    player.SetControls(true,1.0F,""); player.Enqueue(BlockFrom(1));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    Check(player.Stats().submitted == submitted && player.AudibleTimestamp() == 0,"mute flush and no backlog");
    player.SetControls(false,1.0F,""); player.Enqueue(BlockFrom(2));
    Check(Wait([&] { return player.Stats().submitted == submitted + 1; }),"unmute fresh audio");
    player.Enqueue(BlockFrom(2)); Check(player.Stats().dropped > 0,"duplicate rejected");
    // Ordinary losses and larger forward discontinuities must not create a
    // fresh device/server stream for every missing block.
    const auto beforeGaps = player.Stats();
    const auto opensBeforeGaps = state->opens.load();
    for (unsigned seq : {20U,40U,60U}) {
        const auto before = player.Stats().submitted;
        const auto previousFlushes = state->flushes.load();
        player.Enqueue(BlockFrom(seq));
        Check(Wait([&] { return player.Stats().submitted == before+1 && state->flushes > previousFlushes; }),
            "forward timeline gaps flush backend and resume");
    }
    Check(state->opens == opensBeforeGaps && player.Stats().reanchors == beforeGaps.reanchors+3 &&
        player.Stats().sequenceGaps == beforeGaps.sequenceGaps+3,
        "packet gaps retain healthy output rather than reopen storm");
    const auto beforeSmallGap = player.Stats().submitted;
    player.Enqueue(BlockFrom(63));
    Check(Wait([&] { return player.Stats().submitted == beforeSmallGap+1; }),"small loss remains playable");
    Check(state->opens == opensBeforeGaps && state->flushes == 3,"ordinary small gap needs no output reset");
    player.Reset(); player.Enqueue(BlockFrom(64)); Check(player.Stats().queued == 0,"old epoch after reset");
    player.Stop();
    state = std::make_shared<FakeState>(); state->full = true;
    AudioPlayback blocked([state] { return std::make_unique<FakeOutput>(state); }); blocked.Configure(9);
    for (unsigned i=0;i<100;++i) blocked.Enqueue(BlockFrom(i));
    Check(blocked.Stats().queued <= 10 && blocked.Stats().dropped >= 90,"queue duration bound");
    Check(Wait([&] { return blocked.Stats().queued == 0; }),"stale sound expires while output blocked"); blocked.Stop();
    state = std::make_shared<FakeState>(); state->fail = true;
    AudioPlayback failed([state] { return std::make_unique<FakeOutput>(state); }); failed.Configure(9);
    const auto end = Clock::now() + std::chrono::milliseconds(2400); unsigned seq=0;
    while (Clock::now() < end) { failed.Enqueue(BlockFrom(seq++)); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
    Check(state->opens == 3 && failed.Stats().failures == 3,"bounded reopen attempts"); failed.Stop();
}
VideoFrame Video(std::uint64_t pts) { VideoFrame frame; frame.width=1; frame.height=1; frame.pixels.resize(4); frame.timestampMicros=pts; return frame; }
void SyncTests() {
    AudioVideoQueue queue; const auto now = Clock::now(); VideoFrame result;
    queue.Add(Video(1000000),now); queue.Add(Video(1030000),now);
    Check(!queue.Take(980000,now,result),"future video not early");
    Check(queue.Take(1000000,now,result) && result.timestampMicros == 1000000,"matching audio frame");
    Check(queue.Take(1030000,now,result) && result.timestampMicros == 1030000,"next frame");
    for (int i=0;i<10;++i) queue.Add(Video(2000000+static_cast<std::uint64_t>(i)*10000),now);
    Check(queue.Size() == AudioVideoQueue::MaxPendingFrames,"video memory bound");
    Check(queue.Take(1900000,now+std::chrono::milliseconds(101),result),"never freeze on lost clock");
    queue.Add(Video(3000000),now); Check(queue.Take(0,now,result),"video-only fallback");
    queue.Add(Video(9000000),now); Check(queue.Take(1000,now,result),"clock-domain mismatch fallback");
    queue.Clear(); queue.Add(Video(1000000),now); queue.Add(Video(1033333),now+std::chrono::milliseconds(34));
    Check(!queue.Take(870000,now+std::chrono::milliseconds(99),result),"individual deadline not early");
    Check(queue.Take(870000,now+std::chrono::milliseconds(100),result) && result.timestampMicros == 1000000,
        "release overdue oldest picture instead of jumping to newest");
    Check(queue.Size() == 1 && queue.Stats().deadlineReleases > 0 && queue.Stats().lastHoldMicros == 100000 &&
        queue.Stats().haveEstimatedSkew && queue.Stats().estimatedSkewMicros == 130000,"deadline diagnostics");
    Check(!queue.Take(870000,now+std::chrono::milliseconds(133),result),"future picture keeps its own deadline");
    Check(queue.Take(870000,now+std::chrono::milliseconds(134),result) && result.timestampMicros == 1033333,
        "future picture released on its original deadline");
    queue.Clear();
    for (unsigned i=0;i<20;++i) queue.Add(Video(1000000+i*1000),now);
    Check(queue.Size() == AudioVideoQueue::MaxPendingFrames,"burst memory bound");
    Check(queue.Take(870000,now,result) && queue.Stats().overflowReleases == 1,"overflow cannot renew deadline forever");
    Check(queue.Size() == AudioVideoQueue::MaxPendingFrames - 1,"overflow releases only one future picture");
    queue.Clear(); queue.Add(Video(1000000),now);
    Check(!queue.Take(870000,now,result) && !queue.Stats().haveEstimatedSkew,"clear resets pending overflow and timing observation");
    queue.Clear(); queue.Add(Video(1000000),now); queue.Add(Video(1033333),now);
    Check(queue.Take(0,now,result) && result.timestampMicros == 1033333 && queue.Size() == 0 &&
        queue.Stats().lastHoldMicros == 0 && !queue.Stats().haveEstimatedSkew,"audio off stays immediate/latest-only");
    queue.Add(Video(0),now); Check(queue.Take(1000000,now,result),"unknown picture PTS stays immediate");
    queue.Add(Video(UINT64_MAX-1),now);
    Check(queue.Take(UINT64_MAX-5000,now,result),"audio tolerance arithmetic does not overflow");
}
void ContinuousVideoSyncTests() {
    // Model the measured Frame audio delay: ~60 ms in the output device plus
    // ~60-80 ms of pending audio. New pictures must not renew the wait forever.
    const auto start = Clock::now();
    for (unsigned fps : {20U,30U,60U,240U}) {
        for (std::uint64_t lag : {0ULL,20000ULL,60000ULL,130000ULL,250000ULL,1000000ULL}) {
            AudioVideoQueue queue;
            unsigned input = 0, presented = 0;
            std::uint64_t nextFrameMicros = 0, previousPts = 0;
            std::chrono::milliseconds previousPresentation{}, maximumGap{};
            for (unsigned ms = 0; ms < 10000; ms += 2) {
                const auto elapsed = std::chrono::milliseconds(ms);
                const auto micros = static_cast<std::uint64_t>(ms) * 1000;
                if (micros >= nextFrameMicros) {
                    queue.Add(Video(10000000 + micros),start + elapsed);
                    nextFrameMicros += 1000000 / fps;
                    ++input;
                }
                Check(queue.Size() <= AudioVideoQueue::MaxPendingFrames,"continuous queue memory bound");
                VideoFrame result;
                if (queue.Take(lag ? 10000000 + micros - lag : 0,start + elapsed,result)) {
                    Check(result.timestampMicros > previousPts,"continuous playback must advance picture PTS");
                    if (presented) maximumGap = std::max(maximumGap,elapsed - previousPresentation);
                    previousPresentation = elapsed;
                    previousPts = result.timestampMicros;
                    Check(queue.Stats().lastHoldMicros <= 102000,"continuous frames cannot exceed deadline plus polling interval");
                    ++presented;
                }
            }
            if (fps <= 60) {
                Check(presented >= input - fps/10 - 2,"lagging audio clock must preserve input video cadence after startup");
                Check(maximumGap <= std::chrono::milliseconds(1000/fps + 4),"audio clock must not create periodic video pauses");
                Check(queue.Stats().coalescedFrames == 0 && queue.Stats().overflowReleases == 0,"normal cadence needs no overflow or frame coalescing");
            } else {
                Check(presented >= input/2 - 8 && maximumGap <= std::chrono::milliseconds(12),"over-capacity source cannot starve presentation");
            }
            std::cout << "Video sync simulation: " << fps << " FPS, audio lag " << lag/1000
                      << " ms, " << presented << '/' << input << " pictures, max gap " << maximumGap.count() << " ms\n";
        }
    }
}

void MediaClockTests() {
    MediaClock clock; const auto start=Clock::now();
    Check(clock.Timestamp(start)==0,"unobserved media clock invalid");
    clock.Observe(1000000,1020000,start);
    Check(clock.Timestamp(start+std::chrono::milliseconds(10))==1010000,"clock interpolates known playable samples");
    clock.Observe(1005000,1030000,start+std::chrono::milliseconds(10));
    Check(clock.Timestamp(start+std::chrono::milliseconds(10))>=1010000,"latency jitter cannot reverse clock");
    Check(clock.Timestamp(start+std::chrono::milliseconds(90))==1030000,"clock cannot invent unsubmitted sound");
    Check(clock.Timestamp(start+std::chrono::milliseconds(110))==0,"missing observations expire clock");
    clock.Reset(); Check(clock.Timestamp(start)==0,"timeline reset invalidates clock");
    clock.Observe(UINT64_MAX-10000,UINT64_MAX,start);
    Check(clock.Timestamp(start+std::chrono::milliseconds(20))==UINT64_MAX,"clock interpolation saturates without overflow");
    clock.Reset(); clock.Observe(1000000,2000000,start);
    clock.Observe(1200000,2000000,start+std::chrono::milliseconds(2));
    Check(clock.Timestamp(start+std::chrono::milliseconds(2))==1004000,"forward timing correction is bounded");
    clock.Observe(500000,510000,start+std::chrono::milliseconds(4));
    Check(clock.Timestamp(start+std::chrono::milliseconds(4))==500000,"new sample epoch cannot violate clock bounds");
}

void BurstyDispatchTests() {
    const auto start = Clock::now();
    DispatchBudget packetLimit(start);
    for (unsigned i=0;i<DispatchBudget::MaximumMessages;++i) {
        Check(packetLimit.CanDispatch(start),"packet budget permits work"); packetLimit.Dispatched();
    }
    Check(!packetLimit.CanDispatch(start),"packet budget terminates even cheap notification/config work");
    DispatchBudget timeLimit(start);
    Check(!timeLimit.CanDispatch(start+std::chrono::milliseconds(8)),"soft deadline checked before next pop");
    // A synthetic application loop, not just an evenly spaced Add/Take source:
    // transport batches, non-picture dispatch, decoder cost, and interaction
    // work share the same cooperative budget as the VR receiver.
    for (bool audioOn : {false,true}) {
        AudioVideoQueue playout;
        std::deque<std::uint64_t> transport;
        std::uint64_t us=0, nextBatch=0, nextPts=10000000, lastPts=0, lastPresentation=0, maxGap=0;
        unsigned decoded=0, submitted=0, passes=0;
        while (us < 10000000) {
            if (us >= nextBatch) {
                transport.push_back(0); // Notification/config-like work, no picture.
                for (unsigned i=0;i<3;++i) { transport.push_back(nextPts); nextPts+=33333; }
                nextBatch += 100000;
            }
            us += 1000; // Event/input work serviced every pass, including bursts.
            DispatchBudget budget(start+std::chrono::microseconds(us));
            while (budget.CanDispatch(start+std::chrono::microseconds(us)) && !transport.empty()) {
                const auto pts=transport.front(); transport.pop_front(); budget.Dispatched();
                us += pts ? 4000 : 1000; // Variable work stays on the application thread.
                if (pts) {
                    playout.Add(Video(pts),start+std::chrono::microseconds(us)); ++decoded;
                    break; // The actual receiver yields after each produced picture.
                }
            }
            VideoFrame result;
            if (playout.Take(audioOn ? 10000000+us-60000 : 0,start+std::chrono::microseconds(us),result)) {
                Check(result.timestampMicros>lastPts,"bursty dispatch preserves prediction/picture order");
                if (submitted) maxGap=std::max(maxGap,us-lastPresentation);
                lastPts=result.timestampMicros; lastPresentation=us; ++submitted;
            }
            Check(playout.Size()<=AudioVideoQueue::MaxPendingFrames,"bursty runtime memory bound");
            ++passes; us+=2000;
        }
        Check(decoded>=297 && submitted>=decoded-4,"bursty runtime no longer overwrites intermediate pictures");
        Check(maxGap<=110000,"bounded dispatch prevents additional starvation under bursts");
        Check(passes>1000,"interaction receives regular opportunities during media work");
    }
}

void ChangingVideoSyncTests() {
    AudioVideoQueue queue;
    const auto start = Clock::now();
    unsigned presented = 0;
    std::uint64_t nextFrameMicros = 0, previousPts = 0;
    std::chrono::milliseconds previousPresentation{}, maximumGap{};
    for (unsigned ms=0;ms<8000;ms+=2) {
        const auto elapsed = std::chrono::milliseconds(ms);
        const auto micros = static_cast<std::uint64_t>(ms) * 1000;
        if (ms == 4000) queue.Clear(); // Session reset/opt-out discards pending pictures.
        if (micros >= nextFrameMicros) {
            queue.Add(Video(10000000 + micros),start + elapsed);
            nextFrameMicros += 33333;
        }
        std::uint64_t audible = 10000000 + micros - 20000;
        if (ms >= 1000 && ms < 2000) audible -= 110000;
        if (ms >= 3000 && ms < 4000) audible = 12980000; // Output clock stops advancing.
        if (ms >= 4000 && ms < 5000) audible = 0; // Audio off/mute/failure.
        if (ms >= 5000 && ms < 6000) audible -= (ms/50 % 2) ? 120000 : 40000;
        if (ms >= 6000 && ms < 7000) audible -= 230000;
        if (ms >= 7000) audible = 1; // Incompatible clock domain.
        VideoFrame result;
        if (queue.Take(audible,start + elapsed,result)) {
            Check(result.timestampMicros > previousPts,"changing/stalled clock cannot replay old video");
            Check(queue.Stats().lastHoldMicros <= 102000,"changing/stalled clock keeps per-picture wait bound");
            if (presented) maximumGap = std::max(maximumGap,elapsed - previousPresentation);
            previousPresentation = elapsed; previousPts = result.timestampMicros; ++presented;
            if (!audible || audible == 1) Check(!queue.Stats().haveEstimatedSkew,"off/incompatible clock has no skew estimate");
        }
        Check(queue.Size() <= AudioVideoQueue::MaxPendingFrames,"changing/stalled clock memory bound");
    }
    Check(presented >= 210 && maximumGap <= std::chrono::milliseconds(136),"clock transitions/jitter/stalls remain live");
}

void SettingsTests() {
    using namespace phonecast::vr;
    OverlaySettings original; SettingsMenuController menu; menu.Open(original);
    for (int i=0;i<6;++i) menu.Handle(SettingsMenuCommand::NextItem);
    menu.Handle(SettingsMenuCommand::Activate); Check(menu.View().title == "PHONE AUDIO","in-headset audio category");
    menu.Handle(SettingsMenuCommand::Activate); Check(menu.Draft().audioMuted,"mute toggle");
    menu.Handle(SettingsMenuCommand::NextItem); menu.Handle({SettingsMenuCommand::SetNormalized,0.25F});
    Check(menu.Draft().audioVolume == 0.25F,"volume slider");
    menu.Handle(SettingsMenuCommand::NextItem); menu.Handle(SettingsMenuCommand::Increase);
    Check(menu.Draft().audioUseSystemDefault,"output routing control");
    menu.Handle(SettingsMenuCommand::Back); menu.Handle(SettingsMenuCommand::Back);
    Check(menu.Original().audioVolume == 0.7F && !menu.Original().audioMuted,"cancel preserves original");
    const auto path = std::filesystem::temp_directory_path() / "phonecast-audio-test.ini";
    OverlaySettingsStore store(path); std::string error; bool found=false; OverlaySettings loaded;
    original.audioMuted=true; original.audioVolume=0.25F; original.audioUseSystemDefault=true;
    Check(store.Save(original,error) && store.Load(loaded,found,error) && found && loaded.audioMuted &&
        loaded.audioVolume == 0.25F && loaded.audioUseSystemDefault,"audio persistence");
    // Older settings have no audio fields and use safe, independent defaults.
    std::ifstream input(path); std::string text((std::istreambuf_iterator<char>(input)),{}); input.close();
    text.replace(text.find("version=5"),9,"version=3");
    std::ofstream output(path); output << text; output.close();
    Check(store.Load(loaded,found,error) && !loaded.audioMuted && loaded.audioVolume == 0.7F,"v3 migration");
    std::filesystem::remove(path);
}
bool ReceiveExact(SOCKET s, std::uint8_t* p, std::size_t n) {
    while (n) { int r=recv(s,reinterpret_cast<char*>(p),static_cast<int>(n),0); if (r<=0) return false; p+=r; n-=static_cast<std::size_t>(r); } return true;
}
void SendMessage(SOCKET s,const Message& m) {
    const auto bytes=Serialize(m); std::size_t sent=0;
    while (sent<bytes.size()) { int n=send(s,reinterpret_cast<const char*>(bytes.data()+sent),static_cast<int>(bytes.size()-sent),0); Check(n>0,"network send"); sent+=static_cast<std::size_t>(n); }
}
Message ReceiveMessage(SOCKET s) {
    std::uint8_t bytes[kHeaderSize]{}; Check(ReceiveExact(s,bytes,sizeof(bytes)),"network receive header");
    Message m; std::uint32_t n=0; std::string error; Check(ParseHeader(bytes,sizeof(bytes),m,n,error),"network parse header");
    m.payload.resize(n); if (n) Check(ReceiveExact(s,m.payload.data(),n),"network receive payload"); return m;
}
void NetworkTests(bool audioEnabled) {
    SOCKET probe=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP); Check(probe!=INVALID_SOCKET,"probe socket");
    sockaddr_in address{}; address.sin_family=AF_INET; address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    Check(bind(probe,reinterpret_cast<sockaddr*>(&address),sizeof(address)) == 0,"ephemeral test port");
#ifdef _WIN32
    int length=sizeof(address);
#else
    socklen_t length=sizeof(address);
#endif
    getsockname(probe,reinterpret_cast<sockaddr*>(&address),&length); const auto port=ntohs(address.sin_port); closesocket(probe);
    phonecast::platform::network::TcpVideoServer server("123456",port,audioEnabled); std::string error;
    Check(server.Start(error),"start server");
    SOCKET client=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    Check(Wait([&] { return connect(client,reinterpret_cast<sockaddr*>(&address),sizeof(address)) == 0; }),"connect loopback");
#ifdef _WIN32
    DWORD timeout=1500; setsockopt(client,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<const char*>(&timeout),sizeof(timeout));
#else
    timeval timeout{1,500000}; setsockopt(client,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
#endif
    Message hello; hello.type=MessageType::Hello; hello.payload={'1','2','3','4','5','6'}; SendMessage(client,hello);
    Check(Wait([&] { return server.Connected(); }),"pairing handshake");
    // Audio sent without negotiation cannot consume video queue or activate output.
    SendMessage(client,Config()); Check(Wait([&] { return server.AudioDropped()>0; }),"unnegotiated audio dropped");
    Message ping; ping.type=MessageType::Ping; ping.timestampMicros=123; SendMessage(client,ping);
    auto pong=ReceiveMessage(client); Check(pong.type==MessageType::Pong && pong.payload.empty() && pong.timestampMicros==123,"old sender unchanged");
    ping.payload=Capability(); SendMessage(client,ping); pong=ReceiveMessage(client);
    Check(audioEnabled ? HasCapability(pong.payload) : pong.payload.empty(),"capability/video-only fallback");
    Message read;
    if (audioEnabled) {
        SendMessage(client,Config());
        for (unsigned i=0;i<20;++i) SendMessage(client,Frame(i));
        Check(Wait([&] { return server.AudioDropped()>=11; }),"independent receiver audio queue bound");
        Check(server.PopAudio(read) && read.type==MessageType::AudioConfig,"configuration retained");
        Message vc; vc.type=MessageType::VideoConfig; vc.width=2; vc.height=2; vc.payload={1}; SendMessage(client,vc);
        Check(Wait([&] { return server.Pop(read); }),"video not starved by audio");
        Check(read.type==MessageType::VideoConfig,"video queue isolation");
        const auto reply=ReceiveMessage(client); Check(reply.type==MessageType::RequestKeyFrame,"video-only resync");
        Message off; off.type=MessageType::AudioStatus; off.payload=Status(9,CaptureStatus::Off); SendMessage(client,off);
        Check(Wait([&] { return server.PopAudio(read) && read.type==MessageType::AudioStatus; }),"opt-out flushes queued sound");
    } else Check(!server.PopAudio(read),"old receiver accepts no audio");
    closesocket(client); Check(Wait([&] { return !server.Connected(); }),"disconnect"); server.Stop();
}
}
int main() {
#ifdef _WIN32
    WSADATA data{}; WSAStartup(MAKEWORD(2,2),&data);
#endif
    try { ProtocolTests(); PlaybackTests(); SyncTests(); ContinuousVideoSyncTests(); MediaClockTests(); BurstyDispatchTests(); ChangingVideoSyncTests(); SettingsTests(); NetworkTests(true); NetworkTests(false); }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
#ifdef _WIN32
    WSACleanup();
#endif
    std::cout << "Audio protocol, playout, settings, and paired transport tests passed.\n"; return 0;
}
