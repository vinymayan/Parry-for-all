#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include "ClibUtil/editorID.hpp"
#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/filewritestream.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"
#include <thread>
#include <chrono>

namespace FormUtil {
    const RE::TESFile* GetMasterFile(RE::TESForm* ref);
    std::string NormalizeFormID(RE::TESForm* form);
    RE::FormID FormIDFromString(const std::string& str);
}

struct InternalFormInfo {
    RE::FormID formID;
    std::string editorID;
    std::string name;
    std::string pluginName;
    std::string formType;
    std::string cachedDisplayName;

    void UpdateDisplayName() {
        std::string base = !name.empty() ? name : (!editorID.empty() ? editorID : "Unknown");
        cachedDisplayName = std::format("{} [{:08X}]", base, formID);
    }
    // Helper for UI
    std::string GetDisplayName() const {
        if (!name.empty()) return name;
        if (!editorID.empty()) return editorID;
        return std::to_string(formID);
    }
};

class Manager {
public:
    static Manager* GetSingleton() {
        static Manager singleton;
        return &singleton;
    }

    void RegisterAffectedNPC(RE::FormID baseID, const std::string& nifPath);
    void UnregisterAffectedNPC(RE::FormID baseID);
    bool IsNPCAffected(RE::FormID baseID);

    void PopulateAllLists();
    void RefreshLists(std::string_view a_signatures);
    static std::string ToUTF8(std::string_view a_str);
    // Data Store: Map of "TypeName" -> List of InternalFormInfo
    // We use this to feed the UI
    const std::vector<InternalFormInfo>& GetList(const std::string& typeName);

    // Register callback for when population is done
    void RegisterReadyCallback(std::function<void()> callback);

    bool _isPopulated = false;

private:
    Manager() = default;

    template <typename T>
    void PopulateList(const std::string& a_typeName, std::function<bool(T*)> a_filter = nullptr);

    std::map<std::string, std::vector<InternalFormInfo>> _dataStore;
    std::vector<std::function<void()>> _readyCallbacks;
    std::map<RE::FormID, std::string> _affectedNPCs;
};

