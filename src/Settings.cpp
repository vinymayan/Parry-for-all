#include "Settings.h"
#include "Manager.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <system_error>
#include <unordered_set>

namespace ImGui = ImGuiMCP;

namespace ParrySettings {
    const std::string MOD_DIR = "Data/Viny Mods/Parry for all/";
    const std::string RULES_DIR = MOD_DIR + "Rules/";
    const std::string OLD_RULES_DIR = "Data/SKSE/Plugins/ParryAll/Rules/";
    const char* NEW_SETTINGS_PATH = "Data/Viny Mods/Parry for all/Settings.json";
    const char* LEGACY_SETTINGS_PATH = "Data/SKSE/Plugins/ParryAll/ParryAll_Settings.json";
    const char* OLD_SETTINGS_PATH = "Data/SKSE/Plugins/ParryAll.json";
    const char* LANG_PATH = "Data/Viny Mods/Parry for all/Language.json";
    static std::unordered_map<std::string, std::string> LangMap;

    void LoadLanguage() {
        LangMap.clear();
        std::ifstream file(LANG_PATH, std::ios::binary);
        if (!file.is_open()) {
            SKSE::log::warn("Could not load Parry for all Language.json. Using default texts.");
            return;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string jsonStr = buffer.str();
        file.close();

        if (jsonStr.size() >= 3 && (unsigned char)jsonStr[0] == 0xEF && (unsigned char)jsonStr[1] == 0xBB && (unsigned char)jsonStr[2] == 0xBF) {
            jsonStr.erase(0, 3);
        }

        rapidjson::Document doc;
        doc.Parse(jsonStr.c_str());

        if (doc.HasParseError()) return;

        if (doc.IsObject()) {
            for (auto itr = doc.MemberBegin(); itr != doc.MemberEnd(); ++itr) {
                if (itr->value.IsObject()) {
                    std::string category = itr->name.GetString();
                    for (auto jtr = itr->value.MemberBegin(); jtr != itr->value.MemberEnd(); ++jtr) {
                        if (jtr->value.IsString()) {
                            LangMap[category + "." + jtr->name.GetString()] = jtr->value.GetString();
                        }
                    }
                }
                else if (itr->value.IsString()) {
                    LangMap[itr->name.GetString()] = itr->value.GetString();
                }
            }
        }
    }

    const char* GetLoc(const std::string& key, const char* defaultVal) {
        auto it = LangMap.find(key);
        if (it != LangMap.end()) return it->second.c_str();
        return defaultVal;
    }

    inline std::string ToLower(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
        return s;
    }

    std::string SanitizeRuleFileName(const std::string& ruleName) {
        std::string safeName;
        safeName.reserve(ruleName.size());

        for (char ch : ruleName) {
            const auto c = static_cast<unsigned char>(ch);
            if (std::isalnum(c) || ch == ' ' || ch == '_' || ch == '-') {
                safeName.push_back(ch);
            }
            else {
                safeName.push_back('_');
            }
        }

        while (!safeName.empty() && (safeName.back() == ' ' || safeName.back() == '.')) {
            safeName.pop_back();
        }

        if (safeName.empty() || safeName == "." || safeName == "..") {
            safeName = "New Rule";
        }

        return safeName;
    }

    std::filesystem::path GetRulePath(const std::string& directory, const std::string& ruleName) {
        return std::filesystem::path(directory) / (SanitizeRuleFileName(ruleName) + ".json");
    }

    bool IsRuleFileNameInUse(const std::string& ruleName, const NPCParryRule* ignoredRule = nullptr) {
        const auto candidate = ToLower(SanitizeRuleFileName(ruleName));
        return std::any_of(npcRules.begin(), npcRules.end(), [&](const NPCParryRule& existingRule) {
            return &existingRule != ignoredRule && ToLower(SanitizeRuleFileName(existingRule.ruleName)) == candidate;
        });
    }

    inline int GetIndexFromID(int id, const int* idArray, int arraySize) {
        for (int i = 0; i < arraySize; i++) {
            if (idArray[i] == id) return i;
        }
        return 0;
    }

    // Dropdowns UI
    const char* ModeOptions = "Disabled\0Enabled (Free)\0Enabled (Cost)\0\0";
    const char* ReflectOptions = "Disable\0Enable\0Enable only in perfect\0\0";
    const char* CostOptionsUI = "None\0Stamina\0Magicka\0Health\0\0";

    bool DrawReflectTypeCombo(const char* label, int* currentType) {
        bool changed = false;
        const char* reflectTypeNames[] = {
            GetLoc("menu.reflect_flat", "Flat Damage"),
            GetLoc("menu.reflect_percent", "Percentage Damage"),
            GetLoc("menu.reflect_full", "Full Damage"),
            GetLoc("menu.reflect_scaling", "Skill Scaling")
        };
        const char* reflectTypeTooltips[] = {
            GetLoc("menu.tooltip_flat", "Reflects a fixed static amount of damage regardless of the incoming attack power."),
            GetLoc("menu.tooltip_percent", "Reflects a specific percentage of the incoming attack damage."),
            GetLoc("menu.tooltip_full", "Reflects 100% of the incoming attack damage directly back to the attacker."),
            GetLoc("menu.tooltip_scaling", "Calculates reflected damage dynamically based on the defender's Block skill vs the attacker's weapon skill.")
        };

        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::BeginCombo(label, reflectTypeNames[*currentType])) {
            for (int n = 0; n < 4; n++) {
                bool is_selected = (*currentType == n);
                if (ImGui::Selectable(reflectTypeNames[n], is_selected)) {
                    *currentType = n;
                    changed = true;
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::TextUnformatted(reflectTypeTooltips[n]);
                    ImGui::EndTooltip();
                }
            }
            ImGui::EndCombo();
        }
        return changed;
    }

    bool DrawDropdown(const char* label, const std::string& category, RE::FormID& current_form_id, float customWidth = -1.0f) {
        bool changed = false;
        const auto& fullList = Manager::GetSingleton()->GetList(category);
        if (fullList.empty()) return false;

        std::vector<const char*> comboItems;
        std::vector<int> mapToFull;

        comboItems.push_back(GetLoc("menu.opt_none", "None"));
        mapToFull.push_back(-1);

        int localSelection = 0;
        for (size_t i = 0; i < fullList.size(); ++i) {
            comboItems.push_back(fullList[i].cachedDisplayName.c_str());
            mapToFull.push_back(static_cast<int>(i));
            if (fullList[i].formID == current_form_id) {
                localSelection = static_cast<int>(i) + 1;
            }
        }

        ImGui::PushID(label);
        std::string displayLabel = label;
        size_t hashPos = displayLabel.find("##");
        if (hashPos != std::string::npos) displayLabel = displayLabel.substr(0, hashPos);

        ImGui::Text("%s:", displayLabel.c_str());
        ImGui::SameLine();

        if (customWidth > 0.0f) ImGui::SetNextItemWidth(customWidth);
        const char* previewValue = comboItems[localSelection];

        if (ImGui::BeginCombo("##drop", previewValue)) {
            static std::map<std::string, std::string> searchBuffers;
            char searchBuf[256] = "";
            if (searchBuffers.contains(label)) strcpy_s(searchBuf, searchBuffers[label].c_str());

            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::InputText("##busca", searchBuf, sizeof(searchBuf))) {
                searchBuffers[label] = searchBuf;
            }
            ImGui::Separator();

            std::string searchStr = searchBuf;
            std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(), [](unsigned char c) { return std::tolower(c); });

            ImGui::BeginChild("##scroll", ImGuiMCP::ImVec2(0, 200), false);
            for (int i = 0; i < comboItems.size(); i++) {
                std::string itemLower = comboItems[i];
                std::transform(itemLower.begin(), itemLower.end(), itemLower.begin(), [](unsigned char c) { return std::tolower(c); });

                if (searchStr.empty() || itemLower.find(searchStr) != std::string::npos) {
                    bool isSelected = (localSelection == i);
                    if (ImGui::Selectable(comboItems[i], isSelected)) {
                        localSelection = i;
                        int originalIndex = mapToFull[localSelection];

                        if (originalIndex == -1) current_form_id = 0;
                        else current_form_id = fullList[originalIndex].formID;

                        searchBuffers[label] = "";
                        changed = true;
                    }
                    if (isSelected) ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndChild();
            ImGui::EndCombo();
        }
        ImGui::PopID();
        return changed;
    }

    void AddFormIDMember(rapidjson::Document& doc, rapidjson::Document::AllocatorType& alloc, const char* key, RE::FormID formID) {
        auto form = RE::TESForm::LookupByID(formID);
        if (form) {
            const char* editorID = form->GetFormEditorID();
            if (editorID && editorID[0] != '\0') {
                std::string editorKey = std::string(key) + "EditorID";
                rapidjson::Value jsonKey;
                jsonKey.SetString(editorKey.c_str(), static_cast<rapidjson::SizeType>(editorKey.size()), alloc);
                rapidjson::Value jsonValue;
                jsonValue.SetString(editorID, alloc);
                doc.AddMember(jsonKey, jsonValue, alloc);
            }
        }

        std::string formStr = FormUtil::NormalizeFormID(form);
        rapidjson::Value value;
        value.SetString(formStr.c_str(), alloc);
        rapidjson::Value formKey;
        formKey.SetString(key, alloc);
        doc.AddMember(formKey, value, alloc);
    }

    void ReadFormIDMember(const rapidjson::Document& doc, const char* key, RE::FormID& formID) {
        std::string editorKey = std::string(key) + "EditorID";
        if (doc.HasMember(editorKey.c_str()) && doc[editorKey.c_str()].IsString()) {
            if (auto form = RE::TESForm::LookupByEditorID(doc[editorKey.c_str()].GetString())) {
                formID = form->GetFormID();
                return;
            }
        }

        if (!doc.HasMember(key)) return;

        const auto& value = doc[key];
        if (value.IsString()) {
            formID = FormUtil::FormIDFromString(value.GetString());
        }
        else if (value.IsUint()) {
            formID = value.GetUint();
        }
    }

    void AddRuleFormIDMember(rapidjson::Document& doc, rapidjson::Document::AllocatorType& alloc, const char* key, RE::FormID formID) {
        AddFormIDMember(doc, alloc, key, formID);
    }

    void ReadRuleFormIDMember(const rapidjson::Document& doc, const char* key, RE::FormID& formID) {
        ReadFormIDMember(doc, key, formID);
    }

    float ResolveGlobalFloat(RE::FormID a_globalID, float a_fallback) {
        if (auto global = RE::TESForm::LookupByID<RE::TESGlobal>(a_globalID)) {
            return global->value;
        }
        return a_fallback;
    }

    int ResolveParryTimeMS(int a_fallback, RE::FormID a_globalID) {
        return std::max(1, static_cast<int>(ResolveGlobalFloat(a_globalID, static_cast<float>(a_fallback))));
    }

    ValueSource ReadSource(const rapidjson::Document& doc, const char* key, ValueSource fallback) {
        if (!doc.HasMember(key) || !doc[key].IsInt()) return fallback;
        int value = std::clamp(doc[key].GetInt(), 0, 2);
        return static_cast<ValueSource>(value);
    }

    void AddSourceMember(rapidjson::Document& doc, rapidjson::Document::AllocatorType& alloc, const char* key, ValueSource source) {
        rapidjson::Value jsonKey;
        jsonKey.SetString(key, alloc);
        doc.AddMember(jsonKey, static_cast<int>(source), alloc);
    }

    void AddStringMember(rapidjson::Document& doc, rapidjson::Document::AllocatorType& alloc, const char* key, const std::string& text) {
        rapidjson::Value jsonKey;
        jsonKey.SetString(key, alloc);
        rapidjson::Value value;
        value.SetString(text.c_str(), alloc);
        doc.AddMember(jsonKey, value, alloc);
    }

    void ReadActorValueMember(const rapidjson::Document& doc, const char* key, std::string& actorValue);

    void AddRestoreConfig(rapidjson::Document& doc, rapidjson::Document::AllocatorType& alloc, const char* prefix, const ParryRestoreConfig& config) {
        auto makeKey = [prefix](const char* suffix) {
            return std::string(prefix) + suffix;
        };

        auto resourceKey = makeKey("Resource");
        rapidjson::Value resourceJsonKey;
        resourceJsonKey.SetString(resourceKey.c_str(), alloc);
        doc.AddMember(resourceJsonKey, static_cast<int>(config.resource), alloc);

        auto modeKey = makeKey("Mode");
        rapidjson::Value modeJsonKey;
        modeJsonKey.SetString(modeKey.c_str(), alloc);
        doc.AddMember(modeJsonKey, static_cast<int>(config.mode), alloc);

        auto valueKey = makeKey("Value");
        rapidjson::Value valueJsonKey;
        valueJsonKey.SetString(valueKey.c_str(), alloc);
        doc.AddMember(valueJsonKey, config.value, alloc);

        auto globalKey = makeKey("ValueGlobal");
        AddFormIDMember(doc, alloc, globalKey.c_str(), config.valueGlobal);

        auto sourceKey = makeKey("ValueSource");
        AddSourceMember(doc, alloc, sourceKey.c_str(), config.valueSource);

        auto actorValueKey = makeKey("ValueActorValue");
        AddStringMember(doc, alloc, actorValueKey.c_str(), config.valueActorValue);

        auto perkKey = makeKey("Perk");
        AddFormIDMember(doc, alloc, perkKey.c_str(), config.perk);
    }

    void ReadRestoreConfig(const rapidjson::Document& doc, const char* prefix, ParryRestoreConfig& config) {
        auto makeKey = [prefix](const char* suffix) {
            return std::string(prefix) + suffix;
        };

        auto resourceKey = makeKey("Resource");
        if (doc.HasMember(resourceKey.c_str()) && doc[resourceKey.c_str()].IsInt()) {
            config.resource = static_cast<CostType>(std::clamp(doc[resourceKey.c_str()].GetInt(), 0, 3));
        }

        auto modeKey = makeKey("Mode");
        if (doc.HasMember(modeKey.c_str()) && doc[modeKey.c_str()].IsInt()) {
            config.mode = static_cast<RestoreMode>(std::clamp(doc[modeKey.c_str()].GetInt(), 0, 1));
        }

        auto valueKey = makeKey("Value");
        if (doc.HasMember(valueKey.c_str()) && doc[valueKey.c_str()].IsNumber()) {
            config.value = doc[valueKey.c_str()].GetFloat();
        }

        auto globalKey = makeKey("ValueGlobal");
        ReadFormIDMember(doc, globalKey.c_str(), config.valueGlobal);

        auto sourceKey = makeKey("ValueSource");
        config.valueSource = ReadSource(doc, sourceKey.c_str(), config.valueGlobal != 0 ? ValueSource::Global : config.valueSource);

        auto actorValueKey = makeKey("ValueActorValue");
        ReadActorValueMember(doc, actorValueKey.c_str(), config.valueActorValue);

        auto perkKey = makeKey("Perk");
        ReadFormIDMember(doc, perkKey.c_str(), config.perk);
    }

    std::string ActorValueNameFromLegacyInt(int actorValue) {
        auto name = RE::ActorValueList::GetActorValueName(static_cast<RE::ActorValue>(actorValue));
        return name && name[0] != '\0' ? std::string(name) : "Block";
    }

    void ReadActorValueMember(const rapidjson::Document& doc, const char* key, std::string& actorValue) {
        if (!doc.HasMember(key)) return;

        const auto& value = doc[key];
        if (value.IsString()) {
            actorValue = value.GetString();
        }
        else if (value.IsInt()) {
            actorValue = ActorValueNameFromLegacyInt(value.GetInt());
        }
    }

    float ResolveConfigValue(float a_flatValue, ValueSource a_source, RE::FormID a_globalID, const std::string& a_actorValue, RE::Actor* a_actor) {
        switch (a_source) {
        case ValueSource::Global:
            return ResolveGlobalFloat(a_globalID, a_flatValue);
        case ValueSource::ActorValue:
            if (a_actor && a_actor->AsActorValueOwner()) {
                auto actorValue = RE::ActorValueList::LookupActorValueByName(a_actorValue.c_str());
                if (actorValue != RE::ActorValue::kNone) {
                    return a_actor->AsActorValueOwner()->GetActorValue(actorValue);
                }
            }
            return a_flatValue;
        case ValueSource::Flat:
        default:
            return a_flatValue;
        }
    }

    int ResolveConfigInt(int a_flatValue, ValueSource a_source, RE::FormID a_globalID, const std::string& a_actorValue, RE::Actor* a_actor, int a_minValue) {
        return std::max(a_minValue, static_cast<int>(ResolveConfigValue(static_cast<float>(a_flatValue), a_source, a_globalID, a_actorValue, a_actor)));
    }

    bool HasRequiredPerk(RE::Actor* a_actor, RE::FormID a_perkID) {
        if (a_perkID == 0) return true;
        if (!a_actor) return false;

        auto perk = RE::TESForm::LookupByID<RE::BGSPerk>(a_perkID);
        return perk && a_actor->HasPerk(perk);
    }

