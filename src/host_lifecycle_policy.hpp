#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

namespace OutRunLifecycle {
inline constexpr char ExactDiskSha256[] = "68ceb386829066f8455b9d027320af962584321f3e2e8a79c72841495a6134c3";
inline constexpr std::uint32_t BootstrapRva = 0x18080, LoopCallRva = 0x176EE, CleanupCallRva = 0x176F3;
struct Signature { std::uint32_t rva; std::vector<unsigned char> bytes; int relocation = -1; };
inline std::vector<Signature> CriticalSignatures() {
    return {
        {BootstrapRva, {0x8B,0x0D,0x8C,0xDB,0x74,0x00,0x8B,0x44,0x24,0x04}, 2},
        {0x181CF, {0xA3,0x88,0x8C,0x8A,0x00}, 1},
        {LoopCallRva, {0xE8,0x2D,0x04,0x00,0x00}},
        {CleanupCallRva, {0xE8,0x38,0x07,0x00,0x00}},
        {0x17BE0, {0x83,0x7C,0x24,0x14,0x12,0x0F,0x84,0x31,0x02,0x00,0x00}},
        {0x17E10, {0x39,0x1D,0xAC,0x8C,0x8A,0x00,0x0F,0x84,0xB4,0xFD,0xFF,0xFF}, 2},
        {0x17FAF, {0xFF,0x15,0xC0,0x61,0x59,0x00}, 2}
    };
}
inline std::vector<unsigned char> RelocatedBytes(Signature signature, std::uint32_t imageBase) {
    if (signature.relocation >= 0) {
        std::uint32_t value;
        std::memcpy(&value, signature.bytes.data()+signature.relocation, sizeof(value));
        value += imageBase - 0x400000u;
        std::memcpy(signature.bytes.data()+signature.relocation, &value, sizeof(value));
    }
    return signature.bytes;
}
inline bool DiskIdentityMatches(const char* hash) { return hash && std::strcmp(hash, ExactDiskSha256) == 0; }
}
