#pragma once
// The waypoint the player placed on the pause map (DingoMapPOIType_Waypoint), read from the
// map's POI registry so the trainer can teleport to it. Game thread only.
#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace dingosdk::trainer {
struct MapWaypoint {
    bool known{};                    // a registry walk has completed at least once
    std::optional<std::array<float, 3>> position; // the placed waypoint, if any
    std::string detail;              // diagnostic: what the last walk saw
    unsigned pois{};                 // map POIs registered at the last walk (0: map not built)
};
// Walks the registry at most every `interval_ms`; returns the cached result between walks.
void forget_map_waypoint() noexcept; // a new level: the old waypoint belongs to the old map
const MapWaypoint &poll_map_waypoint(std::uintptr_t base, std::uint64_t now, std::uint64_t interval_ms = 250) noexcept;
}
