#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace repp {

struct RegistryValue {
    std::wstring name;
    uint32_t type = 0;
    std::vector<uint8_t> data;
    std::wstring preview;
    uint64_t dataOffset = 0;
    uint64_t dataLength = 0;
};

struct RegistryKey {
    std::wstring name;
    std::wstring path;
    uint32_t cellOffset = 0;
    uint64_t lastWrite = 0;
    uint32_t flags = 0;
    uint32_t parentCell = 0xFFFFFFFFu;
    uint32_t skOffset = 0xFFFFFFFFu;
    uint32_t classOffset = 0xFFFFFFFFu;
    std::wstring className;
    std::vector<uint32_t> subkeys;
    std::vector<uint32_t> values;
};

struct RegistryHive {
    std::wstring filePath;
    std::wstring rootName;
    uint64_t written = 0;
    std::vector<uint8_t> bytes;
    std::vector<RegistryKey> keys;
    std::vector<RegistryValue> values;
    uint32_t rootOffset = 0xFFFFFFFFu;
    std::wstring error;
    bool ok = false;
};

bool hive_open(RegistryHive& hive, const std::wstring& path);
bool hive_dump(const RegistryHive& hive, std::wstring& out);

std::wstring value_type_name(uint32_t type);
std::wstring value_preview(const RegistryValue& value);
std::wstring hex_bytes(const uint8_t* data, size_t count, size_t maxBytes = 64);
std::wstring format_filetime(uint64_t ft);
uint64_t filetime_now();
std::wstring lower(const std::wstring& value);
std::wstring lower(const char* value);

}
