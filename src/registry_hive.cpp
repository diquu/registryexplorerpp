#include "registry_hive.h"

#include <windows.h>
#include <algorithm>
#include <cstring>
#include <cwchar>
#include <unordered_map>
#include <cwctype>
#include <functional>
#include <iostream>
#include <cstdio>

namespace repp {

uint64_t filetime_now() {
    FILETIME ft = {};
    GetSystemTimeAsFileTime(&ft);
    return (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

std::wstring lower(const std::wstring& value) {
    std::wstring out = value;
    for (wchar_t& c : out) {
        c = static_cast<wchar_t>(towlower(c));
    }
    return out;
}

std::wstring lower(const char* value) {
    std::wstring out;
    if (value == nullptr) {
        return out;
    }
    while (*value) {
        out += static_cast<wchar_t>(tolower(static_cast<unsigned char>(*value)));
        ++value;
    }
    return out;
}

std::wstring format_filetime(uint64_t ft) {
    SYSTEMTIME st = {};
    FILETIME local = {}, in = {};
    in.dwHighDateTime = static_cast<DWORD>(ft >> 32);
    in.dwLowDateTime = static_cast<DWORD>(ft & 0xFFFFFFFFu);
    if (!FileTimeToLocalFileTime(&in, &local) || !FileTimeToSystemTime(&local, &st)) {
        return L"--";
    }
    wchar_t buffer[64];
    swprintf_s(buffer, L"%04d-%02d-%02d %02d:%02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute,
               st.wSecond);
    return buffer;
}

std::wstring hex_bytes(const uint8_t* data, size_t count, size_t maxBytes) {
    std::wstring out;
    size_t shown = std::min(count, maxBytes);
    for (size_t i = 0; i < shown; ++i) {
        wchar_t pair[4];
        swprintf_s(pair, L"%02X ", data[i]);
        out += pair;
    }
    if (count > maxBytes) {
        out += L"...";
    }
    if (!out.empty() && out.back() == L' ') {
        out.pop_back();
    }
    return out;
}

std::wstring value_type_name(uint32_t type) {
    switch (type) {
        case 0: return L"REG_NONE";
        case 1: return L"REG_SZ";
        case 2: return L"REG_EXPAND_SZ";
        case 3: return L"REG_BINARY";
        case 4: return L"REG_DWORD";
        case 5: return L"REG_DWORD_BIG_ENDIAN";
        case 6: return L"REG_LINK";
        case 7: return L"REG_MULTI_SZ";
        case 8: return L"REG_RESOURCE_LIST";
        case 9: return L"REG_FULL_RESOURCE_DESCRIPTOR";
        case 10: return L"REG_RESOURCE_REQUIREMENTS_LIST";
        case 11: return L"REG_QWORD";
        case 16: return L"REG_FILETIME";
        case 17: return L"REG_UWP_UINT32";
        case 18: return L"REG_UWP_INT64";
        case 19: return L"REG_UWP_UINT64";
        case 20: return L"REG_UWP_SINGLE";
        case 21: return L"REG_UWP_DOUBLE";
        default: {
            wchar_t buffer[32];
            swprintf_s(buffer, L"type %u", type);
            return buffer;
        }
    }
}

std::wstring decode_utf16_string(const uint8_t* bytes, size_t byteLength) {
    if (byteLength < 2) {
        return L"";
    }
    size_t count = byteLength / 2;
    std::wstring out;
    out.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        wchar_t ch = static_cast<wchar_t>(static_cast<uint16_t>(bytes[i * 2]) |
                                          (static_cast<uint16_t>(bytes[i * 2 + 1]) << 8));
        if (ch == L'\0') {
            if (i + 1 < count) {
                break;
            }
            out += L'\0';
            continue;
        }
        out += ch;
    }
    return out;
}

std::wstring value_preview(const RegistryValue& value) {
    if (value.data.empty()) {
        return value.type == 0 ? L"(value not set)" : L"(empty)";
    }
    switch (value.type) {
        case 1:
        case 2: {
            std::wstring text = decode_utf16_string(value.data.data(), value.data.size());
            size_t nul = text.find(L'\0');
            if (nul != std::wstring::npos) {
                text.erase(nul);
            }
            return text;
        }
        case 6: {
            std::wstring link = decode_utf16_string(value.data.data(), value.data.size());
            size_t nul = link.find(L'\0');
            if (nul != std::wstring::npos) {
                link.erase(nul);
            }
            return link;
        }
        case 7: {
            std::wstring all = decode_utf16_string(value.data.data(), value.data.size());
            std::wstring out;
            size_t cursor = 0;
            bool first = true;
            while (cursor < all.size()) {
                size_t nul = all.find(L'\0', cursor);
                std::wstring piece =
                    all.substr(cursor, nul == std::wstring::npos ? std::wstring::npos : nul - cursor);
                if (!piece.empty()) {
                    if (!first) {
                        out += L", ";
                    }
                    out += piece;
                    first = false;
                }
                if (nul == std::wstring::npos) {
                    break;
                }
                cursor = nul + 1;
            }
            return out;
        }
        case 4: {
            if (value.data.size() < 4) {
                return hex_bytes(value.data.data(), value.data.size());
            }
            uint32_t dword = static_cast<uint32_t>(value.data[0]) | (static_cast<uint32_t>(value.data[1]) << 8) |
                             (static_cast<uint32_t>(value.data[2]) << 16) |
                             (static_cast<uint32_t>(value.data[3]) << 24);
            wchar_t buffer[48];
            swprintf_s(buffer, L"0x%08X (%u)", dword, dword);
            return buffer;
        }
        case 5: {
            if (value.data.size() < 4) {
                return hex_bytes(value.data.data(), value.data.size());
            }
            uint32_t dword = (static_cast<uint32_t>(value.data[0]) << 24) |
                             (static_cast<uint32_t>(value.data[1]) << 16) |
                             (static_cast<uint32_t>(value.data[2]) << 8) | static_cast<uint32_t>(value.data[3]);
            wchar_t buffer[48];
            swprintf_s(buffer, L"0x%08X (%u)", dword, dword);
            return buffer;
        }
        case 11: {
            uint64_t qword = 0;
            for (int i = 0; i < 8 && i < static_cast<int>(value.data.size()); ++i) {
                qword |= static_cast<uint64_t>(value.data[i]) << (i * 8);
            }
            wchar_t buffer[64];
            swprintf_s(buffer, L"0x%016llX (%llu)", qword, qword);
            return buffer;
        }
        case 16: {
            if (value.data.size() < 8) {
                return hex_bytes(value.data.data(), value.data.size());
            }
            uint64_t ft = 0;
            for (int i = 0; i < 8; ++i) {
                ft |= static_cast<uint64_t>(value.data[i]) << (i * 8);
            }
            return format_filetime(ft);
        }
        default:
            return hex_bytes(value.data.data(), value.data.size());
    }
}

namespace {

constexpr uint32_t kFreeBit = 0x80000000u;
constexpr uint32_t kNoOffset = 0xFFFFFFFFu;
constexpr uint32_t kFileBase = 0x1000u;

constexpr int64_t cell_abs_size(int32_t cellSize) {
    return cellSize < 0 ? -static_cast<int64_t>(cellSize) : static_cast<int64_t>(cellSize);
}

bool in_range(const std::vector<uint8_t>& bytes, size_t offset, size_t length) {
    return offset <= bytes.size() && length <= bytes.size() - offset;
}

bool read_u16(const std::vector<uint8_t>& bytes, size_t offset, uint16_t& out) {
    if (!in_range(bytes, offset, 2)) {
        return false;
    }
    out = static_cast<uint16_t>(bytes[offset]) | (static_cast<uint16_t>(bytes[offset + 1]) << 8);
    return true;
}

bool read_u32(const std::vector<uint8_t>& bytes, size_t offset, uint32_t& out) {
    if (!in_range(bytes, offset, 4)) {
        return false;
    }
    out = static_cast<uint32_t>(bytes[offset]) | (static_cast<uint32_t>(bytes[offset + 1]) << 8) |
          (static_cast<uint32_t>(bytes[offset + 2]) << 16) | (static_cast<uint32_t>(bytes[offset + 3]) << 24);
    return true;
}

bool read_i32(const std::vector<uint8_t>& bytes, size_t offset, int32_t& out) {
    if (!in_range(bytes, offset, 4)) {
        return false;
    }
    out = static_cast<int32_t>(static_cast<uint32_t>(bytes[offset]) |
                               (static_cast<uint32_t>(bytes[offset + 1]) << 8) |
                               (static_cast<uint32_t>(bytes[offset + 2]) << 16) |
                               (static_cast<uint32_t>(bytes[offset + 3]) << 24));
    return true;
}

bool read_u64(const std::vector<uint8_t>& bytes, size_t offset, uint64_t& out) {
    if (!in_range(bytes, offset, 8)) {
        return false;
    }
    out = 0;
    for (int i = 0; i < 8; ++i) {
        out |= static_cast<uint64_t>(bytes[offset + i]) << (i * 8);
    }
    return true;
}

bool sig_at(const std::vector<uint8_t>& bytes, size_t offset, const char (&signature)[3]) {
    return offset + 2 <= bytes.size() && bytes[offset] == static_cast<uint8_t>(signature[0]) &&
           bytes[offset + 1] == static_cast<uint8_t>(signature[1]);
}

std::wstring read_name(const std::vector<uint8_t>& bytes, size_t cellAbsolute, uint32_t nameOffset,
                       uint32_t nameLength, bool compressedAscii) {
    if (nameLength == 0) {
        return L"";
    }
    size_t absolute = cellAbsolute + nameOffset;
    if (!in_range(bytes, absolute, compressedAscii ? nameLength : static_cast<size_t>(nameLength) * 2)) {
        return L"(damaged)";
    }
    if (compressedAscii) {
        std::wstring out;
        out.reserve(nameLength);
        for (uint32_t i = 0; i < nameLength; ++i) {
            out += static_cast<wchar_t>(bytes[absolute + i]);
        }
        return out;
    }
    return decode_utf16_string(bytes.data() + absolute, static_cast<size_t>(nameLength) * 2);
}

}

bool hive_open(RegistryHive& hive, const std::wstring& path) {
    hive = RegistryHive();
    hive.filePath = path;

    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        hive.error = L"cannot open file";
        return false;
    }
    LARGE_INTEGER size = {};
    if (!GetFileSizeEx(file, &size)) {
        CloseHandle(file);
        hive.error = L"cannot read file size";
        return false;
    }
    if (size.QuadPart <= 0 || size.QuadPart > 512ll * 1024 * 1024) {
        CloseHandle(file);
        hive.error = L"file is empty or larger than 512 MB";
        return false;
    }
    hive.bytes.resize(static_cast<size_t>(size.QuadPart));
    DWORD read = 0;
    BOOL readOk = ReadFile(file, hive.bytes.data(), static_cast<DWORD>(hive.bytes.size()), &read, nullptr);
    CloseHandle(file);
    if (!readOk || read != hive.bytes.size()) {
        hive.error = L"cannot read file";
        return false;
    }

    if (hive.bytes.size() < 4096 || memcmp(hive.bytes.data(), "regf", 4) != 0) {
        hive.error = L"not a registry hive (signature regf not found)";
        return false;
    }

    if (!read_u32(hive.bytes, 0x24, hive.rootOffset) || hive.rootOffset == kNoOffset) {
        hive.error = L"root cell offset is missing";
        return false;
    }
    read_u64(hive.bytes, 0x0C, hive.written);

    std::wstring rawName;
    for (int i = 0; i < 32; ++i) {
        uint16_t unit = 0;
        read_u16(hive.bytes, 0x30 + i * 2, unit);
        if (unit == 0) {
            break;
        }
        rawName += static_cast<wchar_t>(unit);
    }
    if (rawName.rfind(L"\\??\\", 0) == 0 || rawName.rfind(L"\\\\?\\", 0) == 0) {
        rawName.erase(0, 4);
    }
    if (rawName.empty()) {
        size_t slash = path.find_last_of(L"\\/");
        rawName = slash == std::wstring::npos ? path : path.substr(slash + 1);
    }
    hive.rootName = rawName;

    struct RawCell {
        uint32_t offset;
        int32_t size;
    };
    std::vector<RawCell> allocated;

    size_t position = kFileBase;
    while (position + 0x20 <= hive.bytes.size()) {
        if (memcmp(hive.bytes.data() + position, "hbin", 4) != 0) {
            break;
        }
        uint32_t hbinSize = 0;
        if (!read_u32(hive.bytes, position + 8, hbinSize) || hbinSize < 0x20 ||
            position + hbinSize > hive.bytes.size()) {
            break;
        }
        size_t cellPos = position + 0x20;
        size_t hbinEnd = position + hbinSize;
        while (cellPos + 8 <= hbinEnd) {
            int32_t cellSize = 0;
            if (!read_i32(hive.bytes, cellPos, cellSize)) {
                break;
            }
            uint32_t absSize = static_cast<uint32_t>(cell_abs_size(cellSize));
            if (absSize < 8 || absSize > (hbinEnd - cellPos)) {
                break;
            }
            if ((cellSize & kFreeBit) != 0) {
                allocated.push_back({static_cast<uint32_t>(cellPos - kFileBase), cellSize});
            }
            cellPos += absSize;
        }
        position += hbinSize;
    }
    if (allocated.empty()) {
        hive.error = L"no allocated cells found (file is empty or fully damaged)";
        return false;
    }

    std::unordered_map<uint32_t, size_t> vkCellToValue;

    for (size_t i = 0; i < allocated.size(); ++i) {
        const RawCell& cell = allocated[i];
        if (i % 20000 == 0) {
        }
        size_t absolute = static_cast<size_t>(cell.offset) + kFileBase;
        if (!in_range(hive.bytes, absolute, 6)) {
            continue;
        }
        if (sig_at(hive.bytes, absolute + 4, "nk")) {
            RegistryKey key;
            key.cellOffset = cell.offset;
            read_u64(hive.bytes, absolute + 0x08, key.lastWrite);
            read_u32(hive.bytes, absolute + 0x14, key.parentCell);
            read_u32(hive.bytes, absolute + 0x30, key.skOffset);
            read_u32(hive.bytes, absolute + 0x34, key.classOffset);
            uint16_t flags = 0;
            read_u16(hive.bytes, absolute + 0x06, flags);
            key.flags = flags;

            uint16_t nameLength = 0;
            read_u16(hive.bytes, absolute + 0x4C, nameLength);
            bool ascii = (key.flags & 0x0020) != 0;
            key.name = read_name(hive.bytes, absolute, 0x50, nameLength, ascii);
            hive.keys.push_back(std::move(key));
        } else if (sig_at(hive.bytes, absolute + 4, "vk")) {
            RegistryValue value;
            uint32_t nameLenRaw = 0;
            read_u32(hive.bytes, absolute + 0x08, nameLenRaw);
            read_u32(hive.bytes, absolute + 0x10, value.type);

            uint16_t nameLen = 0;
            read_u16(hive.bytes, absolute + 0x06, nameLen);
            uint16_t presentFlag = 0;
            read_u16(hive.bytes, absolute + 0x14, presentFlag);
            if (nameLen == 0) {
                value.name = L"(default)";
            } else {
                size_t nameAbsolute = absolute + 0x18;
                if (presentFlag > 0) {
                    std::wstring name;
                    size_t available = hive.bytes.size() - nameAbsolute;
                    for (uint16_t i = 0; i < nameLen && i < available; ++i) {
                        name += static_cast<wchar_t>(hive.bytes[nameAbsolute + i]);
                    }
                    value.name = name;
                } else {
                    size_t byteLen = std::min<size_t>(static_cast<size_t>(nameLen) * 2,
                                                      hive.bytes.size() - nameAbsolute);
                    byteLen -= byteLen % 2;
                    value.name = decode_utf16_string(hive.bytes.data() + nameAbsolute, byteLen);
                }
            }

            uint32_t dataLength = nameLenRaw;
            bool resident = (dataLength & kFreeBit) != 0;
            dataLength &= ~kFreeBit;
            if (resident) {
                uint32_t residentAt = static_cast<uint32_t>(absolute + 0x0C);
                size_t take = std::min<size_t>(dataLength, 4);
                take = std::min<size_t>(take, hive.bytes.size() - residentAt);
                value.data.assign(hive.bytes.begin() + residentAt, hive.bytes.begin() + residentAt + take);
                value.dataOffset = 0;
                value.dataLength = take;
            } else {
                uint32_t dataOffset = 0;
                read_u32(hive.bytes, absolute + 0x0C, dataOffset);
                if (dataOffset != kNoOffset) {
                    size_t dataCell = static_cast<size_t>(dataOffset) + kFileBase;
                    int32_t dbSizeRaw = 0;
                    if (in_range(hive.bytes, dataCell, 4)) {
                        read_i32(hive.bytes, dataCell, dbSizeRaw);
                    }
                    uint32_t dbSize = static_cast<uint32_t>(cell_abs_size(dbSizeRaw));
                    size_t dataStart = dataCell + 4;
                    size_t dataBytes = 0;
                    if (in_range(hive.bytes, dataStart, dbSize) && dataLength <= dbSize) {
                        dataBytes = dataLength;
                    } else if (in_range(hive.bytes, dataStart, dbSize)) {
                        dataBytes = dbSize;
                    }
                    if (dataBytes > 0) {
                        value.data.assign(hive.bytes.begin() + dataStart, hive.bytes.begin() + dataStart + dataBytes);
                        value.dataOffset = dataCell;
                        value.dataLength = dataBytes;
                    }
                }
            }
            value.preview = value_preview(value);
            vkCellToValue[cell.offset] = hive.values.size();
            hive.values.push_back(std::move(value));
        }
    }

    std::function<void(uint32_t, std::vector<uint32_t>&, int)> resolve_list =
        [&](uint32_t listOffset, std::vector<uint32_t>& out, int depth) {
        out.clear();
        if (depth > 32 || listOffset == kNoOffset) {
            return;
        }
        size_t absolute = static_cast<size_t>(listOffset) + kFileBase;
        int32_t size = 0;
        if (!read_i32(hive.bytes, absolute, size) || absolute + 8 > hive.bytes.size()) {
            return;
        }
        uint32_t absSize = static_cast<uint32_t>(cell_abs_size(size));
        if (absSize < 8) {
            return;
        }
        bool isRi = sig_at(hive.bytes, absolute + 4, "ri");
        uint16_t count = 0;
        read_u16(hive.bytes, absolute + 0x06, count);
        size_t stride = (sig_at(hive.bytes, absolute + 4, "lf") || sig_at(hive.bytes, absolute + 4, "lh")) ? 8 : 4;
        size_t maxEntries = (absolute + absSize <= hive.bytes.size()) ? (absSize - 8) / stride : 0;
        count = static_cast<uint16_t>(std::min<size_t>(count, maxEntries));
        for (uint16_t i = 0; i < count; ++i) {
            uint32_t cellOffset = 0;
            if (!read_u32(hive.bytes, absolute + 0x08 + i * stride, cellOffset)) {
                break;
            }
            if (cellOffset == kNoOffset) {
                continue;
            }
            size_t entryAbsolute = static_cast<size_t>(cellOffset) + kFileBase;
            if (!in_range(hive.bytes, entryAbsolute, 6)) {
                continue;
            }
            if (sig_at(hive.bytes, entryAbsolute + 4, "nk")) {
                out.push_back(cellOffset);
            } else if (sig_at(hive.bytes, entryAbsolute + 4, "lf") || sig_at(hive.bytes, entryAbsolute + 4, "lh") ||
                       sig_at(hive.bytes, entryAbsolute + 4, "li") || sig_at(hive.bytes, entryAbsolute + 4, "ri")) {
                std::vector<uint32_t> nested;
                resolve_list(cellOffset, nested, depth + 1);
                out.insert(out.end(), nested.begin(), nested.end());
            }
        }
    };

    for (size_t i = 0; i < hive.keys.size(); ++i) {
        if (i % 5000 == 0) {
        }
        RegistryKey& key = hive.keys[i];
        size_t absolute = static_cast<size_t>(key.cellOffset) + kFileBase;
        uint32_t subkeysOffset = kNoOffset;
        read_u32(hive.bytes, absolute + 0x20, subkeysOffset);
        resolve_list(subkeysOffset, key.subkeys, 0);
        uint32_t volatileOffset = kNoOffset;
        read_u32(hive.bytes, absolute + 0x24, volatileOffset);
        std::vector<uint32_t> volatileSubkeys;
        resolve_list(volatileOffset, volatileSubkeys, 0);
        key.subkeys.insert(key.subkeys.end(), volatileSubkeys.begin(), volatileSubkeys.end());

        uint32_t valueListCount = 0;
        read_u32(hive.bytes, absolute + 0x28, valueListCount);
        uint32_t valueListOffset = kNoOffset;
        read_u32(hive.bytes, absolute + 0x2C, valueListOffset);
        if (valueListOffset != kNoOffset && valueListCount > 0 && valueListCount < 0x8000) {
            size_t listAbsolute = static_cast<size_t>(valueListOffset) + kFileBase;
            if (in_range(hive.bytes, listAbsolute, 4 + static_cast<size_t>(valueListCount) * 4)) {
                for (uint32_t i = 0; i < valueListCount; ++i) {
                    uint32_t vkOffset = 0;
                    read_u32(hive.bytes, listAbsolute + i * 4, vkOffset);
                    auto it = vkCellToValue.find(vkOffset);
                    if (it != vkCellToValue.end()) {
                        key.values.push_back(static_cast<uint32_t>(it->second));
                    }
                }
            }
        }
    }

    std::unordered_map<uint32_t, size_t> cellToIndex;
    for (size_t i = 0; i < hive.keys.size(); ++i) {
        cellToIndex[hive.keys[i].cellOffset] = i;
    }
    for (size_t i = 0; i < hive.keys.size(); ++i) {
        if (hive.keys[i].parentCell == kNoOffset) {
            hive.keys[i].path = hive.keys[i].name;
        } else {
            auto it = cellToIndex.find(hive.keys[i].parentCell);
            if (it != cellToIndex.end()) {
                const RegistryKey& parent = hive.keys[it->second];
                hive.keys[i].path = parent.path.empty() ? hive.keys[i].name
                                                        : parent.path + L"\\" + hive.keys[i].name;
            } else {
                hive.keys[i].path = hive.keys[i].name;
            }
        }
    }
    for (int pass = 0; pass < 4; ++pass) {
        for (size_t i = 0; i < hive.keys.size(); ++i) {
            if (hive.keys[i].parentCell != kNoOffset) {
                auto it = cellToIndex.find(hive.keys[i].parentCell);
                if (it != cellToIndex.end()) {
                    const RegistryKey& parent = hive.keys[it->second];
                    if (!parent.path.empty() &&
                        (hive.keys[i].path.empty() || hive.keys[i].path.size() < parent.path.size() + 1)) {
                        hive.keys[i].path = parent.path + L"\\" + hive.keys[i].name;
                    }
                }
            }
        }
    }

    hive.deletedKeyIndices.clear();
    {
        std::unordered_map<uint32_t, size_t> cellToIndex;
        for (size_t i = 0; i < hive.keys.size(); ++i) {
            cellToIndex[hive.keys[i].cellOffset] = i;
        }
        std::vector<char> reachable(hive.keys.size(), 0);
        std::vector<size_t> stack;
        for (size_t i = 0; i < hive.keys.size(); ++i) {
            if (hive.keys[i].cellOffset == hive.rootOffset) {
                stack.push_back(i);
                reachable[i] = 1;
                break;
            }
        }
        while (!stack.empty()) {
            size_t index = stack.back();
            stack.pop_back();
            for (uint32_t cell : hive.keys[index].subkeys) {
                auto it = cellToIndex.find(cell);
                if (it != cellToIndex.end() && !reachable[it->second]) {
                    reachable[it->second] = 1;
                    stack.push_back(it->second);
                }
            }
        }
        for (size_t i = 0; i < hive.keys.size(); ++i) {
            if (!reachable[i]) {
                hive.deletedKeyIndices.push_back(i);
            }
        }
    }

    hive.ok = true;
    return true;
}

bool hive_dump(const RegistryHive& hive, std::wstring& out) {
    if (!hive.ok) {
        out = L"error: " + hive.error;
        return false;
    }
    out.clear();
    wchar_t buffer[512];
    swprintf_s(buffer, L"hive=%s keys=%zu values=%zu root=%s\n", hive.filePath.c_str(), hive.keys.size(),
               hive.values.size(), hive.rootName.c_str());
    out += buffer;
    for (const RegistryKey& key : hive.keys) {
        swprintf_s(buffer, L"KEY %s  (offset=0x%08X, subkeys=%zu, values=%zu, lastwrite=%s)\n", key.path.c_str(),
                   key.cellOffset, key.subkeys.size(), key.values.size(), format_filetime(key.lastWrite).c_str());
        out += buffer;
        for (uint32_t valueIndex : key.values) {
            const RegistryValue& value = hive.values[valueIndex];
            swprintf_s(buffer, L"    %s  [%s]  %s\n", value.name.c_str(), value_type_name(value.type).c_str(),
                       value.preview.c_str());
            out += buffer;
        }
    }
    if (!out.empty()) {
        std::wcout << out;
        std::wcout.flush();
    }
    return true;
}

}
