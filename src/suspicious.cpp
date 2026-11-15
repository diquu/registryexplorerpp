#include "suspicious.h"
#include "text.h"

#include <algorithm>
#include <cctype>

namespace repp {
namespace {

bool ends_with_ci(const std::wstring& value, const std::wstring& suffix) {
    if (value.size() < suffix.size()) {
        return false;
    }
    for (size_t i = 0; i < suffix.size(); ++i) {
        if (towlower(value[value.size() - suffix.size() + i]) != towlower(suffix[i])) {
            return false;
        }
    }
    return true;
}

bool contains_ci(const std::wstring& haystack, const std::wstring& needle) {
    if (needle.empty()) {
        return true;
    }
    std::wstring h = lower(haystack);
    std::wstring n = lower(needle);
    return h.find(n) != std::wstring::npos;
}

std::wstring value_to_wstring(const RegistryValue& value) {
    return value.preview;
}

uint32_t dword_of(const RegistryValue& value) {
    if (value.type != 4 || value.data.size() < 4) {
        return 0xFFFFFFFFu;
    }
    return static_cast<uint32_t>(value.data[0]) | static_cast<uint32_t>(value.data[1]) << 8 |
           static_cast<uint32_t>(value.data[2]) << 16 | static_cast<uint32_t>(value.data[3]) << 24;
}

std::vector<const RegistryValue*> values_of(const RegistryHive& hive, const RegistryKey& key) {
    std::vector<const RegistryValue*> out;
    for (uint32_t index : key.values) {
        if (index < hive.values.size()) {
            out.push_back(&hive.values[index]);
        }
    }
    return out;
}

std::vector<const RegistryValue*> values_named(const RegistryHive& hive, const RegistryKey& key, const std::wstring& name) {
    std::vector<const RegistryValue*> out;
    for (const RegistryValue* value : values_of(hive, key)) {
        if (lower(value->name) == lower(name)) {
            out.push_back(value);
        }
    }
    return out;
}

const RegistryKey* find_key(const RegistryHive& hive, const std::wstring& pathSuffix) {
    for (const RegistryKey& key : hive.keys) {
        if (ends_with_ci(key.path, pathSuffix)) {
            return &key;
        }
    }
    return nullptr;
}

const RegistryKey* find_key_by_index_of_suffix(const RegistryHive& hive, const std::wstring& pathSuffix) {
    return find_key(hive, pathSuffix);
}

std::wstring gather_value_strings(const std::vector<const RegistryValue*>& values) {
    std::wstring out;
    for (const RegistryValue* value : values) {
        if (!out.empty()) {
            out += L", ";
        }
        if (!value->name.empty() && value->name != L"(default)") {
            out += value->name + L"=";
        }
        out += value_to_wstring(*value);
    }
    return out;
}

}

const wchar_t* severity_label(Severity severity) {
    switch (severity) {
        case Severity::Clean: return L"CLEAN";
        case Severity::Review: return L"REVIEW";
        case Severity::Suspicious: return L"SUSPICIOUS";
        case Severity::NotApplicable: return L"N/A";
    }
    return L"?";
}

std::vector<Finding> scan_suspicious(const RegistryHive& hive) {
    std::vector<Finding> out;

    // Rule 1 — Browser URLBlocklist (Lazo: blocks GitHub, NirSoft, etc. during SS)
    {
        Finding finding;
        finding.rule = L"Browser URLBlocklist";
        finding.severity = Severity::Clean;
        finding.status = L"CLEAN";
        finding.evidence = L"No URLBlocklist keys found.";
        std::wstring evidence;
        int hits = 0;
        int keyIndex = -1;
        std::vector<std::wstring> flagged;
        for (size_t i = 0; i < hive.keys.size(); ++i) {
            if (ends_with_ci(hive.keys[i].path, L"\\URLBlocklist") &&
                contains_ci(hive.keys[i].path, L"Policies")) {
                auto values = values_of(hive, hive.keys[i]);
                if (!values.empty()) {
                    ++hits;
                    if (keyIndex < 0) {
                        keyIndex = static_cast<int>(i);
                    }
                    for (const RegistryValue* value : values) {
                        flagged.push_back(value->name);
                    }
                    evidence += hive.keys[i].path + L": " + gather_value_strings(values) + L"\n";
                }
            }
        }
        if (hits > 0) {
            finding.severity = Severity::Suspicious;
            finding.status = L"SUSPICIOUS";
            finding.evidence = L"Blocklists are present and may block pages needed for the review:\n" + evidence;
            finding.keyIndex = keyIndex;
            finding.flaggedValues = flagged;
        }
        out.push_back(finding);
    }

    // Rule 2 — SettingsPageVisibility (HKLM\...\Policies\Explorer, hide:/ShowOnly:)
    {
        Finding finding;
        finding.rule = L"SettingsPageVisibility";
        finding.severity = Severity::Clean;
        finding.status = L"CLEAN";
        finding.evidence = L"Not set (full Settings access).";
        const RegistryKey* key = find_key_by_index_of_suffix(hive, L"Windows\\CurrentVersion\\Policies\\Explorer");
        if (key != nullptr) {
            auto matches = values_named(hive, *key, L"SettingsPageVisibility");
            if (!matches.empty()) {
                std::wstring content = gather_value_strings(matches);
                finding.severity = Severity::Suspicious;
                finding.status = L"SUSPICIOUS";
                finding.evidence = L"Settings pages are restricted: " + content;
                finding.keyIndex = static_cast<int>(key - hive.keys.data());
                finding.flaggedValues.push_back(L"SettingsPageVisibility");
            }
        }
        out.push_back(finding);
    }

    // Rule 3 — OpenSavePidlMRU disabled (HKCU ComDlg32, NotFileMru = DWORD 1)
    {
        Finding finding;
        finding.rule = L"OpenSavePidlMRU disabled";
        finding.severity = Severity::Clean;
        finding.status = L"CLEAN";
        finding.evidence = L"NotFileMru not set (Open/Save history collection enabled).";
        const RegistryKey* key = find_key_by_index_of_suffix(hive, L"Explorer\\ComDlg32");
        if (key != nullptr) {
            auto matches = values_named(hive, *key, L"NotFileMru");
            for (const RegistryValue* value : matches) {
                if (value->type == 4 && dword_of(*value) == 1) {
                    finding.severity = Severity::Suspicious;
                    finding.status = L"SUSPICIOUS";
                    finding.evidence = L"ComDlg32\\NotFileMru = 1 (DWORD): file open/save history collection is disabled.";
                    finding.keyIndex = static_cast<int>(key - hive.keys.data());
                    finding.flaggedValues.push_back(L"NotFileMru");
                }
            }
        }
        out.push_back(finding);
    }

    // Rule 4 — CMD AutoRun with taskkill (HKLM Command Processor)
    {
        Finding finding;
        finding.rule = L"CMD AutoRun taskkill";
        finding.severity = Severity::Clean;
        finding.status = L"CLEAN";
        finding.evidence = L"AutoRun not set.";
        const RegistryKey* key = find_key_by_index_of_suffix(hive, L"Microsoft\\Command Processor");
        if (key != nullptr) {
            auto matches = values_named(hive, *key, L"AutoRun");
            if (!matches.empty()) {
                std::wstring content = gather_value_strings(matches);
                if (contains_ci(content, L"taskkill")) {
                    finding.severity = Severity::Suspicious;
                    finding.status = L"SUSPICIOUS";
                    finding.evidence = L"AutoRun closes CMD with taskkill: " + content;
                } else {
                    finding.severity = Severity::Review;
                    finding.status = L"REVIEW";
                    finding.evidence = L"AutoRun set but no taskkill found: " + content;
                }
                finding.keyIndex = static_cast<int>(key - hive.keys.data());
            }
        }
        out.push_back(finding);
    }

    // Rule 5 — DisallowRun (HKCU Policies\Explorer\DisallowRun exists)
    {
        Finding finding;
        finding.rule = L"DisallowRun blocklist";
        finding.severity = Severity::Clean;
        finding.status = L"CLEAN";
        finding.evidence = L"DisallowRun key not present.";
        const RegistryKey* key = find_key_by_index_of_suffix(hive, L"Policies\\Explorer\\DisallowRun");
        if (key != nullptr) {
            auto values = values_of(hive, *key);
            if (!values.empty()) {
                finding.severity = Severity::Suspicious;
                finding.status = L"SUSPICIOUS";
                finding.evidence = L"Execution restrictions are in place; blocked programs: " +
                                   gather_value_strings(values);
                finding.keyIndex = static_cast<int>(key - hive.keys.data());
            } else {
                finding.severity = Severity::Review;
                finding.status = L"REVIEW";
                finding.evidence = L"DisallowRun key exists but is empty.";
                finding.keyIndex = static_cast<int>(key - hive.keys.data());
            }
        }
        out.push_back(finding);
    }

    // Rule 6 — EnablePrefetcher (HKLM SYSTEM ...\PrefetchParameters, != 4 → suspicious)
    {
        Finding finding;
        finding.rule = L"EnablePrefetcher modified";
        finding.severity = Severity::NotApplicable;
        finding.status = L"N/A";
        finding.evidence = L"Not in this hive (SYSTEM hive only).";
        const RegistryKey* key = find_key_by_index_of_suffix(hive, L"Memory Management\\PrefetchParameters");
        if (key != nullptr) {
            auto matches = values_named(hive, *key, L"EnablePrefetcher");
            if (!matches.empty()) {
                uint32_t value = dword_of(*matches.front());
                wchar_t buffer[64];
                swprintf_s(buffer, L"0x%08X (%u)", value, value);
                finding.evidence = L"EnablePrefetcher = " + std::wstring(buffer);
                if (value != 4) {
                    finding.severity = Severity::Suspicious;
                    finding.status = L"SUSPICIOUS";
                    finding.evidence += L" (expected 4)";
                } else {
                    finding.severity = Severity::Clean;
                    finding.status = L"CLEAN";
                }
                finding.keyIndex = static_cast<int>(key - hive.keys.data());
            } else {
                finding.severity = Severity::Review;
                finding.status = L"REVIEW";
                finding.evidence = L"PrefetchParameters exists but EnablePrefetcher value not found.";
                finding.keyIndex = static_cast<int>(key - hive.keys.data());
            }
        }
        out.push_back(finding);
    }

    // Rule 7 — IFEO Debugger (Image File Execution Options with a "Debugger" value = process is
    // redirected on launch; classic persistence/evasion during review)
    {
        Finding finding;
        finding.rule = L"IFEO Debugger";
        finding.severity = Severity::Clean;
        finding.status = L"CLEAN";
        finding.evidence = L"No \"Debugger\" values under Image File Execution Options.";
        std::wstring hits;
        int hitsCount = 0;
        int keyIndex = -1;
        std::vector<std::wstring> flagged;
        const std::wstring ifeo = L"Image File Execution Options";
        for (size_t i = 0; i < hive.keys.size(); ++i) {
            const std::wstring& path = hive.keys[i].path;
            size_t ifeoPos = lower(path).find(lower(ifeo));
            if (ifeoPos == std::wstring::npos) {
                continue;
            }
            std::wstring tail = lower(path.substr(ifeoPos));
            bool isIfeoRoot = tail == lower(ifeo);
            bool isIfeoChild = contains_ci(path.substr(ifeoPos), L"\\");
            if (!isIfeoRoot && !isIfeoChild) {
                continue;
            }
            auto matches = values_named(hive, hive.keys[i], L"Debugger");
            for (const RegistryValue* value : matches) {
                if (keyIndex < 0) {
                    keyIndex = static_cast<int>(i);
                }
                ++hitsCount;
                flagged.push_back(value->name);
                hits += L"  " + path + L": Debugger = " + value->preview + L"\n";
            }
        }
        if (hitsCount > 0) {
            finding.severity = Severity::Suspicious;
            finding.status = L"SUSPICIOUS";
            finding.evidence = std::to_wstring(hitsCount) +
                               L" \"Debugger\" value(s) under Image File Execution Options:\n" + hits;
            finding.keyIndex = keyIndex;
            finding.flaggedValues = flagged;
        }
        out.push_back(finding);
    }

    // Rule 8 — BAM deleted keys (SYSTEM)
    {
        Finding finding;
        finding.rule = L"BAM entries deleted";
        finding.severity = Severity::NotApplicable;
        finding.status = L"N/A";
        finding.evidence = L"Not in this hive or no BAM-like deleted cells found.";
        std::vector<std::wstring> deletedBam;
        for (size_t index : hive.deletedKeyIndices) {
            const std::wstring& path = hive.keys[index].path;
            if (contains_ci(path, L"\\bam") &&
                (contains_ci(path, L"services") || contains_ci(path, L"Services"))) {
                deletedBam.push_back(path);
            }
        }
        if (!deletedBam.empty()) {
            finding.severity = Severity::Suspicious;
            finding.status = L"SUSPICIOUS";
            std::wstring evidence = std::to_wstring(deletedBam.size()) + L" BAM key(s) deleted:\n";
            for (const std::wstring& path : deletedBam) {
                evidence += L"  " + path + L"\n";
            }
            finding.evidence = evidence;
            for (size_t index : hive.deletedKeyIndices) {
                if (contains_ci(hive.keys[index].path, L"\\bam")) {
                    finding.keyIndex = static_cast<int>(index);
                    break;
                }
            }
        }
        out.push_back(finding);
    }

    return out;
}

}
