#pragma once

#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

// maz::core::Telemetry — an OPT-IN, privacy-first analytics/crash-reporting buffer, Maz's answer to
// Godot's opt-in usage reporting. The contract, in order of importance:
//
//   1. OFF BY DEFAULT. Nothing is recorded until enable(true) is called with explicit consent.
//      Every event()/count()/timing() while disabled is silently dropped (and counted, so a game
//      can show "N events withheld — telemetry is off"). There is no implicit or first-run opt-in.
//   2. LOCAL FIRST. Events accumulate in an in-memory buffer and serialize to newline-delimited
//      JSON (JSONL) — the same shape you'd write to a local file or POST to a sink. There is no
//      built-in network transport: flush() hands the batch to a sink callback YOU install (write a
//      file, upload, or drop it), so the engine never phones home on its own.
//   3. NO PII. An event is a short name plus small string/number fields. The session id is a
//   caller-
//      supplied opaque token (use a random id, never a username/email/device id). Field values are
//      whatever the game passes — keep them anonymous; this module makes it easy to stay clean but
//      can't read minds.
//
// Determinism: events carry a monotonically increasing sequence number (not a wall-clock time), so
// the JSONL is byte-stable and unit-tests exactly. A game can add its own timestamp field if it
// wants one. Header-only, no GPU, no threads — pumped from the game thread.
namespace maz::core {

class Telemetry {
  public:
    // One recorded event: a name, a sequence number, and small typed fields.
    struct Event {
        std::string name;
        uint64_t seq = 0;
        std::vector<std::pair<std::string, std::string>> strFields;
        std::vector<std::pair<std::string, double>> numFields;
    };
    using Sink = std::function<void(const std::string& jsonl)>;

    // Identify the session/app for the batch header. The session id should be an opaque random
    // token (see the NO PII note) — never a username or hardware id.
    void configure(std::string sessionId, std::string app, std::string version) {
        m_session = std::move(sessionId);
        m_app = std::move(app);
        m_version = std::move(version);
    }

    // Opt in / out. Recording only happens while enabled(). Disabling does NOT clear
    // already-buffered events (call clear() for that); it just stops new ones.
    void enable(bool consent) { m_enabled = consent; }
    bool enabled() const { return m_enabled; }

    // Where a flush() batch goes (write a file, upload, etc.). Optional — without a sink, flush()
    // still returns the JSONL so the caller can handle it.
    void setSink(Sink sink) { m_sink = std::move(sink); }

    // Record a general event with string + numeric fields. No-op (counted as dropped) when
    // disabled.
    void event(const std::string& name,
               std::vector<std::pair<std::string, std::string>> strFields = {},
               std::vector<std::pair<std::string, double>> numFields = {}) {
        if (!m_enabled) {
            ++m_dropped;
            return;
        }
        Event e;
        e.name = name;
        e.seq = m_seq++;
        e.strFields = std::move(strFields);
        e.numFields = std::move(numFields);
        m_buffer.push_back(std::move(e));
        ++m_recorded;
    }

    // Convenience: bump a named counter (recorded as an event with a {"value":n} field).
    void count(const std::string& name, double n = 1.0) { event(name, {}, {{"value", n}}); }

    // Convenience: record a timing/measurement in milliseconds.
    void timing(const std::string& name, double ms) { event(name, {}, {{"ms", ms}}); }

    // Serialize the buffered events to JSONL and hand them to the sink (if set), then clear the
    // buffer. Returns the JSONL text (empty if nothing buffered). Each line is one self-describing
    // object carrying the session/app header, so batches are independently ingestible.
    std::string flush() {
        std::string out;
        for (const Event& e : m_buffer) {
            out += serialize(e);
            out += '\n';
        }
        m_buffer.clear();
        if (m_sink && !out.empty()) {
            m_sink(out);
        }
        return out;
    }

    // Drop buffered events without emitting them (e.g. the user revoked consent mid-session).
    void clear() { m_buffer.clear(); }

    size_t pending() const { return m_buffer.size(); }
    uint64_t recorded() const { return m_recorded; } // lifetime events accepted
    uint64_t dropped() const { return m_dropped; }   // lifetime events dropped while disabled

  private:
    static std::string escape(const std::string& s) {
        std::string o;
        o.reserve(s.size() + 2);
        for (char c : s) {
            switch (c) {
            case '"':
                o += "\\\"";
                break;
            case '\\':
                o += "\\\\";
                break;
            case '\n':
                o += "\\n";
                break;
            case '\t':
                o += "\\t";
                break;
            case '\r':
                o += "\\r";
                break;
            default:
                o += c;
            }
        }
        return o;
    }

    static std::string num(double v) {
        // Integer-valued doubles print without a trailing ".000000"; others use %g.
        char buf[32];
        if (v == static_cast<double>(static_cast<long long>(v))) {
            std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(v));
        } else {
            std::snprintf(buf, sizeof(buf), "%g", v);
        }
        return buf;
    }

    std::string serialize(const Event& e) const {
        std::string o = "{";
        o += "\"session\":\"" + escape(m_session) + "\",";
        o += "\"app\":\"" + escape(m_app) + "\",";
        o += "\"v\":\"" + escape(m_version) + "\",";
        o += "\"seq\":" + num(static_cast<double>(e.seq)) + ",";
        o += "\"name\":\"" + escape(e.name) + "\"";
        for (const auto& [k, v] : e.strFields) {
            o += ",\"" + escape(k) + "\":\"" + escape(v) + "\"";
        }
        for (const auto& [k, v] : e.numFields) {
            o += ",\"" + escape(k) + "\":" + num(v);
        }
        o += "}";
        return o;
    }

    bool m_enabled = false; // OFF by default — opt-in only
    std::string m_session, m_app, m_version;
    std::vector<Event> m_buffer;
    Sink m_sink;
    uint64_t m_seq = 0;
    uint64_t m_recorded = 0;
    uint64_t m_dropped = 0;
};

} // namespace maz::core