    bool CanActorUseParry(RE::Actor* a_actor) {
        if (!a_actor) return false;

        if (a_actor->IsPlayerRef()) {
            if (playerDisableParryPerk != 0 && HasRequiredPerk(a_actor, playerDisableParryPerk)) {
                return false;
            }
            return playerParryEnabled && HasRequiredPerk(a_actor, playerParryPerk);
        }

        if (!npcParryEnabled) return false;
        if (npcDisableParryPerk != 0 && HasRequiredPerk(a_actor, npcDisableParryPerk)) {
            return false;
        }

        for (const auto& rule : npcRules) {
            if (rule.perkID != 0 && HasRequiredPerk(a_actor, rule.perkID)) {
                return rule.npcParryEnabled;
            }
        }

        return HasRequiredPerk(a_actor, npcParryPerk);
    }

    void ApplyParryRestore(RE::Actor* a_actor, bool a_perfect) {
        if (!a_actor || !a_actor->AsActorValueOwner()) return;

        const ParryRestoreConfig* config = nullptr;
        if (a_actor->IsPlayerRef()) {
            config = a_perfect ? &playerPerfectRestore : &playerNormalRestore;
        }
        else {
            config = a_perfect ? &npcPerfectRestore : &npcNormalRestore;
            for (const auto& rule : npcRules) {
                if (rule.perkID != 0 && HasRequiredPerk(a_actor, rule.perkID)) {
                    config = a_perfect ? &rule.npcPerfectRestore : &rule.npcNormalRestore;
                    break;
                }
            }
        }

        if (!config || config->resource == CostType::None || !HasRequiredPerk(a_actor, config->perk)) {
            return;
        }

        RE::ActorValue actorValue = RE::ActorValue::kNone;
        switch (config->resource) {
        case CostType::Stamina:
            actorValue = RE::ActorValue::kStamina;
            break;
        case CostType::Magicka:
            actorValue = RE::ActorValue::kMagicka;
            break;
        case CostType::Health:
            actorValue = RE::ActorValue::kHealth;
            break;
        case CostType::None:
        default:
            return;
        }

        float amount = std::max(0.0f, ResolveConfigValue(
            config->value,
            config->valueSource,
            config->valueGlobal,
            config->valueActorValue,
            a_actor));

        if (config->mode == RestoreMode::Percentage) {
            amount = a_actor->GetActorValueMax(actorValue) * (amount / 100.0f);
        }

        if (amount > 0.0f) {
            a_actor->AsActorValueOwner()->RestoreActorValue(actorValue, amount);
        }
    }

    void GetParryWindowsForActor(RE::Actor* a_actor, int& a_normalMS, int& a_perfectMS) {
        if (!a_actor) {
            a_normalMS = ResolveParryTimeMS(400, 0);
            a_perfectMS = ResolveParryTimeMS(100, 0);
            return;
        }

        RE::FormID normalGlobal = 0;
        RE::FormID perfectGlobal = 0;
        ValueSource normalSource = ValueSource::Flat;
        ValueSource perfectSource = ValueSource::Flat;
        std::string normalActorValue = "Block";
        std::string perfectActorValue = "Block";

        if (a_actor->IsPlayerRef()) {
            a_normalMS = playerNormalParryMS;
            a_perfectMS = playerPerfectParryMS;
            normalGlobal = playerNormalParryGlobal;
            perfectGlobal = playerPerfectParryGlobal;
            normalSource = playerNormalParrySource;
            perfectSource = playerPerfectParrySource;
            normalActorValue = playerNormalParryActorValue;
            perfectActorValue = playerPerfectParryActorValue;
        }
        else {
            a_normalMS = npcNormalParryMS;
            a_perfectMS = npcPerfectParryMS;
            normalGlobal = npcNormalParryGlobal;
            perfectGlobal = npcPerfectParryGlobal;
            normalSource = npcNormalParrySource;
            perfectSource = npcPerfectParrySource;
            normalActorValue = npcNormalParryActorValue;
            perfectActorValue = npcPerfectParryActorValue;

            for (const auto& rule : npcRules) {
                if (rule.perkID != 0 && HasRequiredPerk(a_actor, rule.perkID)) {
                    a_normalMS = rule.npcNormalParryMS;
                    a_perfectMS = rule.npcPerfectParryMS;
                    normalGlobal = rule.npcNormalParryGlobal;
                    perfectGlobal = rule.npcPerfectParryGlobal;
                    normalSource = rule.npcNormalParrySource;
                    perfectSource = rule.npcPerfectParrySource;
                    normalActorValue = rule.npcNormalParryActorValue;
                    perfectActorValue = rule.npcPerfectParryActorValue;
                    break;
                }
            }
        }

        a_normalMS = ResolveConfigInt(a_normalMS, normalSource, normalGlobal, normalActorValue, a_actor, 1);
        a_perfectMS = ResolveConfigInt(a_perfectMS, perfectSource, perfectGlobal, perfectActorValue, a_actor, 1);
    }

    float ResolveParryCost(float a_localCost, RE::FormID a_costGlobal) {
        float resolved = ResolveGlobalFloat(a_costGlobal, a_localCost);
        return std::max(0.0f, resolved);
    }

    float ResolveParryCost(float a_localCost, ValueSource a_source, RE::FormID a_costGlobal, const std::string& a_actorValue, RE::Actor* a_actor) {
        float resolved = ResolveConfigValue(a_localCost, a_source, a_costGlobal, a_actorValue, a_actor);
        return std::max(0.0f, resolved);
    }

    float GetStaggerMagnitude(int a_size, bool a_perfect) {
        int effectiveSize = std::clamp(a_size + (a_perfect ? 1 : 0), 0, 3);
        constexpr float magnitudes[] = { 0.25f, 0.5f, 1.0f, 1.5f };
        return magnitudes[effectiveSize];
    }

    bool DrawStaggerSizeCombo(const char* label, int* size) {
        const char* sizeOpts[] = {
            GetLoc("menu.stagger_size_small", "Small"),
            GetLoc("menu.stagger_size_medium", "Medium"),
            GetLoc("menu.stagger_size_large", "Large"),
            GetLoc("menu.stagger_size_largest", "Largest")
        };
        return ImGui::Combo(label, size, sizeOpts, 4);
    }

    bool DrawReflectTriggers(const char* id, int& triggers) {
        bool changed = false;
        ImGui::TextUnformatted(GetLoc("menu.reflect_when", "Reflect When"));

        const auto drawTrigger = [&](const char* text, const char* suffix, int flag) {
            bool enabled = (triggers & flag) != 0;
            std::string label = std::string(text) + "##" + id + suffix;
            if (ImGui::Checkbox(label.c_str(), &enabled)) {
                triggers = enabled ? (triggers | flag) : (triggers & ~flag);
                changed = true;
            }
        };

        drawTrigger(GetLoc("menu.reflect_on_block", "Block"), "Block", 1);
        ImGui::SameLine();
        drawTrigger(GetLoc("menu.reflect_on_bash", "Bash"), "Bash", 2);
        ImGui::SameLine();
        drawTrigger(GetLoc("menu.reflect_on_power_bash", "Power Bash"), "PowerBash", 4);
        return changed;
    }

    bool DrawConditionalPerkDropdown(const char* label, RE::FormID& perk, bool enabled, const char* requirement) {
        ImGui::BeginDisabled(!enabled);
        const bool changed = DrawDropdown(label, "Perk", perk, 300.0f);
        ImGui::EndDisabled();

        if (!enabled) {
            ImGui::SameLine();
            ImGui::TextDisabled("%s: %s", GetLoc("menu.requires", "Requires"), requirement);
        }
        return changed;
    }

    bool RenderStringInput(const char* label, std::string& value, bool disabled = false) {
        bool changed = false;
        char buffer[128]{};
        strcpy_s(buffer, value.c_str());
        ImGui::BeginDisabled(disabled);
        ImGui::SetNextItemWidth(300.0f);
        if (ImGui::InputText(label, buffer, sizeof(buffer))) {
            value = buffer;
            changed = true;
        }
        ImGui::EndDisabled();
        return changed;
    }

    bool DrawValueSourceCombo(const char* label, ValueSource& source, RE::FormID globalID) {
        const char* sourceOpts[] = {
            GetLoc("menu.value_source_flat", "Flat"),
            GetLoc("menu.value_source_global", "Global"),
            GetLoc("menu.value_source_actor_value", "Actor Value")
        };

        bool changed = false;
        int sourceIndex = static_cast<int>(source);
        if (ImGui::Combo(label, &sourceIndex, sourceOpts, 3)) {
            source = static_cast<ValueSource>(std::clamp(sourceIndex, 0, 2));
            changed = true;
        }

        if (globalID != 0 && source == ValueSource::Flat) {
            source = ValueSource::Global;
            changed = true;
        }

        return changed;
    }

    bool RenderIntSliderWithInput(const char* label, int& value, int minValue, int maxValue, bool disabled = false, bool clampInputToSliderRange = true) {
        bool changed = false;
        ImGui::BeginDisabled(disabled);
        ImGui::PushID(label);
        ImGui::SetNextItemWidth(200.0f);
        int sliderValue = std::clamp(value, minValue, maxValue);
        const std::string sliderDisplayValue = std::to_string(value);
        if (ImGui::SliderInt("##slider", &sliderValue, minValue, maxValue, sliderDisplayValue.c_str())) {
            value = sliderValue;
            changed = true;
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::InputInt(label, &value, 0, 0)) changed = true;
        if (clampInputToSliderRange) {
            value = std::clamp(value, minValue, maxValue);
        }
        ImGui::PopID();
        ImGui::EndDisabled();
        return changed;
    }

