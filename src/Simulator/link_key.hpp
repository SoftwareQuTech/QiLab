/*
** Created on: 22.07.2026
** Created by: Claude
** Description: Shared link-key encoding for per-link maps (entanglement
**              attempt models, entanglement channel models, Bell-state pair
**              caching). Order-independent: encode_link_key(a, b) ==
**              encode_link_key(b, a).
*/

#pragma once
#include <cstdint>

inline uint16_t encode_link_key(uint8_t node_a, uint8_t node_b) noexcept
{
    const uint8_t lo = (node_a < node_b) ? node_a : node_b;
    const uint8_t hi = (node_a < node_b) ? node_b : node_a;
    return (static_cast<uint16_t>(lo) << 8) | static_cast<uint16_t>(hi);
}
