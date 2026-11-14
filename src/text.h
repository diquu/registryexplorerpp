#pragma once

#include <string>

namespace repp {

std::string to_utf8(const std::wstring& value);
std::wstring from_utf8(const std::string& value);

}