    bool RenderFloatSliderWithInput(const char* label, float& value, float minValue, float maxValue, const char* format, bool disabled = false) {
        bool changed = false;
        ImGui::BeginDisabled(disabled);
        ImGui::PushID(label);
        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::SliderFloat("##slider", &value, minValue, maxValue, format)) changed = true;
        ImGui::SameLine();
        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::InputFloat(label, &value, 0.0f, 0.0f, format)) changed = true;
        value = std::clamp(value, minValue, maxValue);
        ImGui::PopID();
        ImGui::EndDisabled();
        return changed;
    }

    bool DrawIntValueSource(const char* valueLabel, int& flatValue, int minValue, int maxValue, ValueSource& source, RE::FormID& globalID, std::string& actorValue, bool clampInputToSliderRange = true) {
        bool changed = false;
        std::string sourceLabel = std::string(GetLoc("menu.value_source", "Value Source")) + "##" + valueLabel;
        if (DrawValueSourceCombo(sourceLabel.c_str(), source, globalID)) changed = true;
        if (RenderIntSliderWithInput(valueLabel, flatValue, minValue, maxValue, source != ValueSource::Flat, clampInputToSliderRange)) changed = true;

        if (source == ValueSource::Global) {
            std::string globalLabel = std::string(GetLoc("menu.value_global", "Global Value")) + "##" + valueLabel;
            if (DrawDropdown(globalLabel.c_str(), "Global", globalID, 300.0f)) changed = true;
        }
        else if (source == ValueSource::ActorValue) {
            std::string actorLabel = std::string(GetLoc("menu.value_actor_value", "Actor Value")) + "##" + valueLabel;
            if (RenderStringInput(actorLabel.c_str(), actorValue)) changed = true;
        }
        return changed;
    }

    bool DrawFloatValueSource(const char* valueLabel, float& flatValue, float minValue, float maxValue, const char* format, ValueSource& source, RE::FormID& globalID, std::string& actorValue) {
        bool changed = false;
        std::string sourceLabel = std::string(GetLoc("menu.value_source", "Value Source")) + "##" + valueLabel;
        if (DrawValueSourceCombo(sourceLabel.c_str(), source, globalID)) changed = true;
        if (RenderFloatSliderWithInput(valueLabel, flatValue, minValue, maxValue, format, source != ValueSource::Flat)) changed = true;

        if (source == ValueSource::Global) {
            std::string globalLabel = std::string(GetLoc("menu.value_global", "Global Value")) + "##" + valueLabel;
            if (DrawDropdown(globalLabel.c_str(), "Global", globalID, 300.0f)) changed = true;
        }
        else if (source == ValueSource::ActorValue) {
            std::string actorLabel = std::string(GetLoc("menu.value_actor_value", "Actor Value")) + "##" + valueLabel;
            if (RenderStringInput(actorLabel.c_str(), actorValue)) changed = true;
        }
        return changed;
    }

    bool DrawCostValueWithSource(const char* valueLabel, float& localCost, ValueSource& source, RE::FormID& costGlobal, std::string& actorValue) {
        return DrawFloatValueSource(valueLabel, localCost, 0.0f, 500.0f, "%.0f", source, costGlobal, actorValue);
    }

    bool DrawRestoreConfig(const char* title, const char* id, ParryRestoreConfig& config) {
        bool changed = false;
        const char* resourceOptions[] = {
            GetLoc("menu.opt_none", "None"),
            GetLoc("menu.opt_stamina", "Stamina"),
            GetLoc("menu.opt_magicka", "Magicka"),
            GetLoc("menu.opt_health", "Health")
        };
        const char* modeOptions[] = {
            GetLoc("menu.restore_flat", "Fixed Value"),
            GetLoc("menu.restore_percentage", "Percentage of Maximum")
        };

        ImGui::PushID(id);
        ImGui::TextColored({ 0.6f, 0.9f, 1.0f, 1.0f }, "%s", title);
        ImGui::Indent();

        int resource = static_cast<int>(config.resource);
        if (ImGui::Combo(GetLoc("menu.restore_resource", "Restore Resource"), &resource, resourceOptions, 4)) {
            config.resource = static_cast<CostType>(std::clamp(resource, 0, 3));
            changed = true;
        }

        if (config.resource != CostType::None) {
            int mode = static_cast<int>(config.mode);
            if (ImGui::Combo(GetLoc("menu.restore_mode", "Restore Calculation"), &mode, modeOptions, 2)) {
                config.mode = static_cast<RestoreMode>(std::clamp(mode, 0, 1));
                changed = true;
            }

            const float maxValue = config.mode == RestoreMode::Percentage ? 100.0f : 500.0f;
            const char* valueLabel = config.mode == RestoreMode::Percentage ?
                GetLoc("menu.restore_percentage_value", "Restore Amount (%)") :
                GetLoc("menu.restore_flat_value", "Restore Amount");
            if (DrawFloatValueSource(valueLabel, config.value, 0.0f, maxValue, "%.1f", config.valueSource, config.valueGlobal, config.valueActorValue)) {
                changed = true;
            }

        }

        ImGui::Unindent();
        ImGui::PopID();
        return changed;
    }

    bool SaveRule(const NPCParryRule& rule) {
        std::error_code ec;
        const std::filesystem::path rulesDir(RULES_DIR);
        std::filesystem::create_directories(rulesDir, ec);
        if (ec) {
            SKSE::log::error("[Settings] Failed to create rules directory '{}': {}", rulesDir.string(), ec.message());
            return false;
        }

        const auto filepath = GetRulePath(RULES_DIR, rule.ruleName);
        rapidjson::Document doc; doc.SetObject(); auto& alloc = doc.GetAllocator();

        rapidjson::Value rName; rName.SetString(rule.ruleName.c_str(), alloc);
        doc.AddMember("ruleName", rName, alloc);
        AddRuleFormIDMember(doc, alloc, "perk", rule.perkID);

        doc.AddMember("npcParryEnabled", rule.npcParryEnabled, alloc);
        doc.AddMember("npcNormalParryMS", rule.npcNormalParryMS, alloc);
        doc.AddMember("npcPerfectParryMS", rule.npcPerfectParryMS, alloc);
        AddRuleFormIDMember(doc, alloc, "npcNormalParryGlobal", rule.npcNormalParryGlobal);
        AddRuleFormIDMember(doc, alloc, "npcPerfectParryGlobal", rule.npcPerfectParryGlobal);
        AddSourceMember(doc, alloc, "npcNormalParrySource", rule.npcNormalParrySource);
        AddSourceMember(doc, alloc, "npcPerfectParrySource", rule.npcPerfectParrySource);
        AddStringMember(doc, alloc, "npcNormalParryActorValue", rule.npcNormalParryActorValue);
        AddStringMember(doc, alloc, "npcPerfectParryActorValue", rule.npcPerfectParryActorValue);
        AddRestoreConfig(doc, alloc, "npcNormalRestore", rule.npcNormalRestore);
        AddRestoreConfig(doc, alloc, "npcPerfectRestore", rule.npcPerfectRestore);
        doc.AddMember("npcArrowMode", rule.npcArrowMode, alloc);
        doc.AddMember("npcArrowReflectMode", rule.npcArrowReflectMode, alloc);
        doc.AddMember("npcArrowReflectTriggers", rule.npcArrowReflectTriggers, alloc);
        doc.AddMember("npcArrowCostType", static_cast<int>(rule.npcArrowCostType), alloc);
        doc.AddMember("npcArrowCost", rule.npcArrowCost, alloc);
        AddRuleFormIDMember(doc, alloc, "npcArrowCostGlobal", rule.npcArrowCostGlobal);
        AddSourceMember(doc, alloc, "npcArrowCostSource", rule.npcArrowCostSource);
        AddStringMember(doc, alloc, "npcArrowCostActorValue", rule.npcArrowCostActorValue);
        doc.AddMember("npcMagicMode", rule.npcMagicMode, alloc);
        doc.AddMember("npcMagicReflectMode", rule.npcMagicReflectMode, alloc);
        doc.AddMember("npcMagicReflectTriggers", rule.npcMagicReflectTriggers, alloc);
        doc.AddMember("npcMagicCostType", static_cast<int>(rule.npcMagicCostType), alloc);
        doc.AddMember("npcMagicCost", rule.npcMagicCost, alloc);
        AddRuleFormIDMember(doc, alloc, "npcMagicCostGlobal", rule.npcMagicCostGlobal);
        AddSourceMember(doc, alloc, "npcMagicCostSource", rule.npcMagicCostSource);
        AddStringMember(doc, alloc, "npcMagicCostActorValue", rule.npcMagicCostActorValue);
        doc.AddMember("npcReflectMeleeMode", rule.npcReflectMeleeMode, alloc);
        doc.AddMember("npcMeleeReflectTriggers", rule.npcMeleeReflectTriggers, alloc);
        doc.AddMember("npcReflectMeleeCostType", static_cast<int>(rule.npcReflectMeleeCostType), alloc);
        doc.AddMember("npcReflectMeleeCost", rule.npcReflectMeleeCost, alloc);
        AddRuleFormIDMember(doc, alloc, "npcReflectMeleeCostGlobal", rule.npcReflectMeleeCostGlobal);
        AddSourceMember(doc, alloc, "npcReflectMeleeCostSource", rule.npcReflectMeleeCostSource);
        AddStringMember(doc, alloc, "npcReflectMeleeCostActorValue", rule.npcReflectMeleeCostActorValue);

        // Novos campos salvos nas sub-regras
        doc.AddMember("npcReflectDamageType", rule.npcReflectDamageType, alloc);
        doc.AddMember("npcReflectValue", rule.npcReflectValue, alloc);
        AddRuleFormIDMember(doc, alloc, "npcReflectValueGlobal", rule.npcReflectValueGlobal);
        AddSourceMember(doc, alloc, "npcReflectValueSource", rule.npcReflectValueSource);
        AddStringMember(doc, alloc, "npcReflectValueActorValue", rule.npcReflectValueActorValue);

        doc.AddMember("npcMeleeStagger", rule.npcMeleeStagger, alloc);
        doc.AddMember("npcArrowStagger", rule.npcArrowStagger, alloc);
        doc.AddMember("npcMagicStagger", rule.npcMagicStagger, alloc);
        doc.AddMember("npcMeleeStaggerSize", rule.npcMeleeStaggerSize, alloc);
        doc.AddMember("npcArrowStaggerSize", rule.npcArrowStaggerSize, alloc);
        doc.AddMember("npcMagicStaggerSize", rule.npcMagicStaggerSize, alloc);
        doc.AddMember("npcVisualMode", rule.npcVisualMode, alloc);
        doc.AddMember("npcSoundMode", rule.npcSoundMode, alloc);

        std::ofstream file(filepath, std::ios::binary);
        if (!file.is_open()) {
            SKSE::log::error("[Settings] Failed to open rule for writing '{}'.", filepath.string());
            return false;
        }

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        if (!doc.Accept(writer)) {
            SKSE::log::error("[Settings] Failed to serialize rule '{}'.", rule.ruleName);
            return false;
        }

        file << buffer.GetString();
        if (!file.good()) {
            SKSE::log::error("[Settings] Failed to write rule '{}'.", filepath.string());
            return false;
        }

        return true;
    }

    bool SaveRules() {
        std::error_code ec;
        const std::filesystem::path rulesDir(RULES_DIR);
        std::filesystem::create_directories(rulesDir, ec);
        if (ec) {
            SKSE::log::error("[Settings] Failed to create rules directory '{}': {}", rulesDir.string(), ec.message());
            return false;
        }

        std::unordered_set<std::string> expectedFiles;
        expectedFiles.reserve(npcRules.size());
        for (const auto& rule : npcRules) {
            const auto filename = ToLower(GetRulePath(RULES_DIR, rule.ruleName).filename().string());
            if (!expectedFiles.insert(filename).second) {
                SKSE::log::error("[Settings] Multiple parry rules resolve to the same file '{}'.", filename);
                return false;
            }
        }

        bool saved = true;
        for (const auto& rule : npcRules) {
            saved = SaveRule(rule) && saved;
        }
        if (!saved) {
            return false;
        }

        for (std::filesystem::directory_iterator it(rulesDir, ec), end; it != end && !ec; it.increment(ec)) {
            const auto& entry = *it;
            if (!entry.is_regular_file(ec) || ec || ToLower(entry.path().extension().string()) != ".json") {
                continue;
            }

            const auto filename = ToLower(entry.path().filename().string());
            if (!expectedFiles.contains(filename)) {
                std::filesystem::remove(entry.path(), ec);
                if (ec) {
                    SKSE::log::error("[Settings] Failed to remove stale rule '{}': {}", entry.path().string(), ec.message());
                    return false;
                }
            }
        }
        if (ec) {
            SKSE::log::error("[Settings] Failed to synchronize rules directory '{}': {}", rulesDir.string(), ec.message());
            return false;
        }

        return true;
    }

    void LoadRules() {
        npcRules.clear();
        std::error_code ec;
        const bool newDirectoryExists = std::filesystem::exists(RULES_DIR, ec);
        if (ec) {
            SKSE::log::error("[Settings] Failed to inspect rules directory '{}': {}", RULES_DIR, ec.message());
            return;
        }

        const bool migrateLegacyRules = !newDirectoryExists;
        const std::string loadDir = migrateLegacyRules ? OLD_RULES_DIR : RULES_DIR;
        if (!std::filesystem::exists(loadDir, ec)) {
            if (ec) {
                SKSE::log::error("[Settings] Failed to inspect rules directory '{}': {}", loadDir, ec.message());
            }
            else if (migrateLegacyRules) {
                std::filesystem::create_directories(RULES_DIR, ec);
                if (ec) {
                    SKSE::log::error("[Settings] Failed to create canonical rules directory '{}': {}", RULES_DIR, ec.message());
                }
            }
            return;
        }

        for (std::filesystem::directory_iterator it(loadDir, ec), end; it != end && !ec; it.increment(ec)) {
            const auto& entry = *it;
            if (ToLower(entry.path().extension().string()) == ".json") {
                std::ifstream file(entry.path());
                if (!file.is_open()) {
                    SKSE::log::warn("[Settings] Failed to open rule '{}'.", entry.path().string());
                    continue;
                }
                std::stringstream buffer; buffer << file.rdbuf();
                rapidjson::Document doc; doc.Parse(buffer.str().c_str());
                if (doc.HasParseError() || !doc.IsObject()) {
                    SKSE::log::warn("[Settings] Invalid rule JSON '{}'.", entry.path().string());
                    continue;
                }

                NPCParryRule rule;
                if (doc.HasMember("ruleName")) rule.ruleName = doc["ruleName"].GetString();
                ReadRuleFormIDMember(doc, "perk", rule.perkID);

                if (doc.HasMember("npcParryEnabled")) rule.npcParryEnabled = doc["npcParryEnabled"].GetBool();
                if (doc.HasMember("npcNormalParryMS")) rule.npcNormalParryMS = doc["npcNormalParryMS"].GetInt();
                if (doc.HasMember("npcPerfectParryMS")) rule.npcPerfectParryMS = doc["npcPerfectParryMS"].GetInt();
                ReadRuleFormIDMember(doc, "npcNormalParryGlobal", rule.npcNormalParryGlobal);
                ReadRuleFormIDMember(doc, "npcPerfectParryGlobal", rule.npcPerfectParryGlobal);
                rule.npcNormalParrySource = ReadSource(doc, "npcNormalParrySource", rule.npcNormalParryGlobal != 0 ? ValueSource::Global : rule.npcNormalParrySource);
                rule.npcPerfectParrySource = ReadSource(doc, "npcPerfectParrySource", rule.npcPerfectParryGlobal != 0 ? ValueSource::Global : rule.npcPerfectParrySource);
                ReadActorValueMember(doc, "npcNormalParryActorValue", rule.npcNormalParryActorValue);
                ReadActorValueMember(doc, "npcPerfectParryActorValue", rule.npcPerfectParryActorValue);
                ReadRestoreConfig(doc, "npcNormalRestore", rule.npcNormalRestore);
                ReadRestoreConfig(doc, "npcPerfectRestore", rule.npcPerfectRestore);
                if (doc.HasMember("npcArrowMode")) rule.npcArrowMode = doc["npcArrowMode"].GetInt();
                if (doc.HasMember("npcArrowReflectMode")) rule.npcArrowReflectMode = doc["npcArrowReflectMode"].GetInt();
                if (doc.HasMember("npcArrowReflectTriggers")) rule.npcArrowReflectTriggers = doc["npcArrowReflectTriggers"].GetInt();
                if (doc.HasMember("npcArrowCostType")) rule.npcArrowCostType = static_cast<CostType>(doc["npcArrowCostType"].GetInt());
                if (doc.HasMember("npcArrowCost")) rule.npcArrowCost = doc["npcArrowCost"].GetFloat();
                ReadRuleFormIDMember(doc, "npcArrowCostGlobal", rule.npcArrowCostGlobal);
                rule.npcArrowCostSource = ReadSource(doc, "npcArrowCostSource", rule.npcArrowCostGlobal != 0 ? ValueSource::Global : rule.npcArrowCostSource);
                ReadActorValueMember(doc, "npcArrowCostActorValue", rule.npcArrowCostActorValue);
                if (doc.HasMember("npcMagicMode")) rule.npcMagicMode = doc["npcMagicMode"].GetInt();
                if (doc.HasMember("npcMagicReflectMode")) rule.npcMagicReflectMode = doc["npcMagicReflectMode"].GetInt();
                if (doc.HasMember("npcMagicReflectTriggers")) rule.npcMagicReflectTriggers = doc["npcMagicReflectTriggers"].GetInt();
                if (doc.HasMember("npcMagicCostType")) rule.npcMagicCostType = static_cast<CostType>(doc["npcMagicCostType"].GetInt());
                if (doc.HasMember("npcMagicCost")) rule.npcMagicCost = doc["npcMagicCost"].GetFloat();
                ReadRuleFormIDMember(doc, "npcMagicCostGlobal", rule.npcMagicCostGlobal);
                rule.npcMagicCostSource = ReadSource(doc, "npcMagicCostSource", rule.npcMagicCostGlobal != 0 ? ValueSource::Global : rule.npcMagicCostSource);
                ReadActorValueMember(doc, "npcMagicCostActorValue", rule.npcMagicCostActorValue);
                if (doc.HasMember("npcReflectMeleeMode")) rule.npcReflectMeleeMode = doc["npcReflectMeleeMode"].GetInt();
                if (doc.HasMember("npcMeleeReflectTriggers")) rule.npcMeleeReflectTriggers = doc["npcMeleeReflectTriggers"].GetInt();
                if (doc.HasMember("npcReflectMeleeCostType")) rule.npcReflectMeleeCostType = static_cast<CostType>(doc["npcReflectMeleeCostType"].GetInt());
                if (doc.HasMember("npcReflectMeleeCost")) rule.npcReflectMeleeCost = doc["npcReflectMeleeCost"].GetFloat();
                ReadRuleFormIDMember(doc, "npcReflectMeleeCostGlobal", rule.npcReflectMeleeCostGlobal);
                rule.npcReflectMeleeCostSource = ReadSource(doc, "npcReflectMeleeCostSource", rule.npcReflectMeleeCostGlobal != 0 ? ValueSource::Global : rule.npcReflectMeleeCostSource);
                ReadActorValueMember(doc, "npcReflectMeleeCostActorValue", rule.npcReflectMeleeCostActorValue);

                // Leitura dos novos campos nas regras por Perk (com fallback caso o JSON da regra seja antigo)
                if (doc.HasMember("npcReflectDamageType")) {
                    rule.npcReflectDamageType = doc["npcReflectDamageType"].GetInt();
                }
                else if (doc.HasMember("npcReflectSkillScaling")) {
                    rule.npcReflectDamageType = doc["npcReflectSkillScaling"].GetBool() ? 3 : 2;
                }
                if (doc.HasMember("npcReflectValue")) rule.npcReflectValue = doc["npcReflectValue"].GetFloat();
                ReadRuleFormIDMember(doc, "npcReflectValueGlobal", rule.npcReflectValueGlobal);
                rule.npcReflectValueSource = ReadSource(doc, "npcReflectValueSource", rule.npcReflectValueGlobal != 0 ? ValueSource::Global : rule.npcReflectValueSource);
                ReadActorValueMember(doc, "npcReflectValueActorValue", rule.npcReflectValueActorValue);

                if (doc.HasMember("npcMeleeStagger")) rule.npcMeleeStagger = doc["npcMeleeStagger"].GetInt();
                if (doc.HasMember("npcArrowStagger")) rule.npcArrowStagger = doc["npcArrowStagger"].GetInt();
                if (doc.HasMember("npcMagicStagger")) rule.npcMagicStagger = doc["npcMagicStagger"].GetInt();
                if (doc.HasMember("npcMeleeStaggerSize")) rule.npcMeleeStaggerSize = doc["npcMeleeStaggerSize"].GetInt();
                if (doc.HasMember("npcArrowStaggerSize")) rule.npcArrowStaggerSize = doc["npcArrowStaggerSize"].GetInt();
                if (doc.HasMember("npcMagicStaggerSize")) rule.npcMagicStaggerSize = doc["npcMagicStaggerSize"].GetInt();
                if (doc.HasMember("npcVisualMode")) rule.npcVisualMode = doc["npcVisualMode"].GetInt();
                if (doc.HasMember("npcSoundMode")) rule.npcSoundMode = doc["npcSoundMode"].GetInt();

                if (IsRuleFileNameInUse(rule.ruleName)) {
                    const std::string baseName = rule.ruleName;
                    std::size_t suffix = 2;
                    do {
                        rule.ruleName = baseName + " " + std::to_string(suffix++);
                    } while (IsRuleFileNameInUse(rule.ruleName));
                    SKSE::log::warn("[Settings] Renamed duplicate rule '{}' to '{}' during load.", baseName, rule.ruleName);
                }
                npcRules.push_back(rule);
            }
        }

        if (ec) {
            SKSE::log::error("[Settings] Failed while loading rules from '{}': {}", loadDir, ec.message());
            return;
        }

        if (migrateLegacyRules) {
            if (SaveRules()) {
                SKSE::log::info("[Settings] Migrated {} legacy parry rule(s) from '{}' to '{}'.",
                    npcRules.size(), OLD_RULES_DIR, RULES_DIR);
            }
            else {
                SKSE::log::error("[Settings] Legacy parry rules were loaded, but migration to '{}' was incomplete.", RULES_DIR);
            }
        }
    }

    void Save() {
        std::filesystem::path path(NEW_SETTINGS_PATH);
        if (!std::filesystem::exists(path.parent_path())) { std::filesystem::create_directories(path.parent_path()); }
        rapidjson::Document doc; doc.SetObject(); auto& allocator = doc.GetAllocator();

        doc.AddMember("cancelParryOnBlockStop", cancelParryOnBlockStop, allocator);
        doc.AddMember("bashOpensParryWindow", bashOpensParryWindow, allocator);
        doc.AddMember("respectBlockAngle", respectBlockAngle, allocator);
        doc.AddMember("blockAngle", blockAngle, allocator);
        doc.AddMember("parryFeedback", parryFeedback, allocator);

        // Player
        doc.AddMember("playerNormalParryMS", playerNormalParryMS, allocator);
        doc.AddMember("playerPerfectParryMS", playerPerfectParryMS, allocator);
        doc.AddMember("playerParryEnabled", playerParryEnabled, allocator);
        AddFormIDMember(doc, allocator, "playerParryPerk", playerParryPerk);
        AddFormIDMember(doc, allocator, "playerDisableParryPerk", playerDisableParryPerk);
        AddFormIDMember(doc, allocator, "playerNormalParryGlobal", playerNormalParryGlobal);
        AddFormIDMember(doc, allocator, "playerPerfectParryGlobal", playerPerfectParryGlobal);
        AddSourceMember(doc, allocator, "playerNormalParrySource", playerNormalParrySource);
        AddSourceMember(doc, allocator, "playerPerfectParrySource", playerPerfectParrySource);
        AddStringMember(doc, allocator, "playerNormalParryActorValue", playerNormalParryActorValue);
        AddStringMember(doc, allocator, "playerPerfectParryActorValue", playerPerfectParryActorValue);
        AddRestoreConfig(doc, allocator, "playerNormalRestore", playerNormalRestore);
        AddRestoreConfig(doc, allocator, "playerPerfectRestore", playerPerfectRestore);
        doc.AddMember("playerArrowMode", playerArrowMode, allocator);
        AddFormIDMember(doc, allocator, "playerArrowParryPerk", playerArrowParryPerk);
        doc.AddMember("playerArrowReflectMode", playerArrowReflectMode, allocator);
        doc.AddMember("playerArrowReflectTriggers", playerArrowReflectTriggers, allocator);
        AddFormIDMember(doc, allocator, "playerArrowReflectPerk", playerArrowReflectPerk);
        doc.AddMember("playerArrowCostType", static_cast<int>(playerArrowCostType), allocator);
        doc.AddMember("playerArrowCost", playerArrowCost, allocator);
        AddFormIDMember(doc, allocator, "playerArrowCostGlobal", playerArrowCostGlobal);
        AddSourceMember(doc, allocator, "playerArrowCostSource", playerArrowCostSource);
        AddStringMember(doc, allocator, "playerArrowCostActorValue", playerArrowCostActorValue);
        doc.AddMember("playerMagicMode", playerMagicMode, allocator);
        AddFormIDMember(doc, allocator, "playerMagicParryPerk", playerMagicParryPerk);
        doc.AddMember("playerMagicReflectMode", playerMagicReflectMode, allocator);
        doc.AddMember("playerMagicReflectTriggers", playerMagicReflectTriggers, allocator);
        AddFormIDMember(doc, allocator, "playerMagicReflectPerk", playerMagicReflectPerk);
        doc.AddMember("playerMagicCostType", static_cast<int>(playerMagicCostType), allocator);
        doc.AddMember("playerMagicCost", playerMagicCost, allocator);
        AddFormIDMember(doc, allocator, "playerMagicCostGlobal", playerMagicCostGlobal);
        AddSourceMember(doc, allocator, "playerMagicCostSource", playerMagicCostSource);
        AddStringMember(doc, allocator, "playerMagicCostActorValue", playerMagicCostActorValue);
        doc.AddMember("playerReflectMeleeMode", playerReflectMeleeMode, allocator);
        doc.AddMember("playerMeleeReflectTriggers", playerMeleeReflectTriggers, allocator);
        AddFormIDMember(doc, allocator, "playerReflectMeleePerk", playerReflectMeleePerk);
        doc.AddMember("playerReflectMeleeCostType", static_cast<int>(playerReflectMeleeCostType), allocator);
        doc.AddMember("playerReflectMeleeCost", playerReflectMeleeCost, allocator);
        AddFormIDMember(doc, allocator, "playerReflectMeleeCostGlobal", playerReflectMeleeCostGlobal);
        AddSourceMember(doc, allocator, "playerReflectMeleeCostSource", playerReflectMeleeCostSource);
        AddStringMember(doc, allocator, "playerReflectMeleeCostActorValue", playerReflectMeleeCostActorValue);

        // Novos campos Player no JSON
        doc.AddMember("playerReflectDamageType", playerReflectDamageType, allocator);
        doc.AddMember("playerReflectValue", playerReflectValue, allocator);
        AddFormIDMember(doc, allocator, "playerReflectValueGlobal", playerReflectValueGlobal);
        AddSourceMember(doc, allocator, "playerReflectValueSource", playerReflectValueSource);
        AddStringMember(doc, allocator, "playerReflectValueActorValue", playerReflectValueActorValue);

        doc.AddMember("playerMeleeStagger", playerMeleeStagger, allocator);
        doc.AddMember("playerArrowStagger", playerArrowStagger, allocator);
        doc.AddMember("playerMagicStagger", playerMagicStagger, allocator);
        doc.AddMember("playerMeleeStaggerSize", playerMeleeStaggerSize, allocator);
        doc.AddMember("playerArrowStaggerSize", playerArrowStaggerSize, allocator);
        doc.AddMember("playerMagicStaggerSize", playerMagicStaggerSize, allocator);
        AddFormIDMember(doc, allocator, "playerMeleeStaggerPerk", playerMeleeStaggerPerk);
        AddFormIDMember(doc, allocator, "playerArrowStaggerPerk", playerArrowStaggerPerk);
        AddFormIDMember(doc, allocator, "playerMagicStaggerPerk", playerMagicStaggerPerk);
        doc.AddMember("playerVisualMode", playerVisualMode, allocator);
        doc.AddMember("playerSoundMode", playerSoundMode, allocator);

        // NPC Globais
        doc.AddMember("npcParryEnabled", npcParryEnabled, allocator);
        doc.AddMember("npcNormalParryMS", npcNormalParryMS, allocator);
        doc.AddMember("npcPerfectParryMS", npcPerfectParryMS, allocator);
        AddFormIDMember(doc, allocator, "npcParryPerk", npcParryPerk);
        AddFormIDMember(doc, allocator, "npcDisableParryPerk", npcDisableParryPerk);
        AddFormIDMember(doc, allocator, "npcNormalParryGlobal", npcNormalParryGlobal);
        AddFormIDMember(doc, allocator, "npcPerfectParryGlobal", npcPerfectParryGlobal);
        AddSourceMember(doc, allocator, "npcNormalParrySource", npcNormalParrySource);
        AddSourceMember(doc, allocator, "npcPerfectParrySource", npcPerfectParrySource);
        AddStringMember(doc, allocator, "npcNormalParryActorValue", npcNormalParryActorValue);
        AddStringMember(doc, allocator, "npcPerfectParryActorValue", npcPerfectParryActorValue);
        AddRestoreConfig(doc, allocator, "npcNormalRestore", npcNormalRestore);
        AddRestoreConfig(doc, allocator, "npcPerfectRestore", npcPerfectRestore);
        doc.AddMember("npcArrowMode", npcArrowMode, allocator);
        AddFormIDMember(doc, allocator, "npcArrowParryPerk", npcArrowParryPerk);
        doc.AddMember("npcArrowReflectMode", npcArrowReflectMode, allocator);
        doc.AddMember("npcArrowReflectTriggers", npcArrowReflectTriggers, allocator);
        AddFormIDMember(doc, allocator, "npcArrowReflectPerk", npcArrowReflectPerk);
        doc.AddMember("npcArrowCostType", static_cast<int>(npcArrowCostType), allocator);
        doc.AddMember("npcArrowCost", npcArrowCost, allocator);
        AddFormIDMember(doc, allocator, "npcArrowCostGlobal", npcArrowCostGlobal);
        AddSourceMember(doc, allocator, "npcArrowCostSource", npcArrowCostSource);
        AddStringMember(doc, allocator, "npcArrowCostActorValue", npcArrowCostActorValue);
        doc.AddMember("npcMagicMode", npcMagicMode, allocator);
        AddFormIDMember(doc, allocator, "npcMagicParryPerk", npcMagicParryPerk);
        doc.AddMember("npcMagicReflectMode", npcMagicReflectMode, allocator);
        doc.AddMember("npcMagicReflectTriggers", npcMagicReflectTriggers, allocator);
        AddFormIDMember(doc, allocator, "npcMagicReflectPerk", npcMagicReflectPerk);
        doc.AddMember("npcMagicCostType", static_cast<int>(npcMagicCostType), allocator);
        doc.AddMember("npcMagicCost", npcMagicCost, allocator);
        AddFormIDMember(doc, allocator, "npcMagicCostGlobal", npcMagicCostGlobal);
        AddSourceMember(doc, allocator, "npcMagicCostSource", npcMagicCostSource);
        AddStringMember(doc, allocator, "npcMagicCostActorValue", npcMagicCostActorValue);
        doc.AddMember("npcReflectMeleeMode", npcReflectMeleeMode, allocator);
        doc.AddMember("npcMeleeReflectTriggers", npcMeleeReflectTriggers, allocator);
        AddFormIDMember(doc, allocator, "npcReflectMeleePerk", npcReflectMeleePerk);
        doc.AddMember("npcReflectMeleeCostType", static_cast<int>(npcReflectMeleeCostType), allocator);
        doc.AddMember("npcReflectMeleeCost", npcReflectMeleeCost, allocator);
        AddFormIDMember(doc, allocator, "npcReflectMeleeCostGlobal", npcReflectMeleeCostGlobal);
        AddSourceMember(doc, allocator, "npcReflectMeleeCostSource", npcReflectMeleeCostSource);
        AddStringMember(doc, allocator, "npcReflectMeleeCostActorValue", npcReflectMeleeCostActorValue);

        // Novos campos NPC Global no JSON
        doc.AddMember("npcReflectDamageType", npcReflectDamageType, allocator);
        doc.AddMember("npcReflectValue", npcReflectValue, allocator);
        AddFormIDMember(doc, allocator, "npcReflectValueGlobal", npcReflectValueGlobal);
        AddSourceMember(doc, allocator, "npcReflectValueSource", npcReflectValueSource);
        AddStringMember(doc, allocator, "npcReflectValueActorValue", npcReflectValueActorValue);

        doc.AddMember("npcMeleeStagger", npcMeleeStagger, allocator);
        doc.AddMember("npcArrowStagger", npcArrowStagger, allocator);
        doc.AddMember("npcMagicStagger", npcMagicStagger, allocator);
        doc.AddMember("npcMeleeStaggerSize", npcMeleeStaggerSize, allocator);
        doc.AddMember("npcArrowStaggerSize", npcArrowStaggerSize, allocator);
        doc.AddMember("npcMagicStaggerSize", npcMagicStaggerSize, allocator);
        AddFormIDMember(doc, allocator, "npcMeleeStaggerPerk", npcMeleeStaggerPerk);
        AddFormIDMember(doc, allocator, "npcArrowStaggerPerk", npcArrowStaggerPerk);
        AddFormIDMember(doc, allocator, "npcMagicStaggerPerk", npcMagicStaggerPerk);
        doc.AddMember("npcVisualMode", npcVisualMode, allocator);
        doc.AddMember("npcSoundMode", npcSoundMode, allocator);

        doc.AddMember("npcParrySlowTime", npcParrySlowTime, allocator);
        doc.AddMember("npcParryPlayerEnabled", npcParryPlayerEnabled, allocator);
        doc.AddMember("slowTimeMultiplier", slowTimeMultiplier, allocator);
        doc.AddMember("slowTimeDurationMS", slowTimeDurationMS, allocator);
        AddFormIDMember(doc, allocator, "slowTimeMultiplierGlobal", slowTimeMultiplierGlobal);
        AddFormIDMember(doc, allocator, "slowTimeDurationGlobal", slowTimeDurationGlobal);
        AddSourceMember(doc, allocator, "slowTimeMultiplierSource", slowTimeMultiplierSource);
        AddSourceMember(doc, allocator, "slowTimeDurationSource", slowTimeDurationSource);
        AddStringMember(doc, allocator, "slowTimeMultiplierActorValue", slowTimeMultiplierActorValue);
        AddStringMember(doc, allocator, "slowTimeDurationActorValue", slowTimeDurationActorValue);
        doc.AddMember("playerParryCommitmentEnabled", playerParryCommitmentEnabled, allocator);
        doc.AddMember("playerParryCommitmentMS", playerParryCommitmentMS, allocator);
        doc.AddMember("playerCommitmentReductionNormalMS", playerCommitmentReductionNormalMS, allocator);
        doc.AddMember("playerCommitmentReductionPerfectMS", playerCommitmentReductionPerfectMS, allocator);
        AddFormIDMember(doc, allocator, "playerParryCommitmentGlobal", playerParryCommitmentGlobal);
        AddFormIDMember(doc, allocator, "playerCommitmentReductionNormalGlobal", playerCommitmentReductionNormalGlobal);
        AddFormIDMember(doc, allocator, "playerCommitmentReductionPerfectGlobal", playerCommitmentReductionPerfectGlobal);
        AddSourceMember(doc, allocator, "playerParryCommitmentSource", playerParryCommitmentSource);
        AddSourceMember(doc, allocator, "playerCommitmentReductionNormalSource", playerCommitmentReductionNormalSource);
        AddSourceMember(doc, allocator, "playerCommitmentReductionPerfectSource", playerCommitmentReductionPerfectSource);
        AddStringMember(doc, allocator, "playerParryCommitmentActorValue", playerParryCommitmentActorValue);
        AddStringMember(doc, allocator, "playerCommitmentReductionNormalActorValue", playerCommitmentReductionNormalActorValue);
        AddStringMember(doc, allocator, "playerCommitmentReductionPerfectActorValue", playerCommitmentReductionPerfectActorValue);

        doc.AddMember("w2wMode", w2wMode, allocator);
        doc.AddMember("w2wBaseChance", w2wBaseChance, allocator);
        doc.AddMember("w2wDamageMult", w2wDamageMult, allocator);
        doc.AddMember("w2wSkillMult", w2wSkillMult, allocator);
        doc.AddMember("w2wGlobalDifficulty", w2wGlobalDifficulty, allocator);
        doc.AddMember("w2wReactionTarget", w2wReactionTarget, allocator);
        doc.AddMember("w2wReactionType", w2wReactionType, allocator);

        FILE* fp = nullptr; fopen_s(&fp, NEW_SETTINGS_PATH, "wb");
        if (fp) {
            char writeBuffer[65536]; rapidjson::FileWriteStream os(fp, writeBuffer, sizeof(writeBuffer));
            rapidjson::Writer<rapidjson::FileWriteStream> writer(os); doc.Accept(writer); fclose(fp);
        }
        if (!SaveRules()) {
            SKSE::log::error("[Settings] One or more parry rules failed to save.");
        }
    }

    void Load() {
        FILE* fp = nullptr; fopen_s(&fp, NEW_SETTINGS_PATH, "rb");
        if (!fp) { fopen_s(&fp, LEGACY_SETTINGS_PATH, "rb"); }
        if (!fp) { fopen_s(&fp, OLD_SETTINGS_PATH, "rb"); }

        if (fp) {
            char readBuffer[65536]; rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
            rapidjson::Document doc; doc.ParseStream(is); fclose(fp);

            if (doc.IsObject()) {
                if (doc.HasMember("cancelParryOnBlockStop")) cancelParryOnBlockStop = doc["cancelParryOnBlockStop"].GetBool();
                if (doc.HasMember("bashOpensParryWindow")) bashOpensParryWindow = doc["bashOpensParryWindow"].GetBool();
                if (doc.HasMember("respectBlockAngle") && doc["respectBlockAngle"].IsBool()) respectBlockAngle = doc["respectBlockAngle"].GetBool();
                if (doc.HasMember("blockAngle") && doc["blockAngle"].IsNumber()) blockAngle = std::clamp(doc["blockAngle"].GetFloat(), 0.0f, 180.0f);
                parryFeedback = doc.HasMember("parryFeedback") && doc["parryFeedback"].IsInt() ?
                    std::clamp(doc["parryFeedback"].GetInt(), 0, 2) : 0;
                if (doc.HasMember("playerNormalParryMS")) playerNormalParryMS = doc["playerNormalParryMS"].GetInt();
                if (doc.HasMember("playerPerfectParryMS")) playerPerfectParryMS = doc["playerPerfectParryMS"].GetInt();
                if (doc.HasMember("playerParryEnabled")) playerParryEnabled = doc["playerParryEnabled"].GetBool();
                ReadFormIDMember(doc, "playerParryPerk", playerParryPerk);
                ReadFormIDMember(doc, "playerDisableParryPerk", playerDisableParryPerk);
                ReadFormIDMember(doc, "playerNormalParryGlobal", playerNormalParryGlobal);
                ReadFormIDMember(doc, "playerPerfectParryGlobal", playerPerfectParryGlobal);
                playerNormalParrySource = ReadSource(doc, "playerNormalParrySource", playerNormalParryGlobal != 0 ? ValueSource::Global : playerNormalParrySource);
                playerPerfectParrySource = ReadSource(doc, "playerPerfectParrySource", playerPerfectParryGlobal != 0 ? ValueSource::Global : playerPerfectParrySource);
                ReadActorValueMember(doc, "playerNormalParryActorValue", playerNormalParryActorValue);
                ReadActorValueMember(doc, "playerPerfectParryActorValue", playerPerfectParryActorValue);
                ReadRestoreConfig(doc, "playerNormalRestore", playerNormalRestore);
                ReadRestoreConfig(doc, "playerPerfectRestore", playerPerfectRestore);
                if (doc.HasMember("playerArrowMode")) playerArrowMode = doc["playerArrowMode"].GetInt();
                ReadFormIDMember(doc, "playerArrowParryPerk", playerArrowParryPerk);
                if (doc.HasMember("playerArrowReflectMode")) playerArrowReflectMode = doc["playerArrowReflectMode"].GetInt();
                if (doc.HasMember("playerArrowReflectTriggers")) playerArrowReflectTriggers = doc["playerArrowReflectTriggers"].GetInt();
                ReadFormIDMember(doc, "playerArrowReflectPerk", playerArrowReflectPerk);
                if (doc.HasMember("playerArrowCostType")) playerArrowCostType = static_cast<CostType>(doc["playerArrowCostType"].GetInt());
                if (doc.HasMember("playerArrowCost")) playerArrowCost = doc["playerArrowCost"].GetFloat();
                ReadFormIDMember(doc, "playerArrowCostGlobal", playerArrowCostGlobal);
                playerArrowCostSource = ReadSource(doc, "playerArrowCostSource", playerArrowCostGlobal != 0 ? ValueSource::Global : playerArrowCostSource);
                ReadActorValueMember(doc, "playerArrowCostActorValue", playerArrowCostActorValue);
                if (doc.HasMember("playerMagicMode")) playerMagicMode = doc["playerMagicMode"].GetInt();
                ReadFormIDMember(doc, "playerMagicParryPerk", playerMagicParryPerk);
                if (doc.HasMember("playerMagicReflectMode")) playerMagicReflectMode = doc["playerMagicReflectMode"].GetInt();
                if (doc.HasMember("playerMagicReflectTriggers")) playerMagicReflectTriggers = doc["playerMagicReflectTriggers"].GetInt();
                ReadFormIDMember(doc, "playerMagicReflectPerk", playerMagicReflectPerk);
                if (doc.HasMember("playerMagicCostType")) playerMagicCostType = static_cast<CostType>(doc["playerMagicCostType"].GetInt());
                if (doc.HasMember("playerMagicCost")) playerMagicCost = doc["playerMagicCost"].GetFloat();
                ReadFormIDMember(doc, "playerMagicCostGlobal", playerMagicCostGlobal);
                playerMagicCostSource = ReadSource(doc, "playerMagicCostSource", playerMagicCostGlobal != 0 ? ValueSource::Global : playerMagicCostSource);
                ReadActorValueMember(doc, "playerMagicCostActorValue", playerMagicCostActorValue);
                if (doc.HasMember("playerReflectMeleeMode")) playerReflectMeleeMode = doc["playerReflectMeleeMode"].GetInt();
                if (doc.HasMember("playerMeleeReflectTriggers")) playerMeleeReflectTriggers = doc["playerMeleeReflectTriggers"].GetInt();
                ReadFormIDMember(doc, "playerReflectMeleePerk", playerReflectMeleePerk);
                if (doc.HasMember("playerReflectMeleeCostType")) playerReflectMeleeCostType = static_cast<CostType>(doc["playerReflectMeleeCostType"].GetInt());
                if (doc.HasMember("playerReflectMeleeCost")) playerReflectMeleeCost = doc["playerReflectMeleeCost"].GetFloat();
                ReadFormIDMember(doc, "playerReflectMeleeCostGlobal", playerReflectMeleeCostGlobal);
                playerReflectMeleeCostSource = ReadSource(doc, "playerReflectMeleeCostSource", playerReflectMeleeCostGlobal != 0 ? ValueSource::Global : playerReflectMeleeCostSource);
                ReadActorValueMember(doc, "playerReflectMeleeCostActorValue", playerReflectMeleeCostActorValue);

                // MIGRÇÃO INTELIGENTE PLAYER: Se achar a flag antiga, define o modo equivalente
                if (doc.HasMember("playerReflectDamageType")) {
                    playerReflectDamageType = doc["playerReflectDamageType"].GetInt();
                }
                else if (doc.HasMember("playerReflectSkillScaling")) {
                    playerReflectDamageType = doc["playerReflectSkillScaling"].GetBool() ? 3 : 2;
                }
                if (doc.HasMember("playerReflectValue")) playerReflectValue = doc["playerReflectValue"].GetFloat();
                ReadFormIDMember(doc, "playerReflectValueGlobal", playerReflectValueGlobal);
                playerReflectValueSource = ReadSource(doc, "playerReflectValueSource", playerReflectValueGlobal != 0 ? ValueSource::Global : playerReflectValueSource);
                ReadActorValueMember(doc, "playerReflectValueActorValue", playerReflectValueActorValue);

                if (doc.HasMember("playerMeleeStagger")) playerMeleeStagger = doc["playerMeleeStagger"].GetInt();
                if (doc.HasMember("playerArrowStagger")) playerArrowStagger = doc["playerArrowStagger"].GetInt();
                if (doc.HasMember("playerMagicStagger")) playerMagicStagger = doc["playerMagicStagger"].GetInt();
                if (doc.HasMember("playerMeleeStaggerSize")) playerMeleeStaggerSize = doc["playerMeleeStaggerSize"].GetInt();
                if (doc.HasMember("playerArrowStaggerSize")) playerArrowStaggerSize = doc["playerArrowStaggerSize"].GetInt();
                if (doc.HasMember("playerMagicStaggerSize")) playerMagicStaggerSize = doc["playerMagicStaggerSize"].GetInt();
                ReadFormIDMember(doc, "playerMeleeStaggerPerk", playerMeleeStaggerPerk);
                ReadFormIDMember(doc, "playerArrowStaggerPerk", playerArrowStaggerPerk);
                ReadFormIDMember(doc, "playerMagicStaggerPerk", playerMagicStaggerPerk);
                if (doc.HasMember("playerVisualMode")) playerVisualMode = doc["playerVisualMode"].GetInt();
                if (doc.HasMember("playerSoundMode")) playerSoundMode = doc["playerSoundMode"].GetInt();

                if (doc.HasMember("npcParryEnabled")) npcParryEnabled = doc["npcParryEnabled"].GetBool();
                if (doc.HasMember("npcNormalParryMS")) npcNormalParryMS = doc["npcNormalParryMS"].GetInt();
                if (doc.HasMember("npcPerfectParryMS")) npcPerfectParryMS = doc["npcPerfectParryMS"].GetInt();
                ReadFormIDMember(doc, "npcParryPerk", npcParryPerk);
                ReadFormIDMember(doc, "npcDisableParryPerk", npcDisableParryPerk);
                ReadFormIDMember(doc, "npcNormalParryGlobal", npcNormalParryGlobal);
                ReadFormIDMember(doc, "npcPerfectParryGlobal", npcPerfectParryGlobal);
                npcNormalParrySource = ReadSource(doc, "npcNormalParrySource", npcNormalParryGlobal != 0 ? ValueSource::Global : npcNormalParrySource);
                npcPerfectParrySource = ReadSource(doc, "npcPerfectParrySource", npcPerfectParryGlobal != 0 ? ValueSource::Global : npcPerfectParrySource);
                ReadActorValueMember(doc, "npcNormalParryActorValue", npcNormalParryActorValue);
                ReadActorValueMember(doc, "npcPerfectParryActorValue", npcPerfectParryActorValue);
                ReadRestoreConfig(doc, "npcNormalRestore", npcNormalRestore);
                ReadRestoreConfig(doc, "npcPerfectRestore", npcPerfectRestore);
                if (doc.HasMember("npcArrowMode")) npcArrowMode = doc["npcArrowMode"].GetInt();
                ReadFormIDMember(doc, "npcArrowParryPerk", npcArrowParryPerk);
                if (doc.HasMember("npcArrowReflectMode")) npcArrowReflectMode = doc["npcArrowReflectMode"].GetInt();
                if (doc.HasMember("npcArrowReflectTriggers")) npcArrowReflectTriggers = doc["npcArrowReflectTriggers"].GetInt();
                ReadFormIDMember(doc, "npcArrowReflectPerk", npcArrowReflectPerk);
                if (doc.HasMember("npcArrowCostType")) npcArrowCostType = static_cast<CostType>(doc["npcArrowCostType"].GetInt());
                if (doc.HasMember("npcArrowCost")) npcArrowCost = doc["npcArrowCost"].GetFloat();
                ReadFormIDMember(doc, "npcArrowCostGlobal", npcArrowCostGlobal);
                npcArrowCostSource = ReadSource(doc, "npcArrowCostSource", npcArrowCostGlobal != 0 ? ValueSource::Global : npcArrowCostSource);
                ReadActorValueMember(doc, "npcArrowCostActorValue", npcArrowCostActorValue);
                if (doc.HasMember("npcMagicMode")) npcMagicMode = doc["npcMagicMode"].GetInt();
                ReadFormIDMember(doc, "npcMagicParryPerk", npcMagicParryPerk);
                if (doc.HasMember("npcMagicReflectMode")) npcMagicReflectMode = doc["npcMagicReflectMode"].GetInt();
                if (doc.HasMember("npcMagicReflectTriggers")) npcMagicReflectTriggers = doc["npcMagicReflectTriggers"].GetInt();
                ReadFormIDMember(doc, "npcMagicReflectPerk", npcMagicReflectPerk);
                if (doc.HasMember("npcMagicCostType")) npcMagicCostType = static_cast<CostType>(doc["npcMagicCostType"].GetInt());
                if (doc.HasMember("npcMagicCost")) npcMagicCost = doc["npcMagicCost"].GetFloat();
                ReadFormIDMember(doc, "npcMagicCostGlobal", npcMagicCostGlobal);
                npcMagicCostSource = ReadSource(doc, "npcMagicCostSource", npcMagicCostGlobal != 0 ? ValueSource::Global : npcMagicCostSource);
                ReadActorValueMember(doc, "npcMagicCostActorValue", npcMagicCostActorValue);
                if (doc.HasMember("npcReflectMeleeMode")) npcReflectMeleeMode = doc["npcReflectMeleeMode"].GetInt();
                if (doc.HasMember("npcMeleeReflectTriggers")) npcMeleeReflectTriggers = doc["npcMeleeReflectTriggers"].GetInt();
                ReadFormIDMember(doc, "npcReflectMeleePerk", npcReflectMeleePerk);
                if (doc.HasMember("npcReflectMeleeCostType")) npcReflectMeleeCostType = static_cast<CostType>(doc["npcReflectMeleeCostType"].GetInt());
                if (doc.HasMember("npcReflectMeleeCost")) npcReflectMeleeCost = doc["npcReflectMeleeCost"].GetFloat();
                ReadFormIDMember(doc, "npcReflectMeleeCostGlobal", npcReflectMeleeCostGlobal);
                npcReflectMeleeCostSource = ReadSource(doc, "npcReflectMeleeCostSource", npcReflectMeleeCostGlobal != 0 ? ValueSource::Global : npcReflectMeleeCostSource);
                ReadActorValueMember(doc, "npcReflectMeleeCostActorValue", npcReflectMeleeCostActorValue);

                // MIGRÇÃO INTELIGENTE NPC GLOBAL
                if (doc.HasMember("npcReflectDamageType")) {
                    npcReflectDamageType = doc["npcReflectDamageType"].GetInt();
                }
                else if (doc.HasMember("npcReflectSkillScaling")) {
                    npcReflectDamageType = doc["npcReflectSkillScaling"].GetBool() ? 3 : 2;
                }
                if (doc.HasMember("npcReflectValue")) npcReflectValue = doc["npcReflectValue"].GetFloat();
                ReadFormIDMember(doc, "npcReflectValueGlobal", npcReflectValueGlobal);
                npcReflectValueSource = ReadSource(doc, "npcReflectValueSource", npcReflectValueGlobal != 0 ? ValueSource::Global : npcReflectValueSource);
                ReadActorValueMember(doc, "npcReflectValueActorValue", npcReflectValueActorValue);

                if (doc.HasMember("npcMeleeStagger")) npcMeleeStagger = doc["npcMeleeStagger"].GetInt();
                if (doc.HasMember("npcArrowStagger")) npcArrowStagger = doc["npcArrowStagger"].GetInt();
                if (doc.HasMember("npcMagicStagger")) npcMagicStagger = doc["npcMagicStagger"].GetInt();
                if (doc.HasMember("npcMeleeStaggerSize")) npcMeleeStaggerSize = doc["npcMeleeStaggerSize"].GetInt();
                if (doc.HasMember("npcArrowStaggerSize")) npcArrowStaggerSize = doc["npcArrowStaggerSize"].GetInt();
                if (doc.HasMember("npcMagicStaggerSize")) npcMagicStaggerSize = doc["npcMagicStaggerSize"].GetInt();
                ReadFormIDMember(doc, "npcMeleeStaggerPerk", npcMeleeStaggerPerk);
                ReadFormIDMember(doc, "npcArrowStaggerPerk", npcArrowStaggerPerk);
                ReadFormIDMember(doc, "npcMagicStaggerPerk", npcMagicStaggerPerk);
                if (doc.HasMember("npcVisualMode")) npcVisualMode = doc["npcVisualMode"].GetInt();
                if (doc.HasMember("npcSoundMode")) npcSoundMode = doc["npcSoundMode"].GetInt();

                if (doc.HasMember("npcParrySlowTime")) npcParrySlowTime = doc["npcParrySlowTime"].GetBool();
                if (doc.HasMember("npcParryPlayerEnabled")) npcParryPlayerEnabled = doc["npcParryPlayerEnabled"].GetBool();
                if (doc.HasMember("slowTimeMultiplier")) slowTimeMultiplier = doc["slowTimeMultiplier"].GetFloat();
                if (doc.HasMember("slowTimeDurationMS")) slowTimeDurationMS = doc["slowTimeDurationMS"].GetInt();
                ReadFormIDMember(doc, "slowTimeMultiplierGlobal", slowTimeMultiplierGlobal);
                ReadFormIDMember(doc, "slowTimeDurationGlobal", slowTimeDurationGlobal);
                slowTimeMultiplierSource = ReadSource(doc, "slowTimeMultiplierSource", slowTimeMultiplierGlobal != 0 ? ValueSource::Global : slowTimeMultiplierSource);
                slowTimeDurationSource = ReadSource(doc, "slowTimeDurationSource", slowTimeDurationGlobal != 0 ? ValueSource::Global : slowTimeDurationSource);
                ReadActorValueMember(doc, "slowTimeMultiplierActorValue", slowTimeMultiplierActorValue);
                ReadActorValueMember(doc, "slowTimeDurationActorValue", slowTimeDurationActorValue);
                if (doc.HasMember("playerParryCommitmentEnabled")) playerParryCommitmentEnabled = doc["playerParryCommitmentEnabled"].GetBool();
                if (doc.HasMember("playerParryCommitmentMS")) playerParryCommitmentMS = doc["playerParryCommitmentMS"].GetInt();
                if (doc.HasMember("playerCommitmentReductionNormalMS")) playerCommitmentReductionNormalMS = doc["playerCommitmentReductionNormalMS"].GetInt();
                if (doc.HasMember("playerCommitmentReductionPerfectMS")) playerCommitmentReductionPerfectMS = doc["playerCommitmentReductionPerfectMS"].GetInt();
                ReadFormIDMember(doc, "playerParryCommitmentGlobal", playerParryCommitmentGlobal);
                ReadFormIDMember(doc, "playerCommitmentReductionNormalGlobal", playerCommitmentReductionNormalGlobal);
                ReadFormIDMember(doc, "playerCommitmentReductionPerfectGlobal", playerCommitmentReductionPerfectGlobal);
                playerParryCommitmentSource = ReadSource(doc, "playerParryCommitmentSource", playerParryCommitmentGlobal != 0 ? ValueSource::Global : playerParryCommitmentSource);
                playerCommitmentReductionNormalSource = ReadSource(doc, "playerCommitmentReductionNormalSource", playerCommitmentReductionNormalGlobal != 0 ? ValueSource::Global : playerCommitmentReductionNormalSource);
                playerCommitmentReductionPerfectSource = ReadSource(doc, "playerCommitmentReductionPerfectSource", playerCommitmentReductionPerfectGlobal != 0 ? ValueSource::Global : playerCommitmentReductionPerfectSource);
                ReadActorValueMember(doc, "playerParryCommitmentActorValue", playerParryCommitmentActorValue);
                ReadActorValueMember(doc, "playerCommitmentReductionNormalActorValue", playerCommitmentReductionNormalActorValue);
                ReadActorValueMember(doc, "playerCommitmentReductionPerfectActorValue", playerCommitmentReductionPerfectActorValue);

                if (doc.HasMember("w2wMode")) w2wMode = doc["w2wMode"].GetInt();
                if (doc.HasMember("w2wBaseChance")) w2wBaseChance = doc["w2wBaseChance"].GetFloat();
                if (doc.HasMember("w2wDamageMult")) w2wDamageMult = doc["w2wDamageMult"].GetFloat();
                if (doc.HasMember("w2wSkillMult")) w2wSkillMult = doc["w2wSkillMult"].GetFloat();
                if (doc.HasMember("w2wGlobalDifficulty")) w2wGlobalDifficulty = doc["w2wGlobalDifficulty"].GetFloat();
                if (doc.HasMember("w2wReactionTarget")) w2wReactionTarget = doc["w2wReactionTarget"].GetInt();
                if (doc.HasMember("w2wReactionType")) w2wReactionType = doc["w2wReactionType"].GetInt();
            }
        }
        LoadRules();
    }

    void NPCMenu() {
        bool changed = false;

        // Opções localizadas para as Combos
        const char* modeOpts[] = { GetLoc("menu.opt_disabled", "Disabled"), GetLoc("menu.opt_enabled_free", "Enabled (Free)"), GetLoc("menu.opt_enabled_cost", "Enabled (Cost)") };
        const char* reflectOpts[] = { GetLoc("menu.opt_disable", "Disable"), GetLoc("menu.opt_enable", "Enable"), GetLoc("menu.opt_enable_perfect", "Enable only in perfect") };
        const char* costOpts[] = { GetLoc("menu.opt_none", "None"), GetLoc("menu.opt_stamina", "Stamina"), GetLoc("menu.opt_magicka", "Magicka"), GetLoc("menu.opt_health", "Health") };

        if (ImGui::CollapsingHeader(GetLoc("menu.npc_global_header", "NPC Global Settings"), ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::Checkbox(GetLoc("menu.npc_enable_parry", "Enable NPC Parry"), &npcParryEnabled)) changed = true;

            if (npcParryEnabled) {
                ImGui::Indent();
                if (ImGui::CollapsingHeader(GetLoc("menu.timing_header", "Timing"))) {
                    ImGui::Indent();
                    if (DrawIntValueSource(GetLoc("menu.npc_normal_window", "NPC Normal Window (ms)"), npcNormalParryMS, 50, 2000, npcNormalParrySource, npcNormalParryGlobal, npcNormalParryActorValue, false)) changed = true;
                    if (DrawIntValueSource(GetLoc("menu.npc_perfect_window", "NPC Perfect Window (ms)"), npcPerfectParryMS, 10, 500, npcPerfectParrySource, npcPerfectParryGlobal, npcPerfectParryActorValue, false)) changed = true;
                    ImGui::Unindent();
                }
                ImGui::Spacing();
                if (ImGui::CollapsingHeader(GetLoc("menu.restore_header", "Restore on Parry"))) {
                    ImGui::Indent();
                    if (DrawRestoreConfig(GetLoc("menu.restore_normal", "Normal Parry"), "NPCNormalRestore", npcNormalRestore)) changed = true;
                    if (DrawRestoreConfig(GetLoc("menu.restore_perfect", "Perfect Parry"), "NPCPerfectRestore", npcPerfectRestore)) changed = true;
                    ImGui::Unindent();
                }
                ImGui::Spacing();
                if (ImGui::CollapsingHeader(GetLoc("menu.effects_header", "Effects"))) {
                    ImGui::Indent();
                    if (ImGui::Combo(GetLoc("menu.npc_visual_effects", "NPC Visual Effects"), &npcVisualMode, reflectOpts, 3)) changed = true;
                    if (ImGui::Combo(GetLoc("menu.npc_sound_effects", "NPC Sound Effects"), &npcSoundMode, reflectOpts, 3)) changed = true;
                    ImGui::Spacing();
                    if (ImGui::Checkbox(GetLoc("menu.npc_can_parry_player", "Can Parry Player"), &npcParryPlayerEnabled)) changed = true;
                    if (npcParryPlayerEnabled) {
                        if (ImGui::Checkbox(GetLoc("menu.npc_slow_time", "Slow Time on NPC Parry the Player"), &npcParrySlowTime)) changed = true;
                    }
                    ImGui::Unindent();
                }
                ImGui::Spacing();
                if (ImGui::CollapsingHeader(GetLoc("menu.melee_header", "Melee"))) {
                    ImGui::Indent();
                    if (ImGui::Combo(GetLoc("menu.npc_stagger_melee", "Stagger Attacker on Melee Parry"), &npcMeleeStagger, reflectOpts, 3)) changed = true;
                    if (npcMeleeStagger > 0) {
                        ImGui::Indent();
                        if (DrawStaggerSizeCombo(GetLoc("menu.npc_melee_stagger_size", "Melee Stagger Size"), &npcMeleeStaggerSize)) changed = true;
                        ImGui::Unindent();
                    }
                    if (ImGui::Combo(GetLoc("menu.npc_reflect_melee", "NPC Reflect Melee Damage"), &npcReflectMeleeMode, reflectOpts, 3)) changed = true;
                    if (npcReflectMeleeMode > 0) {
                        ImGui::Indent();
                        if (DrawReflectTriggers("NPCMelee", npcMeleeReflectTriggers)) changed = true;
                        std::string comboLabel = std::string(GetLoc("menu.reflect_calc_type", "Reflect Calculation Type")) + "##NPCGlobal";
                        if (DrawReflectTypeCombo(comboLabel.c_str(), &npcReflectDamageType)) changed = true;
                        if (npcReflectDamageType == 0) {
                            std::string flatLabel = std::string(GetLoc("menu.flat_damage_val", "Flat Damage Value")) + "##NPCGlobal";
                            if (DrawFloatValueSource(flatLabel.c_str(), npcReflectValue, 1.0f, 500.0f, "%.0f", npcReflectValueSource, npcReflectValueGlobal, npcReflectValueActorValue)) changed = true;
                        }
                        else if (npcReflectDamageType == 1) {
                            std::string pctLabel = std::string(GetLoc("menu.pct_modifier", "Percentage Modifier (%)")) + "##NPCGlobal";
                            if (DrawFloatValueSource(pctLabel.c_str(), npcReflectValue, 1.0f, 200.0f, "%.0f", npcReflectValueSource, npcReflectValueGlobal, npcReflectValueActorValue)) changed = true;
                        }
                        if (ImGui::Combo(GetLoc("menu.npc_reflect_cost_type", "NPC Reflect Cost Type"), (int*)&npcReflectMeleeCostType, costOpts, 4)) changed = true;
                        if (npcReflectMeleeCostType != CostType::None && DrawCostValueWithSource(GetLoc("menu.npc_reflect_cost", "NPC Reflect Cost"), npcReflectMeleeCost, npcReflectMeleeCostSource, npcReflectMeleeCostGlobal, npcReflectMeleeCostActorValue)) changed = true;
                        ImGui::Unindent();
                    }
                    ImGui::Unindent();
                }
                ImGui::Spacing();
                if (ImGui::CollapsingHeader(GetLoc("menu.projectiles_header", "Projectiles"))) {
                    ImGui::Indent();
                    if (ImGui::Combo(GetLoc("menu.npc_parry_arrows", "NPC Parry Arrows"), &npcArrowMode, modeOpts, 3)) changed = true;
                    if (npcArrowMode > 0) {
                        ImGui::Indent();
                        if (ImGui::Combo(GetLoc("menu.npc_reflect_arrows", "NPC Reflect Arrows"), &npcArrowReflectMode, reflectOpts, 3)) changed = true;
                        if (npcArrowReflectMode > 0) {
                            if (DrawReflectTriggers("NPCArrow", npcArrowReflectTriggers)) changed = true;
                        }
                        if (npcArrowMode == 2) {
                            if (ImGui::Combo(GetLoc("menu.npc_arrow_cost", "NPC Arrow Cost"), (int*)&npcArrowCostType, costOpts, 4)) changed = true;
                            if (npcArrowCostType != CostType::None && DrawCostValueWithSource(GetLoc("menu.npc_arrow_cost_value", "NPC Arrow Cost Value"), npcArrowCost, npcArrowCostSource, npcArrowCostGlobal, npcArrowCostActorValue)) changed = true;
                        }
                        if (ImGui::Combo(GetLoc("menu.npc_stagger_shooter", "Stagger Shooter"), &npcArrowStagger, reflectOpts, 3)) changed = true;
                        if (npcArrowStagger > 0) {
                            if (DrawStaggerSizeCombo(GetLoc("menu.npc_arrow_stagger_size", "Arrow Stagger Size"), &npcArrowStaggerSize)) changed = true;
                        }
                        ImGui::Unindent();
                    }
                    ImGui::Unindent();
                }
                ImGui::Spacing();
                if (ImGui::CollapsingHeader(GetLoc("menu.magic_header", "Magic"))) {
                    ImGui::Indent();
                    if (ImGui::Combo(GetLoc("menu.npc_parry_magic", "NPC Parry Magic"), &npcMagicMode, modeOpts, 3)) changed = true;
                    if (npcMagicMode > 0) {
                        ImGui::Indent();
                        if (ImGui::Combo(GetLoc("menu.npc_reflect_magic", "NPC Reflect Magic"), &npcMagicReflectMode, reflectOpts, 3)) changed = true;
                        if (npcMagicReflectMode > 0) {
                            if (DrawReflectTriggers("NPCMagic", npcMagicReflectTriggers)) changed = true;
                        }
                        if (npcMagicMode == 2) {
                            if (ImGui::Combo(GetLoc("menu.npc_magic_cost", "NPC Magic Cost"), (int*)&npcMagicCostType, costOpts, 4)) changed = true;
                            if (npcMagicCostType != CostType::None && DrawCostValueWithSource(GetLoc("menu.npc_magic_cost_value", "NPC Magic Cost Value"), npcMagicCost, npcMagicCostSource, npcMagicCostGlobal, npcMagicCostActorValue)) changed = true;
                        }
                        if (ImGui::Combo(GetLoc("menu.npc_stagger_caster", "Stagger Caster"), &npcMagicStagger, reflectOpts, 3)) changed = true;
                        if (npcMagicStagger > 0) {
                            if (DrawStaggerSizeCombo(GetLoc("menu.npc_magic_stagger_size", "Magic Stagger Size"), &npcMagicStaggerSize)) changed = true;
                        }
                        ImGui::Unindent();
                    }
                    ImGui::Unindent();
                }
                ImGui::Unindent();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            if (ImGui::CollapsingHeader(GetLoc("menu.perk_locks_header", "Perk Locks"))) {
                ImGui::Indent();
                const char* npcParryRequirement = GetLoc("menu.requires_npc_parry", "Enable NPC Parry");
                if (DrawConditionalPerkDropdown(GetLoc("menu.npc_parry_perk", "Lock Parry Behind Perk"), npcParryPerk, npcParryEnabled, npcParryRequirement)) changed = true;
                if (DrawConditionalPerkDropdown(GetLoc("menu.npc_disable_parry_perk", "Disable Parry If Actor Has Perk"), npcDisableParryPerk, npcParryEnabled, npcParryRequirement)) changed = true;
                if (DrawConditionalPerkDropdown(GetLoc("menu.restore_normal_perk", "Lock Normal Restore Behind Perk"), npcNormalRestore.perk, npcParryEnabled && npcNormalRestore.resource != CostType::None, GetLoc("menu.requires_normal_restore", "Enable Parry and Normal Parry Restore"))) changed = true;
                if (DrawConditionalPerkDropdown(GetLoc("menu.restore_perfect_perk", "Lock Perfect Restore Behind Perk"), npcPerfectRestore.perk, npcParryEnabled && npcPerfectRestore.resource != CostType::None, GetLoc("menu.requires_perfect_restore", "Enable Parry and Perfect Parry Restore"))) changed = true;
                if (DrawConditionalPerkDropdown(GetLoc("menu.npc_melee_stagger_perk", "Lock Melee Stagger Behind Perk"), npcMeleeStaggerPerk, npcParryEnabled && npcMeleeStagger > 0, GetLoc("menu.requires_melee_stagger", "Enable Parry and Melee Stagger"))) changed = true;
                if (DrawConditionalPerkDropdown(GetLoc("menu.npc_reflect_melee_perk", "Lock Melee Reflect Behind Perk"), npcReflectMeleePerk, npcParryEnabled && npcReflectMeleeMode > 0, GetLoc("menu.requires_melee_reflect", "Enable Parry and Melee Reflect"))) changed = true;
                if (DrawConditionalPerkDropdown(GetLoc("menu.npc_arrow_parry_perk", "Lock Arrow Parry Behind Perk"), npcArrowParryPerk, npcParryEnabled && npcArrowMode > 0, GetLoc("menu.requires_arrow_parry", "Enable Parry and Arrow Parry"))) changed = true;
                if (DrawConditionalPerkDropdown(GetLoc("menu.npc_arrow_reflect_perk", "Lock Arrow Reflect Behind Perk"), npcArrowReflectPerk, npcParryEnabled && npcArrowMode > 0 && npcArrowReflectMode > 0, GetLoc("menu.requires_arrow_reflect", "Enable Parry, Arrow Parry, and Arrow Reflect"))) changed = true;
                if (DrawConditionalPerkDropdown(GetLoc("menu.npc_arrow_stagger_perk", "Lock Arrow Stagger Behind Perk"), npcArrowStaggerPerk, npcParryEnabled && npcArrowMode > 0 && npcArrowStagger > 0, GetLoc("menu.requires_arrow_stagger", "Enable Parry, Arrow Parry, and Arrow Stagger"))) changed = true;
                if (DrawConditionalPerkDropdown(GetLoc("menu.npc_magic_parry_perk", "Lock Magic Parry Behind Perk"), npcMagicParryPerk, npcParryEnabled && npcMagicMode > 0, GetLoc("menu.requires_magic_parry", "Enable Parry and Magic Parry"))) changed = true;
                if (DrawConditionalPerkDropdown(GetLoc("menu.npc_magic_reflect_perk", "Lock Magic Reflect Behind Perk"), npcMagicReflectPerk, npcParryEnabled && npcMagicMode > 0 && npcMagicReflectMode > 0, GetLoc("menu.requires_magic_reflect", "Enable Parry, Magic Parry, and Magic Reflect"))) changed = true;
                if (DrawConditionalPerkDropdown(GetLoc("menu.npc_magic_stagger_perk", "Lock Magic Stagger Behind Perk"), npcMagicStaggerPerk, npcParryEnabled && npcMagicMode > 0 && npcMagicStagger > 0, GetLoc("menu.requires_magic_stagger", "Enable Parry, Magic Parry, and Magic Stagger"))) changed = true;
                ImGui::Unindent();
            }
        }

        if (changed) Save();
    }

    void RulesMenu() {
        bool changed = false;

        const char* modeOpts[] = { GetLoc("menu.opt_disabled", "Disabled"), GetLoc("menu.opt_enabled_free", "Enabled (Free)"), GetLoc("menu.opt_enabled_cost", "Enabled (Cost)") };
        const char* reflectOpts[] = { GetLoc("menu.opt_disable", "Disable"), GetLoc("menu.opt_enable", "Enable"), GetLoc("menu.opt_enable_perfect", "Enable only in perfect") };
        const char* costOpts[] = { GetLoc("menu.opt_none", "None"), GetLoc("menu.opt_stamina", "Stamina"), GetLoc("menu.opt_magicka", "Magicka"), GetLoc("menu.opt_health", "Health") };

        ImGui::TextColored({ 0.4f, 1.0f, 0.4f, 1.0f }, "%s", GetLoc("menu.npc_rules_perk_header", "NPC Parry Rules (By Perk)"));
        if (ImGui::Button(GetLoc("menu.add_rule", "+ Add Rule"))) {
            NPCParryRule newRule;
            std::size_t suffix = 1;
            do {
                newRule.ruleName = "New Rule " + std::to_string(suffix++);
            } while (IsRuleFileNameInUse(newRule.ruleName));
            npcRules.push_back(newRule);
            changed = true;
        }

        ImGui::Spacing();

        for (size_t i = 0; i < npcRules.size(); ) {
            auto& rule = npcRules[i];
            ImGui::PushID(static_cast<int>(i));

            if (ImGui::CollapsingHeader(rule.ruleName.c_str())) {
                ImGui::Indent();

                char nameBuf[128];
                strcpy_s(nameBuf, rule.ruleName.c_str());
                if (ImGui::InputText(GetLoc("menu.rule_name", "Rule Name"), nameBuf, sizeof(nameBuf))) {
                    std::string newName(nameBuf);
                    if (newName != rule.ruleName && !newName.empty()) {
                        if (IsRuleFileNameInUse(newName, &rule)) {
                            SKSE::log::warn("[Settings] Rule name '{}' conflicts with another rule file.", newName);
                        }
                        else {
                            const auto oldPath = GetRulePath(RULES_DIR, rule.ruleName);
                            std::error_code ec;
                            std::filesystem::remove(oldPath, ec);
                            if (ec) {
                                SKSE::log::warn("[Settings] Failed to remove renamed rule '{}': {}", oldPath.string(), ec.message());
                            }
                            rule.ruleName = newName;
                            changed = true;
                        }
                    }
                }

                RE::FormID prevPerk = rule.perkID;
                std::string perkLabel = std::string(GetLoc("menu.target_perk", "Target Perk")) + "##" + std::to_string(i);
                if (DrawDropdown(perkLabel.c_str(), "Perk", rule.perkID, 300.0f)) {
                    bool conflict = false;
                    if (rule.perkID != 0) {
                        for (size_t j = 0; j < npcRules.size(); j++) {
                            if (i != j && npcRules[j].perkID == rule.perkID) { conflict = true; break; }
                        }
                    }
                    if (conflict) rule.perkID = prevPerk;
                    else changed = true;
                }

                ImGui::Separator();
                if (ImGui::Checkbox(GetLoc("menu.rule_enable_parry", "Enable Parry in this Rule"), &rule.npcParryEnabled)) changed = true;

                if (rule.npcParryEnabled) {
                    if (ImGui::CollapsingHeader(GetLoc("menu.timing_header", "Timing"))) {
                        ImGui::Indent();
                        std::string normalLabel = std::string(GetLoc("menu.normal_window", "Normal Window (ms)")) + "##Rule";
                        if (DrawIntValueSource(normalLabel.c_str(), rule.npcNormalParryMS, 50, 2000, rule.npcNormalParrySource, rule.npcNormalParryGlobal, rule.npcNormalParryActorValue, false)) changed = true;
                        std::string perfectLabel = std::string(GetLoc("menu.perfect_window", "Perfect Window (ms)")) + "##Rule";
                        if (DrawIntValueSource(perfectLabel.c_str(), rule.npcPerfectParryMS, 10, 500, rule.npcPerfectParrySource, rule.npcPerfectParryGlobal, rule.npcPerfectParryActorValue, false)) changed = true;
                        ImGui::Unindent();
                    }

                    if (ImGui::CollapsingHeader(GetLoc("menu.restore_header", "Restore on Parry"))) {
                        ImGui::Indent();
                        if (DrawRestoreConfig(GetLoc("menu.restore_normal", "Normal Parry"), "RuleNormalRestore", rule.npcNormalRestore)) changed = true;
                        if (DrawRestoreConfig(GetLoc("menu.restore_perfect", "Perfect Parry"), "RulePerfectRestore", rule.npcPerfectRestore)) changed = true;
                        ImGui::Unindent();
                    }

                    if (ImGui::CollapsingHeader(GetLoc("menu.projectiles_header", "Projectiles"))) {
                        ImGui::Indent();
                        std::string parryArrowsLabel = std::string(GetLoc("menu.parry_arrows", "Parry Arrows")) + "##Rule";
                        if (ImGui::Combo(parryArrowsLabel.c_str(), &rule.npcArrowMode, modeOpts, 3)) changed = true;
                        if (rule.npcArrowMode > 0) {
                            ImGui::Indent();
                            std::string reflectArrowsLabel = std::string(GetLoc("menu.reflect_arrows", "Reflect Arrows")) + "##Rule";
                            if (ImGui::Combo(reflectArrowsLabel.c_str(), &rule.npcArrowReflectMode, reflectOpts, 3)) changed = true;
                            if (rule.npcArrowReflectMode > 0) {
                                std::string triggerID = "RuleArrow" + std::to_string(i);
                                if (DrawReflectTriggers(triggerID.c_str(), rule.npcArrowReflectTriggers)) changed = true;
                            }
                            if (rule.npcArrowMode == 2) {
                                std::string arrowCostLabel = std::string(GetLoc("menu.arrow_cost", "Arrow Cost")) + "##Rule";
                                if (ImGui::Combo(arrowCostLabel.c_str(), (int*)&rule.npcArrowCostType, costOpts, 4)) changed = true;
                                if (rule.npcArrowCostType != CostType::None) {
                                    std::string arrowCostValueLabel = std::string(GetLoc("menu.arrow_cost_value", "Arrow Cost Value")) + "##Rule";
                                    std::string arrowCostGlobalLabel = std::string(GetLoc("menu.arrow_cost_global", "Use Global For Arrow Cost")) + "##Rule";
                                    if (DrawCostValueWithSource(arrowCostValueLabel.c_str(), rule.npcArrowCost, rule.npcArrowCostSource, rule.npcArrowCostGlobal, rule.npcArrowCostActorValue)) changed = true;
                                }
                            }
                            std::string staggerShooterLabel = std::string(GetLoc("menu.stagger_shooter", "Stagger Shooter")) + "##Rule";
                            if (ImGui::Combo(staggerShooterLabel.c_str(), &rule.npcArrowStagger, reflectOpts, 3)) changed = true;
                            if (rule.npcArrowStagger > 0) {
                                std::string arrowSizeLabel = std::string(GetLoc("menu.arrow_stagger_size", "Arrow Stagger Size")) + "##Rule";
                                if (DrawStaggerSizeCombo(arrowSizeLabel.c_str(), &rule.npcArrowStaggerSize)) changed = true;
                            }
                            ImGui::Unindent();
                        }
                        ImGui::Unindent();
                    }

                    if (ImGui::CollapsingHeader(GetLoc("menu.magic_header", "Magic"))) {
                        ImGui::Indent();
                        std::string parryMagicLabel = std::string(GetLoc("menu.parry_magic", "Parry Magic")) + "##Rule";
                        if (ImGui::Combo(parryMagicLabel.c_str(), &rule.npcMagicMode, modeOpts, 3)) changed = true;
                        if (rule.npcMagicMode > 0) {
                            ImGui::Indent();
                            std::string reflectMagicLabel = std::string(GetLoc("menu.reflect_magic", "Reflect Magic")) + "##Rule";
                            if (ImGui::Combo(reflectMagicLabel.c_str(), &rule.npcMagicReflectMode, reflectOpts, 3)) changed = true;
                            if (rule.npcMagicReflectMode > 0) {
                                std::string triggerID = "RuleMagic" + std::to_string(i);
                                if (DrawReflectTriggers(triggerID.c_str(), rule.npcMagicReflectTriggers)) changed = true;
                            }
                            if (rule.npcMagicMode == 2) {
                                std::string magicCostLabel = std::string(GetLoc("menu.magic_cost", "Magic Cost")) + "##Rule";
                                if (ImGui::Combo(magicCostLabel.c_str(), (int*)&rule.npcMagicCostType, costOpts, 4)) changed = true;
                                if (rule.npcMagicCostType != CostType::None) {
                                    std::string magicCostValueLabel = std::string(GetLoc("menu.magic_cost_value", "Magic Cost Value")) + "##Rule";
                                    std::string magicCostGlobalLabel = std::string(GetLoc("menu.magic_cost_global", "Use Global For Magic Cost")) + "##Rule";
                                    if (DrawCostValueWithSource(magicCostValueLabel.c_str(), rule.npcMagicCost, rule.npcMagicCostSource, rule.npcMagicCostGlobal, rule.npcMagicCostActorValue)) changed = true;
                                }
                            }
                            std::string staggerCasterLabel = std::string(GetLoc("menu.stagger_caster", "Stagger Caster")) + "##Rule";
                            if (ImGui::Combo(staggerCasterLabel.c_str(), &rule.npcMagicStagger, reflectOpts, 3)) changed = true;
                            if (rule.npcMagicStagger > 0) {
                                std::string magicSizeLabel = std::string(GetLoc("menu.magic_stagger_size", "Magic Stagger Size")) + "##Rule";
                                if (DrawStaggerSizeCombo(magicSizeLabel.c_str(), &rule.npcMagicStaggerSize)) changed = true;
                            }
                            ImGui::Unindent();
                        }
                        ImGui::Unindent();
                    }

                    if (ImGui::CollapsingHeader(GetLoc("menu.melee_header", "Melee"))) {
                        ImGui::Indent();
                        std::string reflectMeleeLabel = std::string(GetLoc("menu.reflect_melee", "Reflect Melee Damage")) + "##Rule";
                        if (ImGui::Combo(reflectMeleeLabel.c_str(), &rule.npcReflectMeleeMode, reflectOpts, 3)) changed = true;
                        if (rule.npcReflectMeleeMode > 0) {
                            ImGui::Indent();
                            std::string triggerID = "RuleMelee" + std::to_string(i);
                            if (DrawReflectTriggers(triggerID.c_str(), rule.npcMeleeReflectTriggers)) changed = true;
                            std::string comboLabel = std::string(GetLoc("menu.reflect_calc_type", "Reflect Calculation Type")) + "##Rule_" + std::to_string(i);
                            if (DrawReflectTypeCombo(comboLabel.c_str(), &rule.npcReflectDamageType)) changed = true;

                            if (rule.npcReflectDamageType == 0) {
                                std::string flatLabel = std::string(GetLoc("menu.flat_damage_val", "Flat Damage Value")) + "##Rule";
                                if (DrawFloatValueSource(flatLabel.c_str(), rule.npcReflectValue, 1.0f, 500.0f, "%.0f", rule.npcReflectValueSource, rule.npcReflectValueGlobal, rule.npcReflectValueActorValue)) changed = true;
                            }
                            else if (rule.npcReflectDamageType == 1) {
                                std::string pctLabel = std::string(GetLoc("menu.pct_modifier", "Percentage Modifier (%)")) + "##Rule";
                                if (DrawFloatValueSource(pctLabel.c_str(), rule.npcReflectValue, 1.0f, 200.0f, "%.0f", rule.npcReflectValueSource, rule.npcReflectValueGlobal, rule.npcReflectValueActorValue)) changed = true;
                            }
                            std::string reflectCostLabel = std::string(GetLoc("menu.reflect_cost_type", "Reflect Cost Type")) + "##Rule";
                            if (ImGui::Combo(reflectCostLabel.c_str(), (int*)&rule.npcReflectMeleeCostType, costOpts, 4)) changed = true;
                            if (rule.npcReflectMeleeCostType != CostType::None) {
                                std::string reflectCostValueLabel = std::string(GetLoc("menu.reflect_cost_value", "Reflect Cost Value")) + "##Rule";
                                std::string reflectCostGlobalLabel = std::string(GetLoc("menu.reflect_cost_global", "Use Global For Reflect Cost")) + "##Rule";
                                if (DrawCostValueWithSource(reflectCostValueLabel.c_str(), rule.npcReflectMeleeCost, rule.npcReflectMeleeCostSource, rule.npcReflectMeleeCostGlobal, rule.npcReflectMeleeCostActorValue)) changed = true;
                            }
                            ImGui::Unindent();
                        }

                        std::string staggerAttackerLabel = std::string(GetLoc("menu.stagger_attacker", "Stagger Attacker")) + "##Rule";
                        if (ImGui::Combo(staggerAttackerLabel.c_str(), &rule.npcMeleeStagger, reflectOpts, 3)) changed = true;
                        if (rule.npcMeleeStagger > 0) {
                            std::string meleeSizeLabel = std::string(GetLoc("menu.melee_stagger_size", "Melee Stagger Size")) + "##Rule";
                            if (DrawStaggerSizeCombo(meleeSizeLabel.c_str(), &rule.npcMeleeStaggerSize)) changed = true;
                        }
                        ImGui::Unindent();
                    }

                    if (ImGui::CollapsingHeader(GetLoc("menu.effects_header", "Effects"))) {
                        ImGui::Indent();
                        std::string visualEffectsLabel = std::string(GetLoc("menu.visual_effects", "Visual Effects")) + "##Rule";
                        if (ImGui::Combo(visualEffectsLabel.c_str(), &rule.npcVisualMode, reflectOpts, 3)) changed = true;
                        std::string soundEffectsLabel = std::string(GetLoc("menu.sound_effects", "Sound Effects")) + "##Rule";
                        if (ImGui::Combo(soundEffectsLabel.c_str(), &rule.npcSoundMode, reflectOpts, 3)) changed = true;
                        ImGui::Unindent();
                    }
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                if (ImGui::CollapsingHeader(GetLoc("menu.perk_locks_header", "Perk Locks"))) {
                    ImGui::Indent();
                    if (DrawConditionalPerkDropdown(GetLoc("menu.restore_normal_perk", "Lock Normal Restore Behind Perk"), rule.npcNormalRestore.perk, rule.npcParryEnabled && rule.npcNormalRestore.resource != CostType::None, GetLoc("menu.requires_normal_restore", "Enable Parry and Normal Parry Restore"))) changed = true;
                    if (DrawConditionalPerkDropdown(GetLoc("menu.restore_perfect_perk", "Lock Perfect Restore Behind Perk"), rule.npcPerfectRestore.perk, rule.npcParryEnabled && rule.npcPerfectRestore.resource != CostType::None, GetLoc("menu.requires_perfect_restore", "Enable Parry and Perfect Parry Restore"))) changed = true;
                    ImGui::Unindent();
                }

                ImGui::Spacing();
                if (ImGui::Button(GetLoc("menu.remove_rule", "Remove Rule"), { 150, 0 })) {
                    const auto path = GetRulePath(RULES_DIR, rule.ruleName);
                    std::error_code ec;
                    std::filesystem::remove(path, ec);
                    if (ec) {
                        SKSE::log::warn("[Settings] Failed to remove rule '{}': {}", path.string(), ec.message());
                    }
                    npcRules.erase(npcRules.begin() + i);
                    changed = true;
                    ImGui::PopID();
                    continue;
                }
                ImGui::Unindent();
            }
            ImGui::PopID();
            i++;
        }

        if (changed) Save();
    }

    void PlayerMenu()
    {
        bool changed = false;

        const char* modeOpts[] = { GetLoc("menu.opt_disabled", "Disabled"), GetLoc("menu.opt_enabled_free", "Enabled (Free)"), GetLoc("menu.opt_enabled_cost", "Enabled (Cost)") };
        const char* reflectOpts[] = { GetLoc("menu.opt_disable", "Disable"), GetLoc("menu.opt_enable", "Enable"), GetLoc("menu.opt_enable_perfect", "Enable only in perfect") };
        const char* costOpts[] = { GetLoc("menu.opt_none", "None"), GetLoc("menu.opt_stamina", "Stamina"), GetLoc("menu.opt_magicka", "Magicka"), GetLoc("menu.opt_health", "Health") };

        if (ImGuiMCP::CollapsingHeader(GetLoc("menu.player_settings_header", "Player Settings"), ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGuiMCP::Checkbox(GetLoc("menu.player_enable_parry", "Enable Player Parry"), &playerParryEnabled)) changed = true;

            if (playerParryEnabled) {
                ImGuiMCP::Indent();
                if (ImGuiMCP::CollapsingHeader(GetLoc("menu.timing_header", "Timing"))) {
                    ImGuiMCP::Indent();
                    if (DrawIntValueSource(GetLoc("menu.player_normal_window", "Normal Window (ms)"), playerNormalParryMS, 50, 2000, playerNormalParrySource, playerNormalParryGlobal, playerNormalParryActorValue, false)) changed = true;
                    if (DrawIntValueSource(GetLoc("menu.player_perfect_window", "Perfect Window (ms)"), playerPerfectParryMS, 10, 500, playerPerfectParrySource, playerPerfectParryGlobal, playerPerfectParryActorValue, false)) changed = true;
                    ImGuiMCP::Unindent();
                }

                ImGuiMCP::Spacing();
                if (ImGuiMCP::CollapsingHeader(GetLoc("menu.restore_header", "Restore on Parry"))) {
                    ImGuiMCP::Indent();
                    if (DrawRestoreConfig(GetLoc("menu.restore_normal", "Normal Parry"), "PlayerNormalRestore", playerNormalRestore)) changed = true;
                    if (DrawRestoreConfig(GetLoc("menu.restore_perfect", "Perfect Parry"), "PlayerPerfectRestore", playerPerfectRestore)) changed = true;
                    ImGuiMCP::Unindent();
                }

                ImGuiMCP::Spacing();
                if (ImGuiMCP::CollapsingHeader(GetLoc("menu.commitment_header", "Commitment"))) {
                    ImGuiMCP::Indent();
                    if (ImGuiMCP::Checkbox(GetLoc("menu.player_enable_commitment", "Enable Parry Commitment"), &playerParryCommitmentEnabled)) changed = true;
                    if (playerParryCommitmentEnabled) {
                        ImGuiMCP::Indent();
                        if (DrawIntValueSource(GetLoc("menu.player_commitment_time", "Commitment Time (ms)"), playerParryCommitmentMS, 100, 2000, playerParryCommitmentSource, playerParryCommitmentGlobal, playerParryCommitmentActorValue)) changed = true;
                        if (DrawIntValueSource(GetLoc("menu.player_reduc_normal", "Reduction on Normal Parry (ms)"), playerCommitmentReductionNormalMS, 0, 2000, playerCommitmentReductionNormalSource, playerCommitmentReductionNormalGlobal, playerCommitmentReductionNormalActorValue)) changed = true;
                        if (DrawIntValueSource(GetLoc("menu.player_reduc_perfect", "Reduction on Perfect Parry (ms)"), playerCommitmentReductionPerfectMS, 0, 2000, playerCommitmentReductionPerfectSource, playerCommitmentReductionPerfectGlobal, playerCommitmentReductionPerfectActorValue)) changed = true;
                        ImGuiMCP::Unindent();
                    }
                    ImGuiMCP::Unindent();
                }

                ImGuiMCP::Spacing();
                if (ImGuiMCP::CollapsingHeader(GetLoc("menu.effects_header", "Effects"))) {
                    ImGuiMCP::Indent();
                    if (ImGuiMCP::Combo(GetLoc("menu.player_visual_effects", "Visual Effects"), &playerVisualMode, reflectOpts, 3)) changed = true;
                    if (ImGuiMCP::Combo(GetLoc("menu.player_sound_effects", "Sound Effects"), &playerSoundMode, reflectOpts, 3)) changed = true;
                    ImGuiMCP::Unindent();
                }
                ImGuiMCP::Spacing();
                if (ImGuiMCP::CollapsingHeader(GetLoc("menu.melee_header", "Melee"))) {
                    ImGuiMCP::Indent();
                    if (ImGuiMCP::Combo(GetLoc("menu.player_stagger_melee", "Stagger Attacker on Melee Parry"), &playerMeleeStagger, reflectOpts, 3)) changed = true;
                    if (playerMeleeStagger > 0) {
                        ImGuiMCP::Indent();
                        if (DrawStaggerSizeCombo(GetLoc("menu.player_melee_stagger_size", "Melee Stagger Size"), &playerMeleeStaggerSize)) changed = true;
                        ImGuiMCP::Unindent();
                    }
                    if (ImGuiMCP::Combo(GetLoc("menu.player_reflect_melee", "Reflect Melee Damage"), &playerReflectMeleeMode, reflectOpts, 3)) changed = true;
                    if (playerReflectMeleeMode > 0) {
                        ImGuiMCP::Indent();
                        if (DrawReflectTriggers("PlayerMelee", playerMeleeReflectTriggers)) changed = true;
                        std::string comboLabel = std::string(GetLoc("menu.reflect_calc_type", "Reflect Calculation Type")) + "##Player";
                        if (DrawReflectTypeCombo(comboLabel.c_str(), &playerReflectDamageType)) changed = true;
                        if (playerReflectDamageType == 0) {
                            std::string flatLabel = std::string(GetLoc("menu.flat_damage_val", "Flat Damage Value")) + "##Player";
                            if (DrawFloatValueSource(flatLabel.c_str(), playerReflectValue, 1.0f, 500.0f, "%.0f", playerReflectValueSource, playerReflectValueGlobal, playerReflectValueActorValue)) changed = true;
                        }
                        else if (playerReflectDamageType == 1) {
                            std::string pctLabel = std::string(GetLoc("menu.pct_modifier", "Percentage Modifier (%)")) + "##Player";
                            if (DrawFloatValueSource(pctLabel.c_str(), playerReflectValue, 1.0f, 200.0f, "%.0f", playerReflectValueSource, playerReflectValueGlobal, playerReflectValueActorValue)) changed = true;
                        }
                        if (ImGuiMCP::Combo(GetLoc("menu.player_reflect_cost_type", "Reflect Cost Type"), (int*)&playerReflectMeleeCostType, costOpts, 4)) changed = true;
                        if (playerReflectMeleeCostType != CostType::None && DrawCostValueWithSource(GetLoc("menu.player_reflect_cost_value", "Reflect Cost Value"), playerReflectMeleeCost, playerReflectMeleeCostSource, playerReflectMeleeCostGlobal, playerReflectMeleeCostActorValue)) changed = true;
                        ImGuiMCP::Unindent();
                    }
                    ImGuiMCP::Unindent();
                }

                // Arrow
                ImGuiMCP::Spacing();
                if (ImGuiMCP::CollapsingHeader(GetLoc("menu.projectiles_header", "Projectiles"))) {
                    ImGuiMCP::Indent();
                    if (ImGuiMCP::Combo(GetLoc("menu.player_parry_arrows", "Parry Arrows"), &playerArrowMode, modeOpts, 3)) changed = true;
                    if (playerArrowMode > 0) {
                        ImGuiMCP::Indent();
                        if (ImGuiMCP::Combo(GetLoc("menu.player_reflect_arrows", "Reflect Arrows"), &playerArrowReflectMode, reflectOpts, 3)) changed = true;
                        if (playerArrowReflectMode > 0) {
                            if (DrawReflectTriggers("PlayerArrow", playerArrowReflectTriggers)) changed = true;
                        }
                        if (playerArrowMode == 2) {
                            if (ImGuiMCP::Combo(GetLoc("menu.player_arrow_cost", "Arrow Cost"), (int*)&playerArrowCostType, costOpts, 4)) changed = true;
                            if (playerArrowCostType != CostType::None && DrawCostValueWithSource(GetLoc("menu.player_arrow_cost_value", "Arrow Cost Value"), playerArrowCost, playerArrowCostSource, playerArrowCostGlobal, playerArrowCostActorValue)) changed = true;
                        }
                        if (ImGuiMCP::Combo(GetLoc("menu.player_stagger_shooter", "Stagger Shooter"), &playerArrowStagger, reflectOpts, 3)) changed = true;
                        if (playerArrowStagger > 0) {
                            if (DrawStaggerSizeCombo(GetLoc("menu.player_arrow_stagger_size", "Arrow Stagger Size"), &playerArrowStaggerSize)) changed = true;
                        }
                        ImGuiMCP::Unindent();
                    }
                    ImGuiMCP::Unindent();
                }

                // Magic
                ImGuiMCP::Spacing();
                if (ImGuiMCP::CollapsingHeader(GetLoc("menu.magic_header", "Magic"))) {
                    ImGuiMCP::Indent();
                    if (ImGuiMCP::Combo(GetLoc("menu.player_parry_magic", "Parry Magic"), &playerMagicMode, modeOpts, 3)) changed = true;
                    if (playerMagicMode > 0) {
                        ImGuiMCP::Indent();
                        if (ImGuiMCP::Combo(GetLoc("menu.player_reflect_magic", "Reflect Magic"), &playerMagicReflectMode, reflectOpts, 3)) changed = true;
                        if (playerMagicReflectMode > 0) {
                            if (DrawReflectTriggers("PlayerMagic", playerMagicReflectTriggers)) changed = true;
                        }
                        if (playerMagicMode == 2) {
                            if (ImGuiMCP::Combo(GetLoc("menu.player_magic_cost", "Magic Cost"), (int*)&playerMagicCostType, costOpts, 4)) changed = true;
                            if (playerMagicCostType != CostType::None && DrawCostValueWithSource(GetLoc("menu.player_magic_cost_value", "Magic Cost Value"), playerMagicCost, playerMagicCostSource, playerMagicCostGlobal, playerMagicCostActorValue)) changed = true;
                        }
                        if (ImGuiMCP::Combo(GetLoc("menu.player_stagger_caster", "Stagger Caster"), &playerMagicStagger, reflectOpts, 3)) changed = true;
                        if (playerMagicStagger > 0) {
                            if (DrawStaggerSizeCombo(GetLoc("menu.player_magic_stagger_size", "Magic Stagger Size"), &playerMagicStaggerSize)) changed = true;
                        }
                        ImGuiMCP::Unindent();
                    }
                    ImGuiMCP::Unindent();
                }

                ImGuiMCP::Unindent();
            }

            ImGuiMCP::Spacing();
            ImGuiMCP::Separator();
            ImGuiMCP::Spacing();
            if (ImGuiMCP::CollapsingHeader(GetLoc("menu.perk_locks_header", "Perk Locks"))) {
                ImGuiMCP::Indent();
                const char* playerParryRequirement = GetLoc("menu.requires_player_parry", "Enable Player Parry");
                if (DrawConditionalPerkDropdown(GetLoc("menu.player_parry_perk", "Lock Parry Behind Perk"), playerParryPerk, playerParryEnabled, playerParryRequirement)) changed = true;
                if (DrawConditionalPerkDropdown(GetLoc("menu.player_disable_parry_perk", "Disable Parry If Player Has Perk"), playerDisableParryPerk, playerParryEnabled, playerParryRequirement)) changed = true;
                if (DrawConditionalPerkDropdown(GetLoc("menu.restore_normal_perk", "Lock Normal Restore Behind Perk"), playerNormalRestore.perk, playerParryEnabled && playerNormalRestore.resource != CostType::None, GetLoc("menu.requires_normal_restore", "Enable Parry and Normal Parry Restore"))) changed = true;
                if (DrawConditionalPerkDropdown(GetLoc("menu.restore_perfect_perk", "Lock Perfect Restore Behind Perk"), playerPerfectRestore.perk, playerParryEnabled && playerPerfectRestore.resource != CostType::None, GetLoc("menu.requires_perfect_restore", "Enable Parry and Perfect Parry Restore"))) changed = true;
                if (DrawConditionalPerkDropdown(GetLoc("menu.player_melee_stagger_perk", "Lock Melee Stagger Behind Perk"), playerMeleeStaggerPerk, playerParryEnabled && playerMeleeStagger > 0, GetLoc("menu.requires_melee_stagger", "Enable Parry and Melee Stagger"))) changed = true;
                if (DrawConditionalPerkDropdown(GetLoc("menu.player_reflect_melee_perk", "Lock Melee Reflect Behind Perk"), playerReflectMeleePerk, playerParryEnabled && playerReflectMeleeMode > 0, GetLoc("menu.requires_melee_reflect", "Enable Parry and Melee Reflect"))) changed = true;
                if (DrawConditionalPerkDropdown(GetLoc("menu.player_arrow_parry_perk", "Lock Arrow Parry Behind Perk"), playerArrowParryPerk, playerParryEnabled && playerArrowMode > 0, GetLoc("menu.requires_arrow_parry", "Enable Parry and Arrow Parry"))) changed = true;
                if (DrawConditionalPerkDropdown(GetLoc("menu.player_arrow_reflect_perk", "Lock Arrow Reflect Behind Perk"), playerArrowReflectPerk, playerParryEnabled && playerArrowMode > 0 && playerArrowReflectMode > 0, GetLoc("menu.requires_arrow_reflect", "Enable Parry, Arrow Parry, and Arrow Reflect"))) changed = true;
                if (DrawConditionalPerkDropdown(GetLoc("menu.player_arrow_stagger_perk", "Lock Arrow Stagger Behind Perk"), playerArrowStaggerPerk, playerParryEnabled && playerArrowMode > 0 && playerArrowStagger > 0, GetLoc("menu.requires_arrow_stagger", "Enable Parry, Arrow Parry, and Arrow Stagger"))) changed = true;
                if (DrawConditionalPerkDropdown(GetLoc("menu.player_magic_parry_perk", "Lock Magic Parry Behind Perk"), playerMagicParryPerk, playerParryEnabled && playerMagicMode > 0, GetLoc("menu.requires_magic_parry", "Enable Parry and Magic Parry"))) changed = true;
                if (DrawConditionalPerkDropdown(GetLoc("menu.player_magic_reflect_perk", "Lock Magic Reflect Behind Perk"), playerMagicReflectPerk, playerParryEnabled && playerMagicMode > 0 && playerMagicReflectMode > 0, GetLoc("menu.requires_magic_reflect", "Enable Parry, Magic Parry, and Magic Reflect"))) changed = true;
                if (DrawConditionalPerkDropdown(GetLoc("menu.player_magic_stagger_perk", "Lock Magic Stagger Behind Perk"), playerMagicStaggerPerk, playerParryEnabled && playerMagicMode > 0 && playerMagicStagger > 0, GetLoc("menu.requires_magic_stagger", "Enable Parry, Magic Parry, and Magic Stagger"))) changed = true;
                ImGuiMCP::Unindent();
            }
        }

        ImGuiMCP::Spacing();
        if (ImGuiMCP::CollapsingHeader(GetLoc("menu.slow_motion_header", "Slow Motion Parry"))) {
            if (DrawFloatValueSource(GetLoc("menu.slow_time_multiplier", "Time Multiplier"), slowTimeMultiplier, 0.02f, 1.0f, "%.2f", slowTimeMultiplierSource, slowTimeMultiplierGlobal, slowTimeMultiplierActorValue)) changed = true;
            if (DrawIntValueSource(GetLoc("menu.slow_time_duration", "Duration (ms)"), slowTimeDurationMS, 100, 5000, slowTimeDurationSource, slowTimeDurationGlobal, slowTimeDurationActorValue)) changed = true;
        }

        if (changed) Save();
    }

    void GeneralMenu()
    {
        bool changed = false;

        const char* feedbackOptions[] = {
            GetLoc("menu.opt_feedback_none", "None"),
            GetLoc("menu.opt_feedback_recoil", "Recoil"),
            GetLoc("menu.opt_feedback_hit_block", "Hit Block")
        };
        if (ImGuiMCP::Combo(GetLoc("menu.parry_feedback", "Parry Feedback"), &parryFeedback, feedbackOptions, 3)) changed = true;

        const char* w2wModeOptions[] = {
            GetLoc("menu.opt_disabled", "Disabled"),
            GetLoc("menu.opt_w2w_npcs", "Enabled (NPCs Only)"),
            GetLoc("menu.opt_w2w_all", "Enabled (All)")
        };
        const char* w2wTargetOptions[] = {
            GetLoc("menu.opt_disabled", "Disabled"),
            GetLoc("menu.opt_w2w_attacker", "Attacker Only"),
            GetLoc("menu.opt_w2w_defender", "Defender Only"),
            GetLoc("menu.opt_w2w_both", "Both")
        };
        const char* w2wReactionOptions[] = {
            GetLoc("menu.opt_w2w_recoil", "Recoil"),
            GetLoc("menu.opt_w2w_stagger", "Stagger")
        };

        if (ImGuiMCP::CollapsingHeader(GetLoc("menu.parry_window_behavior_header", "Parry Window Behavior"), ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGuiMCP::Checkbox(GetLoc("menu.bash_opens_parry_window", "Bash Opens the Parry Window Instead of Block"), &bashOpensParryWindow)) changed = true;
            if (ImGuiMCP::Checkbox(GetLoc("menu.cancel_parry_on_block_stop", "Close the Parry Window When Leaving Block"), &cancelParryOnBlockStop)) changed = true;
            if (ImGuiMCP::Checkbox(GetLoc("menu.respect_block_angle", "Parry Only Within the Block Angle"), &respectBlockAngle)) changed = true;
            if (respectBlockAngle) {
                ImGuiMCP::Indent();
                if (RenderFloatSliderWithInput(GetLoc("menu.block_angle", "Block Angle (degrees each side, 0 = game setting)"), blockAngle, 0.0f, 180.0f, "%.0f")) changed = true;
                ImGuiMCP::Unindent();
            }
        }

        ImGuiMCP::Spacing();

        if (ImGuiMCP::CollapsingHeader(GetLoc("menu.w2w_settings_header", "Weapon to Weapon (W2W) Settings"), ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGuiMCP::Combo(GetLoc("menu.w2w_recoil_mode", "W2W Recoil Mode"), &w2wMode, w2wModeOptions, 3)) changed = true;

            if (w2wMode > 0) {
                ImGuiMCP::Indent();
                if (ImGuiMCP::Combo(GetLoc("menu.w2w_reaction_target", "Reaction Target"), &w2wReactionTarget, w2wTargetOptions, 4)) changed = true;
                if (w2wReactionTarget > 0) {
                    ImGuiMCP::Indent();
                    if (ImGuiMCP::Combo(GetLoc("menu.w2w_reaction_type", "Reaction Type"), &w2wReactionType, w2wReactionOptions, 2)) changed = true;
                    ImGuiMCP::Unindent();
                }

                ImGuiMCP::Separator();

                // --- Base Chance ---
                std::string baseLabel = std::string(GetLoc("menu.w2w_base_value", "Base Value")) + "##W2W";
                if (RenderFloatSliderWithInput(baseLabel.c_str(), w2wBaseChance, 0.0f, 500.0f, "%.1f")) changed = true;

                // --- Damage Mult ---
                std::string dmgLabel = std::string(GetLoc("menu.w2w_damage_mult", "Damage Mult")) + "##W2W";
                if (RenderFloatSliderWithInput(dmgLabel.c_str(), w2wDamageMult, 0.0f, 100.0f, "%.2f")) changed = true;

                // --- Skill Mult ---
                std::string skillLabel = std::string(GetLoc("menu.w2w_skill_mult", "Skill Mult")) + "##W2W";
                if (RenderFloatSliderWithInput(skillLabel.c_str(), w2wSkillMult, 0.0f, 100.0f, "%.2f")) changed = true;

                // --- Global Difficulty ---
                std::string diffLabel = std::string(GetLoc("menu.w2w_global_difficulty", "Global Difficulty")) + "##W2W";
                if (RenderFloatSliderWithInput(diffLabel.c_str(), w2wGlobalDifficulty, 1.0f, 5000.0f, "%.1f")) changed = true;

                ImGuiMCP::Separator();

                // --- Seção de Simulação e Lógica W2W ---
                ImGuiMCP::TextColored({ 1.0f, 0.8f, 0.0f, 1.0f }, "%s", GetLoc("menu.w2w_power_logic", "W2W Power Logic:"));
                ImGuiMCP::Spacing();

                float exDmgLow = 15.0f, exSkillLow = 20.0f;
                float exDmgHigh = 45.0f, exSkillHigh = 75.0f;

                float powerLow = w2wBaseChance + (exDmgLow * w2wDamageMult) + (exSkillLow * w2wSkillMult);
                float powerHigh = w2wBaseChance + (exDmgHigh * w2wDamageMult) + (exSkillHigh * w2wSkillMult);

                float chanceLow = (powerLow / (powerLow + w2wGlobalDifficulty)) * 100.0f;
                float chanceHigh = (powerHigh / (powerHigh + w2wGlobalDifficulty)) * 100.0f;

                ImGuiMCP::BulletText(GetLoc("menu.w2w_bandit_bullet", "Basic Bandit - Dmg: %.0f, Skill: %.0f -> Power: %.1f (Win Chance: %.2f%%)"), exDmgLow, exSkillLow, powerLow, chanceLow);
                ImGuiMCP::BulletText(GetLoc("menu.w2w_boss_bullet", "Elite Boss   - Dmg: %.0f, Skill: %.0f -> Power: %.1f (Win Chance: %.2f%%)"), exDmgHigh, exSkillHigh, powerHigh, chanceHigh);

                ImGuiMCP::Unindent();
            }
        }

        if (changed) Save();
    }

}


void ParrySettings::MmRegister() {
    if (SKSEMenuFramework::IsInstalled()) {
        LoadLanguage();
        Load();
        SKSEMenuFramework::SetSection(GetLoc("menu.section", "Parry for all"));
        SKSEMenuFramework::AddSectionItem(GetLoc("menu.player_settings", "Player Settings"), PlayerMenu);
        SKSEMenuFramework::AddSectionItem(GetLoc("menu.npc_settings", "NPC Settings"), NPCMenu);
        SKSEMenuFramework::AddSectionItem(GetLoc("menu.rules_settings", "Rules Settings"), RulesMenu);
        SKSEMenuFramework::AddSectionItem(GetLoc("menu.general_settings", "General Settings"), GeneralMenu);
    }
}

