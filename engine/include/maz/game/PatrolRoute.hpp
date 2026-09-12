#pragma once

#include "maz/math/Math.hpp" // vec2

#include <cmath>
#include <cstddef>
#include <vector>

// maz::game patrol route — the discrete waypoint patrol every guard, sentry and roaming enemy walks: a list
// of points, each with an optional DWELL time, traversed at a set speed in one of three modes (Once, Loop,
// PingPong). It owns the mover's position, walks it toward the current waypoint, PAUSES there for the
// waypoint's dwell on arrival, then advances to the next per the mode. It reports the target index, whether
// it is dwelling, whether the route has finished (Once), and a one-frame "just arrived" pulse to hang
// events on (play an animation, sweep a view cone, bark a line).
//
// This is deliberately distinct from `game::PathFollow2D`, which slides CONTINUOUSLY along a baked Curve2D by
// an arc-length offset with a single wrap flag — no discrete waypoints, no per-point pause, no ping-pong,
// no arrival event. A patrol is a stop-and-go tour of specific spots, which is what stealth/AI code wants
// (and pairs naturally with the M571 DetectionMeter for a guard that patrols until it spots you). Godot
// ships neither a patrol node nor dwell/ping-pong traversal. Header-only, std-only, deterministic.
namespace maz::game {

enum class PatrolMode { Once, Loop, PingPong };

struct Waypoint {
    math::vec2 pos{0.0f, 0.0f};
    float wait = 0.0f; // seconds to dwell on arrival before moving on
};

class PatrolRoute {
public:
    explicit PatrolRoute(PatrolMode mode = PatrolMode::Loop) : m_mode(mode) {}

    void addWaypoint(const math::vec2& p, float wait = 0.0f) {
        m_points.push_back(Waypoint{p, wait < 0.0f ? 0.0f : wait});
        if (m_points.size() == 1) m_pos = p; // start sitting on the first point
    }

    void setMode(PatrolMode mode) { m_mode = mode; }
    PatrolMode mode() const { return m_mode; }
    std::size_t size() const { return m_points.size(); }

    // Restart: sit on the first waypoint heading to the next, clear dwell/finished/direction.
    void reset() {
        m_target = m_points.size() > 1 ? 1 : 0;
        m_dir = 1;
        m_waitLeft = 0.0f;
        m_waiting = false;
        m_finished = false;
        m_arrived = false;
        if (!m_points.empty()) m_pos = m_points.front().pos;
    }

    // Move toward the current target for `dt` seconds at `speed` units/sec, dwelling on arrival and then
    // advancing per the mode. Non-positive dt/speed, an empty or single-point route, or a finished Once
    // route are no-ops (a single-point route just sits on its point). Fewer than 2 points never moves.
    void update(float speed, float dt) {
        m_arrived = false;
        if (dt <= 0.0f || m_points.size() < 2 || m_finished) return;

        if (m_waiting) {
            m_waitLeft -= dt;
            if (m_waitLeft <= 0.0f) {
                m_waiting = false;
                advanceTarget();
            }
            return; // a dwell consumes the whole frame
        }
        if (speed <= 0.0f) return;

        const math::vec2 to = m_points[static_cast<std::size_t>(m_target)].pos - m_pos;
        const float dist = std::sqrt(to.x * to.x + to.y * to.y);
        const float travel = speed * dt;
        if (dist <= travel || dist < 1e-6f) {
            m_pos = m_points[static_cast<std::size_t>(m_target)].pos; // snap onto the waypoint
            m_arrived = true;
            const float w = m_points[static_cast<std::size_t>(m_target)].wait;
            if (w > 0.0f) {
                m_waiting = true;
                m_waitLeft = w;
            } else {
                advanceTarget(); // no dwell: pick the next target immediately
            }
        } else {
            m_pos += to * (travel / dist);
        }
    }

    math::vec2 position() const { return m_pos; }
    int targetIndex() const { return m_target; }        // waypoint being moved toward / dwelt on
    bool isWaiting() const { return m_waiting; }
    bool isFinished() const { return m_finished; }        // Once mode reached the last waypoint
    bool arrivedThisUpdate() const { return m_arrived; }  // one-frame pulse when a waypoint is reached
    float waitRemaining() const { return m_waiting ? m_waitLeft : 0.0f; }
    const std::vector<Waypoint>& waypoints() const { return m_points; }

private:
    void advanceTarget() {
        const int n = static_cast<int>(m_points.size());
        switch (m_mode) {
            case PatrolMode::Loop:
                m_target = (m_target + 1) % n;
                break;
            case PatrolMode::Once:
                if (m_target >= n - 1) m_finished = true;
                else ++m_target;
                break;
            case PatrolMode::PingPong:
                if (m_target + m_dir >= n || m_target + m_dir < 0) m_dir = -m_dir; // bounce at an endpoint
                m_target += m_dir;
                break;
        }
    }

    PatrolMode m_mode;
    std::vector<Waypoint> m_points;
    math::vec2 m_pos{0.0f, 0.0f};
    int m_target = 1; // first thing to do is head from waypoint 0 to waypoint 1
    int m_dir = 1;    // PingPong direction
    float m_waitLeft = 0.0f;
    bool m_waiting = false;
    bool m_finished = false;
    bool m_arrived = false;
};

} // namespace maz::game
