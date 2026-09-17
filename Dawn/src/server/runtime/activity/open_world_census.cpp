#include <Windows.h>
#include "open_world_census.h"
#include "../../../core/filesystem/path.h"
#include "../../../core/settings/settings.h"
#include "../../../state/activity/open_world_member_observations.h"
#include <atomic>
#include <charconv>
#include <cstdio>
#include <mutex>
namespace dawn::server::runtime::activity::open_world_census {
namespace members = state::activity::open_world_members;
namespace {
constexpr std::size_t kLineBytes = 2048;
constexpr std::size_t kQueueCapacity = 256;
constexpr std::uint64_t kMaximumRecords = 100000;
constexpr std::uint64_t kMaximumBytes = 16ULL * 1024ULL * 1024ULL;

struct Line final {
    std::array<char, kLineBytes> bytes{};
    std::uint16_t length{};
};

struct Sink final {
    std::mutex mutex;
    testing::BoundedQueue<Line, kQueueCapacity> queue{};
    HANDLE wake{}, stop{}, worker{};
    std::atomic<std::uint64_t> sequence{}, dropped{}, ioFailures{}, producers{};
    std::uint64_t start{};
    std::atomic<bool> active{};
};
Sink sink;

class Writer final {
public:
    explicit Writer(std::span<char> output) : output_(output) {}
    bool text(std::string_view value) {
        if (size_ + value.size() > output_.size()) return false;
        for (char character : value) output_[size_++] = character;
        return true;
    }
    bool string(std::string_view value) {
        if (!text("\"")) return false;
        for (unsigned char character : value) {
            if (character == '\"' || character == '\\') {
                if (!text("\\") || !one(static_cast<char>(character))) return false;
            } else if (character < 0x20) {
                if (!text("?")) return false;
            } else if (!one(static_cast<char>(character))) return false;
        }
        return text("\"");
    }
    template<class T> bool number(T value) {
        std::array<char, 32> bytes{};
        const auto result = std::to_chars(bytes.data(), bytes.data() + bytes.size(), value);
        return result.ec == std::errc{}
            && text({bytes.data(), static_cast<std::size_t>(result.ptr - bytes.data())});
    }
    bool hex(std::uint64_t value) {
        std::array<char, 20> bytes{};bytes[0] = '\"';bytes[1] = '0';bytes[2] = 'x';
        const auto result = std::to_chars(bytes.data() + 3, bytes.data() + 19, value, 16);
        if (result.ec != std::errc{}) return false;
        *result.ptr = '\"';
        return text({bytes.data(), static_cast<std::size_t>(result.ptr - bytes.data() + 1)});
    }
    [[nodiscard]] std::size_t size() const { return size_; }
private:
    bool one(char value) {
        if (size_ == output_.size()) return false;
        output_[size_++] = value;return true;
    }
    std::span<char> output_;
    std::size_t size_{};
};

std::string_view name(Event event) {
    switch (event) {
    case Event::runStart:return "run_start";case Event::boundary:return "boundary";
    case Event::sourceRequest:return "source_request";case Event::sourceObservation:return "source_observation";
    case Event::taskSelection:return "task_selection";case Event::actorAdmitted:return "actor_admitted";
    case Event::actorDied:return "actor_died";case Event::actorRetired:return "actor_retired";
    case Event::sourceRecreated:return "source_recreated";case Event::sourceSnapshot:return "source_snapshot";
    case Event::renewalStarted:return "renewal_started";case Event::renewalBusy:return "renewal_busy";
    case Event::renewalCancelled:return "renewal_cancelled";case Event::renewalCommitted:return "renewal_committed";
    case Event::failure:return "failure";
    }
    return "failure";
}

HANDLE create_file() noexcept {
    core::path::Buffer directory{};
    if (!core::path::artifact_directory(GetModuleHandleW(L"steam_api64.dll"), directory)
        || !core::path::append(directory, L"\\logs")
        || (!CreateDirectoryW(directory.chars.data(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS))
        return INVALID_HANDLE_VALUE;
    for (unsigned run = 1; run <= 9999; ++run) {
        wchar_t suffix[40]{};
        if (swprintf_s(suffix, L"\\open-world-census-%04u.jsonl", run) <= 0) return INVALID_HANDLE_VALUE;
        auto path = directory;
        if (!core::path::append(path, suffix)) return INVALID_HANDLE_VALUE;
        const auto file = CreateFileW(path.chars.data(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
            CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file != INVALID_HANDLE_VALUE) return file;
        if (GetLastError() != ERROR_FILE_EXISTS) return INVALID_HANDLE_VALUE;
    }
    return INVALID_HANDLE_VALUE;
}

bool dequeue(Line& output) noexcept {
    std::lock_guard lock(sink.mutex);
    return sink.queue.pop(output);
}
DWORD WINAPI worker_main(void*)noexcept {
    auto file=create_file();if(file==INVALID_HANDLE_VALUE)++sink.ioFailures;
    std::uint64_t records{},bytes{};bool stopping{},limitReached{};HANDLE waits[]{sink.stop,sink.wake};
    while(!stopping) {
        const auto wait=WaitForMultipleObjects(2,waits,FALSE,INFINITE);stopping=wait==WAIT_OBJECT_0;
        if(wait!=WAIT_OBJECT_0&&wait!=WAIT_OBJECT_0+1){++sink.ioFailures;break;}
        Line line{};
        while(dequeue(line)) {
            if(file==INVALID_HANDLE_VALUE||records>=kMaximumRecords-1||bytes+line.length>kMaximumBytes-512) {
                ++sink.dropped;limitReached|=file!=INVALID_HANDLE_VALUE;continue;
            }
            DWORD written{};
            if(!WriteFile(file,line.bytes.data(),line.length,&written,nullptr)||written!=line.length) {
                bytes+=written;++sink.ioFailures;CloseHandle(file);file=INVALID_HANDLE_VALUE;
            } else {++records;bytes+=written;}
        }
    }
    if(file!=INVALID_HANDLE_VALUE) {
        std::array<char,512> summary{};
        const auto length=std::snprintf(summary.data(),summary.size(),
            "{\"schema\":\"open_world_census/v1\",\"event\":\"diagnostic_summary\",\"dropped_records\":%llu,\"io_failures\":%llu,\"member_diagnostic_loss_events\":%llu,\"limit_reached\":%s}\n",
            sink.dropped.load(),sink.ioFailures.load(),members::losses(),limitReached?"true":"false");
        DWORD written{};
        if(length<=0||static_cast<std::size_t>(length)>=summary.size()
            || !WriteFile(file,summary.data(),static_cast<DWORD>(length),&written,nullptr)
            || written!=static_cast<DWORD>(length))++sink.ioFailures;
        FlushFileBuffers(file);CloseHandle(file);
    }
    return 0;
}
}
bool format(const Record& r, std::uint64_t sequence, std::uint64_t elapsed,
    std::span<char> output, std::size_t& length) noexcept {
    Writer writer(output);length = 0;
    const auto key = [&](std::string_view value) {return writer.string(value) && writer.text(":");};
    const auto comma = [&]() {return writer.text(",");};
#define F(x) do{if(!(x))return false;}while(false)
#define K(x) F(comma()&&key(x))
    F(writer.text("{") && key("schema") && writer.string("open_world_census/v1"));
    K("seq");F(writer.number(sequence));K("elapsed_ms");F(writer.number(elapsed));
    K("event");F(writer.string(name(r.event)));K("destination");F(writer.string(r.destination));
    K("scenario");F(writer.hex(r.scenario));K("bubble");F(writer.number(r.bubble));
    K("source_bubble");F(r.registry?writer.number(r.sourceBubble):writer.text("null"));
    K("arrived");F(writer.text(r.arrived?"true":"false"));K("registry");F(r.registry?writer.hex(r.registry):writer.text("null"));
    K("source_slot");F(r.registry?writer.number(r.sourceSlot):writer.text("null"));
    K("generation");F(r.generation?writer.number(r.generation):writer.text("null"));
    K("request_seq");F(r.requestSequence?writer.number(r.requestSequence):writer.text("null"));
    K("requested_first");F(writer.number(r.requestedFirst));K("requested_second");F(writer.number(r.requestedSecond));
    K("consumed_first");F(r.consumedKnown?writer.number(r.consumedFirst):writer.text("null"));
    K("consumed_second");F(r.consumedKnown?writer.number(r.consumedSecond):writer.text("null"));
    K("has_rule");F(r.registry?writer.text(r.hasRule?"true":"false"):writer.text("null"));
    K("rule_slot");F(r.registry&&r.hasRule?writer.number(r.ruleSlot):writer.text("null"));
    for(const auto unknown:{"rule_guid","position","type","rank","entity"}){K(unknown);F(writer.text("null"));}
    K("category");F(r.member.known?writer.number(r.member.category):writer.text("null"));
    K("category_key");F(r.member.known?writer.hex(r.member.categoryKey):writer.text("null"));
    K("template_entity");F(r.member.known?writer.hex(r.member.entity):writer.text("null"));
    K("choice_variant");F(r.member.known?writer.number(r.member.variant):writer.text("null"));
    K("choice_ordinal");F(r.member.known?writer.number(r.member.choice):writer.text("null"));
    K("choice_weight");F(r.member.known?writer.number(r.member.weight):writer.text("null"));
    K("member_kind");F(r.member.known?writer.hex(r.member.kind):writer.text("null"));
    K("member_offset");F(r.member.known?writer.number(r.member.offset):writer.text("null"));
    K("metadata_evidence");F(writer.string(r.member.evidence));
    K("tactical_provider");F(r.tacticalProvider?writer.hex(r.tacticalProvider):writer.text("null"));
    K("tactical_slot");F(r.tacticalProvider?writer.number(r.tacticalSlot):writer.text("null"));
    K("tactical_row");F(r.tacticalRow>=0?writer.number(r.tacticalRow):writer.text("null"));
    K("tactical_revision");F(r.tacticalRevision?writer.number(r.tacticalRevision):writer.text("null"));
    K("admitted");F(r.hasCounts?writer.number(r.counts.admitted):writer.text("null"));
    K("live");F(r.hasCounts?writer.number(r.counts.alive):writer.text("null"));
    K("dead");F(r.hasCounts?writer.number(r.counts.dead):writer.text("null"));
    K("resident");F(r.hasCounts?writer.number(r.counts.resident):writer.text("null"));
    K("retired");F(r.hasCounts&&r.counts.resident<=r.counts.admitted
        ?writer.number(r.counts.admitted-r.counts.resident):writer.text("null"));
    K("source_retired");F(r.hasCounts?writer.text(r.counts.sourceRetired?"true":"false"):writer.text("null"));
    K("failed");F(writer.text((r.failed||(r.hasCounts&&r.counts.failed)
        ||(r.hasCounts&&r.counts.resident>r.counts.admitted))?"true":"false"));
    K("reason");F(r.reason.empty()?writer.text("null"):writer.string(r.reason));
    K("prior_bubble");F(r.event==Event::boundary?writer.number(r.priorBubble):writer.text("null"));
    K("prior_arrived");F(r.event==Event::boundary?writer.text(r.priorArrived?"true":"false"):writer.text("null"));
    K("prior_generation");F(r.priorGeneration?writer.number(r.priorGeneration):writer.text("null"));
    K("next_generation");F(r.nextGeneration?writer.number(r.nextGeneration):writer.text("null"));
    K("dropped_records");F(writer.number(sink.dropped.load()));
    K("io_failures");F(writer.number(sink.ioFailures.load()));F(writer.text("}\n"));
    length=writer.size();return true;
#undef K
#undef F
}
bool enabled() noexcept {
    return core::settings::get().omegaExperiments.openWorldCensus;
}

Stats stats() noexcept {
    return {sink.dropped.load(), sink.ioFailures.load()};
}

bool initialize() noexcept {
    if (!enabled() || sink.active.load()) return true;
    sink.start = GetTickCount64();
    sink.wake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    sink.stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!sink.wake || !sink.stop) {
        if (sink.wake) CloseHandle(sink.wake);
        if (sink.stop) CloseHandle(sink.stop);
        sink.wake = sink.stop = nullptr;
        return false;
    }
    sink.worker = CreateThread(nullptr, 0, &worker_main, nullptr, 0, nullptr);
    if (!sink.worker) {
        CloseHandle(sink.wake);CloseHandle(sink.stop);
        sink.wake = sink.stop = nullptr;
        return false;
    }
    members::start();
    sink.active.store(true, std::memory_order_release);
    return true;
}

void shutdown() noexcept {
    if (!sink.active.exchange(false)) return;
    members::stop();
    while (sink.producers.load(std::memory_order_acquire)) Sleep(0);
    if (sink.stop) SetEvent(sink.stop);
    if (sink.worker) {
        WaitForSingleObject(sink.worker, INFINITE);
        CloseHandle(sink.worker);
    }
    if (sink.wake) CloseHandle(sink.wake);
    if (sink.stop) CloseHandle(sink.stop);
    sink.worker = sink.wake = sink.stop = nullptr;
    std::lock_guard lock(sink.mutex);
    sink.queue.clear();
}

void emit(const Record& record) noexcept {
    if (!sink.active.load(std::memory_order_acquire)) return;
    ++sink.producers;
    if (!sink.active.load(std::memory_order_acquire)) {
        --sink.producers;
        return;
    }
    // Serialize sequence allocation with enqueue order. This lock is try-only
    // and covers bounded formatting/copying, never filesystem I/O.
    if (!sink.mutex.try_lock()) {
        ++sink.dropped;--sink.producers;return;
    }
    Line line{};std::size_t length{};
    const auto sequence = sink.sequence.fetch_add(1) + 1;
    if (!format(record, sequence, GetTickCount64() - sink.start, line.bytes, length)) {
        sink.mutex.unlock();
        ++sink.dropped;--sink.producers;return;
    }
    line.length = static_cast<std::uint16_t>(length);
    const bool queued = sink.queue.push(line);
    sink.mutex.unlock();
    if (!queued) ++sink.dropped;
    else SetEvent(sink.wake);
    --sink.producers;
}
}
