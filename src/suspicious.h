#pragma once

#include "registry_hive.h"

#include <string>
#include <vector>

namespace repp {

enum class Severity {
    Clean,
    Review,
    Suspicious,
    NotApplicable,
};

struct Finding {
    Severity severity = Severity::NotApplicable;
    std::wstring rule;
    std::wstring status;
    std::wstring evidence;
    int keyIndex = -1;
};

std::vector<Finding> scan_suspicious(const RegistryHive& hive);
const wchar_t* severity_label(Severity severity);

}
