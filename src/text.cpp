#include "text.h"

#include <windows.h>

#include <cwchar>

namespace repp {

std::string to_utf8(const std::wstring& value) {
    if (value.empty()) {
        return "";
    }
    int needed = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string out;
    if (needed > 1) {
        out.resize(static_cast<size_t>(needed - 1));
        WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, out.data(), needed, nullptr, nullptr);
    }
    return out;
}

std::wstring from_utf8(const std::string& value) {
    if (value.empty()) {
        return L"";
    }
    int needed = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    std::wstring out;
    if (needed > 1) {
        out.resize(static_cast<size_t>(needed - 1));
        MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, out.data(), needed);
    }
    return out;
}

}
