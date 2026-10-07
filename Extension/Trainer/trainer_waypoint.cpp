#include "trainer_waypoint.h"
#include "Engine/Core/Platform/memory.h"
#include "Engine/Game/Abi/native_data.h"
#include "Engine/Game/Build/addresses.h"
#include "Engine/Game/Build/20260929/native_party.h"
#include <cmath>
#include <format>
#include <map>

// The pause map keeps every point of interest in one registry (the map POI manager, the same
// one ReSkate's party markers register into: native_party_hooks.cpp). A waypoint the player
// places with the map cursor is a POI whose kind (MapObjectData @404) is
// DingoMapPOIType_Waypoint (3); its world transform is the matrix at @0 (field 9).
namespace dingosdk::trainer {
namespace {
namespace party = addr::native_party;
using Address = std::uintptr_t;
using Handle = std::uint64_t;
constexpr std::uint32_t kind_waypoint = 3;
constexpr std::uint32_t kind_offset = 404;

template<class T> bool get(Address address, T &value) { return address && memory::peek(address, value); }

Address model_type(Address base, Address manager, Handle handle) {
    const auto record = reinterpret_cast<Address (*)(Address, Handle, std::uint8_t)>(base + party::model_record)(manager, handle, 0);
    if (!record) return 0;
    return reinterpret_cast<Address (*)(Address, Handle, Address, std::uint8_t)>(base + party::model_type)(manager, handle, record, 0);
}

MapWaypoint walk(Address base) {
    MapWaypoint out;
    Address map{}, vtable{}, manager{};
    if (!get(base + party::map_manager, map) || !map) { out.detail = "no map POI manager"; return out; }
    if (!get(map, vtable) || vtable != base + party::map_manager_vtable) { out.detail = "map POI manager type differs"; return out; }
    if (!get(map + 0x18, manager) || !manager) { out.detail = "no map model manager"; return out; }
    auto &n = game::native_data().models;
    if (!n.value || !n.lock || !n.unlock) { out.detail = "native model API unavailable"; return out; }
    game::ModelWriteLock lock(manager);
    Address table{};
    std::uint32_t buckets{}, count{};
    if (!get(map + 0x48, table) || !get(map + 0x50, buckets) || !get(map + 0x54, count) || !table ||
        buckets > 65536 || count > 20000) { out.detail = "map POI registry unreadable"; return out; }
    const auto poi_type = base + party::map_poi_type;
    std::map<std::uint32_t, unsigned> kinds;
    unsigned visited{};
    for (std::uint32_t i = 0; i < buckets && visited < count; ++i) {
        Address node{};
        if (!get(table + i * 8, node)) break;
        while (node && visited++ < count) {
            Handle handle{};
            if (!get(node, handle)) break;
            if (handle && model_type(base, manager, handle) == poi_type) {
                if (const auto value = n.value(manager, handle, 0, 0)) {
                    std::uint32_t kind{};
                    std::array<float, 16> world{};
                    if (get(value + kind_offset, kind)) ++kinds[kind];
                    if (kind == kind_waypoint && !out.position && get(value, world) &&
                        std::isfinite(world[12]) && std::isfinite(world[13]) && std::isfinite(world[14]) &&
                        std::abs(world[12]) < 1e6f && std::abs(world[13]) < 1e6f && std::abs(world[14]) < 1e6f)
                        out.position = std::array<float, 3>{world[12], world[13], world[14]};
                }
            }
            if (!get(node + 8, node)) break;
        }
    }
    out.known = true;
    out.pois = visited;
    out.detail = std::format("{} POIs:", visited);
    for (const auto &[kind, number] : kinds) out.detail += std::format(" kind{}={}", kind, number);
    return out;
}
} // namespace

namespace {
MapWaypoint &cache() { static MapWaypoint value; return value; }
}
void forget_map_waypoint() noexcept { cache() = {}; }

const MapWaypoint &poll_map_waypoint(std::uintptr_t base, std::uint64_t now, std::uint64_t interval_ms) noexcept {
    auto &cached = cache();
    static std::uint64_t next{};
    if (!base || now < next) return cached;
    next = now + interval_ms;
    try {
        auto fresh = walk(base);
        // The registry may only be populated while the map screen is open. Keep the last
        // waypoint seen while it can't be read or is empty; a populated registry without a
        // waypoint means the player removed it.
        const bool populated = fresh.known && fresh.detail.find("kind") != std::string::npos;
        if (fresh.position || populated) cached = std::move(fresh);
        else { cached.detail = fresh.detail; cached.pois = fresh.pois; }
    } catch (const std::exception &e) {
        cached.detail = e.what();
    } catch (...) {
        cached.detail = "map POI walk failed";
    }
    return cached;
}
} // namespace dingosdk::trainer
