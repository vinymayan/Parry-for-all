#include "Settings.h"
#include "Manager.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>

namespace ImGui = ImGuiMCP;

namespace ParrySettings {
    const std::string RULES_DIR = "Data/SKSE/Plugins/ParryAll/Rules/";
    const char* NEW_SETTINGS_PATH = "Data/SKSE/Plugins/ParryAll/ParryAll_Settings.json";
    const char* OLD_SETTINGS_PATH = "Data/SKSE/Plugins/ParryAll.json";
    const char* LANG_PATH = "Data/SKSE/Plugins/ParryAll/Language.json";
    static std::unordered_map<std::string, std::string> LangMap;

    void LoadLanguage() {
        LangMap.clear();
        std::ifstream file(LANG_PATH, std::ios::binary);
        if (!file.is_open()) {
            SKSE::log::warn("Não foi possível carregar DodgeMod_Language.json. Usando textos padrões.");
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

    inline int GetIndexFromID(int id, const int* idArray, int arraySize) {
        for (int i = 0; i < arraySize; i++) {
            if (idArray[i] == id) return i;
        }
        return 0;
    }

    // Dropdowns UI
    const char* ModeOptions = "Disabled\0Enabled (Free)\0Enabled (Cost)\0\0";
    const char* ReflectOptions = "Disable\0Enable\0Enable only in perfect\0\0";
    const char* CostOptionsUI = "None\0Stamina\0Magicka\0\0";

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

    void SaveRule(const NPCParryRule& rule) {
        if (!std::filesystem::exists(RULES_DIR)) { std::filesystem::create_directories(RULES_DIR); }
        std::string filepath = RULES_DIR + rule.ruleName + ".json";
        rapidjson::Document doc; doc.SetObject(); auto& alloc = doc.GetAllocator();

        rapidjson::Value rName; rName.SetString(rule.ruleName.c_str(), alloc);
        doc.AddMember("ruleName", rName, alloc);
        auto perkForm = RE::TESForm::LookupByID(rule.perkID);
        std::string perkStr = FormUtil::NormalizeFormID(perkForm);
        rapidjson::Value pStr; pStr.SetString(perkStr.c_str(), alloc);
        doc.AddMember("perk", pStr, alloc);

        doc.AddMember("npcParryEnabled", rule.npcParryEnabled, alloc);
        doc.AddMember("npcNormalParryMS", rule.npcNormalParryMS, alloc);
        doc.AddMember("npcPerfectParryMS", rule.npcPerfectParryMS, alloc);
        doc.AddMember("npcArrowMode", rule.npcArrowMode, alloc);
        doc.AddMember("npcArrowReflectMode", rule.npcArrowReflectMode, alloc);
        doc.AddMember("npcArrowCostType", static_cast<int>(rule.npcArrowCostType), alloc);
        doc.AddMember("npcMagicMode", rule.npcMagicMode, alloc);
        doc.AddMember("npcMagicReflectMode", rule.npcMagicReflectMode, alloc);
        doc.AddMember("npcMagicCostType", static_cast<int>(rule.npcMagicCostType), alloc);
        doc.AddMember("npcReflectMeleeMode", rule.npcReflectMeleeMode, alloc);
        doc.AddMember("npcReflectMeleeCostType", static_cast<int>(rule.npcReflectMeleeCostType), alloc);

        // Novos campos salvos nas sub-regras
        doc.AddMember("npcReflectDamageType", rule.npcReflectDamageType, alloc);
        doc.AddMember("npcReflectValue", rule.npcReflectValue, alloc);

        doc.AddMember("npcMeleeStagger", rule.npcMeleeStagger, alloc);
        doc.AddMember("npcArrowStagger", rule.npcArrowStagger, alloc);
        doc.AddMember("npcMagicStagger", rule.npcMagicStagger, alloc);
        doc.AddMember("npcVisualMode", rule.npcVisualMode, alloc);
        doc.AddMember("npcSoundMode", rule.npcSoundMode, alloc);

        std::ofstream file(filepath);
        if (file.is_open()) {
            rapidjson::StringBuffer buffer; rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            doc.Accept(writer); file << buffer.GetString();
        }
    }

    void LoadRules() {
        npcRules.clear(); if (!std::filesystem::exists(RULES_DIR)) return;
        for (const auto& entry : std::filesystem::directory_iterator(RULES_DIR)) {
            if (entry.path().extension() == ".json") {
                std::ifstream file(entry.path()); if (!file.is_open()) continue;
                std::stringstream buffer; buffer << file.rdbuf();
                rapidjson::Document doc; doc.Parse(buffer.str().c_str());
                if (doc.HasParseError() || !doc.IsObject()) continue;

                NPCParryRule rule;
                if (doc.HasMember("ruleName")) rule.ruleName = doc["ruleName"].GetString();
                if (doc.HasMember("perk")) { rule.perkID = FormUtil::FormIDFromString(doc["perk"].GetString()); }

                if (doc.HasMember("npcParryEnabled")) rule.npcParryEnabled = doc["npcParryEnabled"].GetBool();
                if (doc.HasMember("npcNormalParryMS")) rule.npcNormalParryMS = doc["npcNormalParryMS"].GetInt();
                if (doc.HasMember("npcPerfectParryMS")) rule.npcPerfectParryMS = doc["npcPerfectParryMS"].GetInt();
                if (doc.HasMember("npcArrowMode")) rule.npcArrowMode = doc["npcArrowMode"].GetInt();
                if (doc.HasMember("npcArrowReflectMode")) rule.npcArrowReflectMode = doc["npcArrowReflectMode"].GetInt();
                if (doc.HasMember("npcArrowCostType")) rule.npcArrowCostType = static_cast<CostType>(doc["npcArrowCostType"].GetInt());
                if (doc.HasMember("npcMagicMode")) rule.npcMagicMode = doc["npcMagicMode"].GetInt();
                if (doc.HasMember("npcMagicReflectMode")) rule.npcMagicReflectMode = doc["npcMagicReflectMode"].GetInt();
                if (doc.HasMember("npcMagicCostType")) rule.npcMagicCostType = static_cast<CostType>(doc["npcMagicCostType"].GetInt());
                if (doc.HasMember("npcReflectMeleeMode")) rule.npcReflectMeleeMode = doc["npcReflectMeleeMode"].GetInt();
                if (doc.HasMember("npcReflectMeleeCostType")) rule.npcReflectMeleeCostType = static_cast<CostType>(doc["npcReflectMeleeCostType"].GetInt());

                // Leitura dos novos campos nas regras por Perk (com fallback caso o JSON da regra seja antigo)
                if (doc.HasMember("npcReflectDamageType")) {
                    rule.npcReflectDamageType = doc["npcReflectDamageType"].GetInt();
                }
                else if (doc.HasMember("npcReflectSkillScaling")) {
                    rule.npcReflectDamageType = doc["npcReflectSkillScaling"].GetBool() ? 3 : 2;
                }
                if (doc.HasMember("npcReflectValue")) rule.npcReflectValue = doc["npcReflectValue"].GetFloat();

                if (doc.HasMember("npcMeleeStagger")) rule.npcMeleeStagger = doc["npcMeleeStagger"].GetInt();
                if (doc.HasMember("npcArrowStagger")) rule.npcArrowStagger = doc["npcArrowStagger"].GetInt();
                if (doc.HasMember("npcMagicStagger")) rule.npcMagicStagger = doc["npcMagicStagger"].GetInt();
                if (doc.HasMember("npcVisualMode")) rule.npcVisualMode = doc["npcVisualMode"].GetInt();
                if (doc.HasMember("npcSoundMode")) rule.npcSoundMode = doc["npcSoundMode"].GetInt();
                npcRules.push_back(rule);
            }
        }
    }

    void Save() {
        std::filesystem::path path(NEW_SETTINGS_PATH);
        if (!std::filesystem::exists(path.parent_path())) { std::filesystem::create_directories(path.parent_path()); }
        rapidjson::Document doc; doc.SetObject(); auto& allocator = doc.GetAllocator();

        // Player
        doc.AddMember("playerNormalParryMS", playerNormalParryMS, allocator);
        doc.AddMember("playerPerfectParryMS", playerPerfectParryMS, allocator);
        doc.AddMember("playerParryEnabled", playerParryEnabled, allocator);
        doc.AddMember("playerArrowMode", playerArrowMode, allocator);
        doc.AddMember("playerArrowReflectMode", playerArrowReflectMode, allocator);
        doc.AddMember("playerArrowCostType", static_cast<int>(playerArrowCostType), allocator);
        doc.AddMember("playerMagicMode", playerMagicMode, allocator);
        doc.AddMember("playerMagicReflectMode", playerMagicReflectMode, allocator);
        doc.AddMember("playerMagicCostType", static_cast<int>(playerMagicCostType), allocator);
        doc.AddMember("playerReflectMeleeMode", playerReflectMeleeMode, allocator);
        doc.AddMember("playerReflectMeleeCostType", static_cast<int>(playerReflectMeleeCostType), allocator);

        // Novos campos Player no JSON
        doc.AddMember("playerReflectDamageType", playerReflectDamageType, allocator);
        doc.AddMember("playerReflectValue", playerReflectValue, allocator);

        doc.AddMember("playerMeleeStagger", playerMeleeStagger, allocator);
        doc.AddMember("playerArrowStagger", playerArrowStagger, allocator);
        doc.AddMember("playerMagicStagger", playerMagicStagger, allocator);
        doc.AddMember("playerVisualMode", playerVisualMode, allocator);
        doc.AddMember("playerSoundMode", playerSoundMode, allocator);

        // NPC Globais
        doc.AddMember("npcParryEnabled", npcParryEnabled, allocator);
        doc.AddMember("npcNormalParryMS", npcNormalParryMS, allocator);
        doc.AddMember("npcPerfectParryMS", npcPerfectParryMS, allocator);
        doc.AddMember("npcArrowMode", npcArrowMode, allocator);
        doc.AddMember("npcArrowReflectMode", npcArrowReflectMode, allocator);
        doc.AddMember("npcArrowCostType", static_cast<int>(npcArrowCostType), allocator);
        doc.AddMember("npcMagicMode", npcMagicMode, allocator);
        doc.AddMember("npcMagicReflectMode", npcMagicReflectMode, allocator);
        doc.AddMember("npcMagicCostType", static_cast<int>(npcMagicCostType), allocator);
        doc.AddMember("npcReflectMeleeMode", npcReflectMeleeMode, allocator);
        doc.AddMember("npcReflectMeleeCostType", static_cast<int>(npcReflectMeleeCostType), allocator);

        // Novos campos NPC Global no JSON
        doc.AddMember("npcReflectDamageType", npcReflectDamageType, allocator);
        doc.AddMember("npcReflectValue", npcReflectValue, allocator);

        doc.AddMember("npcMeleeStagger", npcMeleeStagger, allocator);
        doc.AddMember("npcArrowStagger", npcArrowStagger, allocator);
        doc.AddMember("npcMagicStagger", npcMagicStagger, allocator);
        doc.AddMember("npcVisualMode", npcVisualMode, allocator);
        doc.AddMember("npcSoundMode", npcSoundMode, allocator);

        doc.AddMember("npcParrySlowTime", npcParrySlowTime, allocator);
        doc.AddMember("npcParryPlayerEnabled", npcParryPlayerEnabled, allocator);
        doc.AddMember("slowTimeMultiplier", slowTimeMultiplier, allocator);
        doc.AddMember("slowTimeDurationMS", slowTimeDurationMS, allocator);
        doc.AddMember("playerParryCommitmentEnabled", playerParryCommitmentEnabled, allocator);
        doc.AddMember("playerParryCommitmentMS", playerParryCommitmentMS, allocator);
        doc.AddMember("playerCommitmentReductionNormalMS", playerCommitmentReductionNormalMS, allocator);
        doc.AddMember("playerCommitmentReductionPerfectMS", playerCommitmentReductionPerfectMS, allocator);

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
        for (const auto& rule : npcRules) { SaveRule(rule); }
    }

    void Load() {
        FILE* fp = nullptr; fopen_s(&fp, NEW_SETTINGS_PATH, "rb");
        if (!fp) { fopen_s(&fp, OLD_SETTINGS_PATH, "rb"); }

        if (fp) {
            char readBuffer[65536]; rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
            rapidjson::Document doc; doc.ParseStream(is); fclose(fp);

            if (doc.IsObject()) {
                if (doc.HasMember("playerNormalParryMS")) playerNormalParryMS = doc["playerNormalParryMS"].GetInt();
                if (doc.HasMember("playerPerfectParryMS")) playerPerfectParryMS = doc["playerPerfectParryMS"].GetInt();
                if (doc.HasMember("playerParryEnabled")) playerParryEnabled = doc["playerParryEnabled"].GetBool();
                if (doc.HasMember("playerArrowMode")) playerArrowMode = doc["playerArrowMode"].GetInt();
                if (doc.HasMember("playerArrowReflectMode")) playerArrowReflectMode = doc["playerArrowReflectMode"].GetInt();
                if (doc.HasMember("playerArrowCostType")) playerArrowCostType = static_cast<CostType>(doc["playerArrowCostType"].GetInt());
                if (doc.HasMember("playerMagicMode")) playerMagicMode = doc["playerMagicMode"].GetInt();
                if (doc.HasMember("playerMagicReflectMode")) playerMagicReflectMode = doc["playerMagicReflectMode"].GetInt();
                if (doc.HasMember("playerMagicCostType")) playerMagicCostType = static_cast<CostType>(doc["playerMagicCostType"].GetInt());
                if (doc.HasMember("playerReflectMeleeMode")) playerReflectMeleeMode = doc["playerReflectMeleeMode"].GetInt();
                if (doc.HasMember("playerReflectMeleeCostType")) playerReflectMeleeCostType = static_cast<CostType>(doc["playerReflectMeleeCostType"].GetInt());

                // MIGRÇÃO INTELIGENTE PLAYER: Se achar a flag antiga, define o modo equivalente
                if (doc.HasMember("playerReflectDamageType")) {
                    playerReflectDamageType = doc["playerReflectDamageType"].GetInt();
                }
                else if (doc.HasMember("playerReflectSkillScaling")) {
                    playerReflectDamageType = doc["playerReflectSkillScaling"].GetBool() ? 3 : 2;
                }
                if (doc.HasMember("playerReflectValue")) playerReflectValue = doc["playerReflectValue"].GetFloat();

                if (doc.HasMember("playerMeleeStagger")) playerMeleeStagger = doc["playerMeleeStagger"].GetInt();
                if (doc.HasMember("playerArrowStagger")) playerArrowStagger = doc["playerArrowStagger"].GetInt();
                if (doc.HasMember("playerMagicStagger")) playerMagicStagger = doc["playerMagicStagger"].GetInt();
                if (doc.HasMember("playerVisualMode")) playerVisualMode = doc["playerVisualMode"].GetInt();
                if (doc.HasMember("playerSoundMode")) playerSoundMode = doc["playerSoundMode"].GetInt();

                if (doc.HasMember("npcParryEnabled")) npcParryEnabled = doc["npcParryEnabled"].GetBool();
                if (doc.HasMember("npcNormalParryMS")) npcNormalParryMS = doc["npcNormalParryMS"].GetInt();
                if (doc.HasMember("npcPerfectParryMS")) npcPerfectParryMS = doc["npcPerfectParryMS"].GetInt();
                if (doc.HasMember("npcArrowMode")) npcArrowMode = doc["npcArrowMode"].GetInt();
                if (doc.HasMember("npcArrowReflectMode")) npcArrowReflectMode = doc["npcArrowReflectMode"].GetInt();
                if (doc.HasMember("npcArrowCostType")) npcArrowCostType = static_cast<CostType>(doc["npcArrowCostType"].GetInt());
                if (doc.HasMember("npcMagicMode")) npcMagicMode = doc["npcMagicMode"].GetInt();
                if (doc.HasMember("npcMagicReflectMode")) npcMagicReflectMode = doc["npcMagicReflectMode"].GetInt();
                if (doc.HasMember("npcMagicCostType")) npcMagicCostType = static_cast<CostType>(doc["npcMagicCostType"].GetInt());
                if (doc.HasMember("npcReflectMeleeMode")) npcReflectMeleeMode = doc["npcReflectMeleeMode"].GetInt();
                if (doc.HasMember("npcReflectMeleeCostType")) npcReflectMeleeCostType = static_cast<CostType>(doc["npcReflectMeleeCostType"].GetInt());

                // MIGRÇÃO INTELIGENTE NPC GLOBAL
                if (doc.HasMember("npcReflectDamageType")) {
                    npcReflectDamageType = doc["npcReflectDamageType"].GetInt();
                }
                else if (doc.HasMember("npcReflectSkillScaling")) {
                    npcReflectDamageType = doc["npcReflectSkillScaling"].GetBool() ? 3 : 2;
                }
                if (doc.HasMember("npcReflectValue")) npcReflectValue = doc["npcReflectValue"].GetFloat();

                if (doc.HasMember("npcMeleeStagger")) npcMeleeStagger = doc["npcMeleeStagger"].GetInt();
                if (doc.HasMember("npcArrowStagger")) npcArrowStagger = doc["npcArrowStagger"].GetInt();
                if (doc.HasMember("npcMagicStagger")) npcMagicStagger = doc["npcMagicStagger"].GetInt();
                if (doc.HasMember("npcVisualMode")) npcVisualMode = doc["npcVisualMode"].GetInt();
                if (doc.HasMember("npcSoundMode")) npcSoundMode = doc["npcSoundMode"].GetInt();

                if (doc.HasMember("npcParrySlowTime")) npcParrySlowTime = doc["npcParrySlowTime"].GetBool();
                if (doc.HasMember("npcParryPlayerEnabled")) npcParryPlayerEnabled = doc["npcParryPlayerEnabled"].GetBool();
                if (doc.HasMember("slowTimeMultiplier")) slowTimeMultiplier = doc["slowTimeMultiplier"].GetFloat();
                if (doc.HasMember("slowTimeDurationMS")) slowTimeDurationMS = doc["slowTimeDurationMS"].GetInt();
                if (doc.HasMember("playerParryCommitmentEnabled")) playerParryCommitmentEnabled = doc["playerParryCommitmentEnabled"].GetBool();
                if (doc.HasMember("playerParryCommitmentMS")) playerParryCommitmentMS = doc["playerParryCommitmentMS"].GetInt();
                if (doc.HasMember("playerCommitmentReductionNormalMS")) playerCommitmentReductionNormalMS = doc["playerCommitmentReductionNormalMS"].GetInt();
                if (doc.HasMember("playerCommitmentReductionPerfectMS")) playerCommitmentReductionPerfectMS = doc["playerCommitmentReductionPerfectMS"].GetInt();

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
        const char* costOpts[] = { GetLoc("menu.opt_none", "None"), GetLoc("menu.opt_stamina", "Stamina"), GetLoc("menu.opt_magicka", "Magicka") };

        if (ImGui::CollapsingHeader(GetLoc("menu.npc_global_header", "NPC Global Settings"), ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::Checkbox(GetLoc("menu.npc_enable_parry", "Enable NPC Parry"), &npcParryEnabled)) changed = true;

            if (npcParryEnabled) {
                ImGui::Indent();
                ImGui::SetNextItemWidth(200.0f);
                if (ImGui::SliderInt(GetLoc("menu.npc_normal_window", "NPC Normal Window (ms)"), &npcNormalParryMS, 50, 2000)) changed = true;
                ImGui::SetNextItemWidth(200.0f);
                if (ImGui::SliderInt(GetLoc("menu.npc_perfect_window", "NPC Perfect Window (ms)"), &npcPerfectParryMS, 10, 500)) changed = true;
                ImGui::Spacing();
                if (ImGui::Combo(GetLoc("menu.npc_visual_effects", "NPC Visual Effects"), &npcVisualMode, reflectOpts, 3)) changed = true;
                if (ImGui::Combo(GetLoc("menu.npc_sound_effects", "NPC Sound Effects"), &npcSoundMode, reflectOpts, 3)) changed = true;
                ImGui::Spacing();
                if (ImGui::Checkbox(GetLoc("menu.npc_can_parry_player", "Can Parry Player"), &npcParryPlayerEnabled)) changed = true;
                if (npcParryPlayerEnabled) {
                    if (ImGui::Checkbox(GetLoc("menu.npc_slow_time", "Slow Time on NPC Parry the Player"), &npcParrySlowTime)) changed = true;
                }
                if (ImGui::Combo(GetLoc("menu.npc_stagger_melee", "Stagger Attacker on Melee Parry"), &npcMeleeStagger, reflectOpts, 3)) changed = true;
                if (ImGui::Combo(GetLoc("menu.npc_reflect_melee", "NPC Reflect Melee Damage"), &npcReflectMeleeMode, reflectOpts, 3)) changed = true;
                if (npcReflectMeleeMode > 0) {
                    ImGui::Indent();
                    std::string comboLabel = std::string(GetLoc("menu.reflect_calc_type", "Reflect Calculation Type")) + "##NPCGlobal";
                    if (DrawReflectTypeCombo(comboLabel.c_str(), &npcReflectDamageType)) changed = true;
                    if (npcReflectDamageType == 0) {
                        std::string flatLabel = std::string(GetLoc("menu.flat_damage_val", "Flat Damage Value")) + "##NPCGlobal";
                        if (ImGui::SliderFloat(flatLabel.c_str(), &npcReflectValue, 1.0f, 500.0f, "%.0f")) changed = true;
                    }
                    else if (npcReflectDamageType == 1) {
                        std::string pctLabel = std::string(GetLoc("menu.pct_modifier", "Percentage Modifier (%)")) + "##NPCGlobal";
                        if (ImGui::SliderFloat(pctLabel.c_str(), &npcReflectValue, 1.0f, 200.0f, "%.0f%%")) changed = true;
                    }
                    if (ImGui::Combo(GetLoc("menu.npc_reflect_cost_type", "NPC Reflect Cost Type"), (int*)&npcReflectMeleeCostType, costOpts, 3)) changed = true;
                    ImGui::Unindent();
                }
                if (ImGui::Combo(GetLoc("menu.npc_parry_arrows", "NPC Parry Arrows"), &npcArrowMode, modeOpts, 3)) changed = true;
                if (npcArrowMode > 0) {
                    ImGui::Indent();
                    if (ImGui::Combo(GetLoc("menu.npc_reflect_arrows", "NPC Reflect Arrows"), &npcArrowReflectMode, reflectOpts, 3)) changed = true;
                    if (npcArrowMode == 2) {
                        if (ImGui::Combo(GetLoc("menu.npc_arrow_cost", "NPC Arrow Cost"), (int*)&npcArrowCostType, costOpts, 3)) changed = true;
                    }
                    if (ImGui::Combo(GetLoc("menu.npc_stagger_shooter", "Stagger Shooter"), &npcArrowStagger, reflectOpts, 3)) changed = true;
                    ImGui::Unindent();
                }
                if (ImGui::Combo(GetLoc("menu.npc_parry_magic", "NPC Parry Magic"), &npcMagicMode, modeOpts, 3)) changed = true;
                if (npcMagicMode > 0) {
                    ImGui::Indent();
                    if (ImGui::Combo(GetLoc("menu.npc_reflect_magic", "NPC Reflect Magic"), &npcMagicReflectMode, reflectOpts, 3)) changed = true;
                    if (npcMagicMode == 2) {
                        if (ImGui::Combo(GetLoc("menu.npc_magic_cost", "NPC Magic Cost"), (int*)&npcMagicCostType, costOpts, 3)) changed = true;
                    }
                    if (ImGui::Combo(GetLoc("menu.npc_stagger_caster", "Stagger Caster"), &npcMagicStagger, reflectOpts, 3)) changed = true;
                    ImGui::Unindent();
                }
                ImGui::Unindent();
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored({ 0.4f, 1.0f, 0.4f, 1.0f }, "%s", GetLoc("menu.npc_rules_perk_header", "NPC Parry Rules (By Perk)"));

        if (ImGui::Button(GetLoc("menu.add_rule", "+ Add Rule"))) {
            NPCParryRule newRule;
            newRule.ruleName = "New Rule " + std::to_string(npcRules.size() + 1);
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
                        std::string oldPath = RULES_DIR + rule.ruleName + ".json";
                        if (std::filesystem::exists(oldPath)) std::filesystem::remove(oldPath);
                        rule.ruleName = newName;
                        changed = true;
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
                    ImGui::SetNextItemWidth(200.0f);
                    std::string normalLabel = std::string(GetLoc("menu.normal_window", "Normal Window (ms)")) + "##Rule";
                    if (ImGui::SliderInt(normalLabel.c_str(), &rule.npcNormalParryMS, 50, 2000)) changed = true;
                    ImGui::SetNextItemWidth(200.0f);
                    std::string perfectLabel = std::string(GetLoc("menu.perfect_window", "Perfect Window (ms)")) + "##Rule";
                    if (ImGui::SliderInt(perfectLabel.c_str(), &rule.npcPerfectParryMS, 10, 500)) changed = true;

                    std::string parryArrowsLabel = std::string(GetLoc("menu.parry_arrows", "Parry Arrows")) + "##Rule";
                    if (ImGui::Combo(parryArrowsLabel.c_str(), &rule.npcArrowMode, modeOpts, 3)) changed = true;
                    if (rule.npcArrowMode > 0) {
                        ImGui::Indent();
                        std::string reflectArrowsLabel = std::string(GetLoc("menu.reflect_arrows", "Reflect Arrows")) + "##Rule";
                        if (ImGui::Combo(reflectArrowsLabel.c_str(), &rule.npcArrowReflectMode, reflectOpts, 3)) changed = true;
                        if (rule.npcArrowMode == 2) {
                            std::string arrowCostLabel = std::string(GetLoc("menu.arrow_cost", "Arrow Cost")) + "##Rule";
                            if (ImGui::Combo(arrowCostLabel.c_str(), (int*)&rule.npcArrowCostType, costOpts, 3)) changed = true;
                        }
                        std::string staggerShooterLabel = std::string(GetLoc("menu.stagger_shooter", "Stagger Shooter")) + "##Rule";
                        if (ImGui::Combo(staggerShooterLabel.c_str(), &rule.npcArrowStagger, reflectOpts, 3)) changed = true;
                        ImGui::Unindent();
                    }

                    std::string parryMagicLabel = std::string(GetLoc("menu.parry_magic", "Parry Magic")) + "##Rule";
                    if (ImGui::Combo(parryMagicLabel.c_str(), &rule.npcMagicMode, modeOpts, 3)) changed = true;
                    if (rule.npcMagicMode > 0) {
                        ImGui::Indent();
                        std::string reflectMagicLabel = std::string(GetLoc("menu.reflect_magic", "Reflect Magic")) + "##Rule";
                        if (ImGui::Combo(reflectMagicLabel.c_str(), &rule.npcMagicReflectMode, reflectOpts, 3)) changed = true;
                        if (rule.npcMagicMode == 2) {
                            std::string magicCostLabel = std::string(GetLoc("menu.magic_cost", "Magic Cost")) + "##Rule";
                            if (ImGui::Combo(magicCostLabel.c_str(), (int*)&rule.npcMagicCostType, costOpts, 3)) changed = true;
                        }
                        std::string staggerCasterLabel = std::string(GetLoc("menu.stagger_caster", "Stagger Caster")) + "##Rule";
                        if (ImGui::Combo(staggerCasterLabel.c_str(), &rule.npcMagicStagger, reflectOpts, 3)) changed = true;
                        ImGui::Unindent();
                    }

                    std::string reflectMeleeLabel = std::string(GetLoc("menu.reflect_melee", "Reflect Melee Damage")) + "##Rule";
                    if (ImGui::Combo(reflectMeleeLabel.c_str(), &rule.npcReflectMeleeMode, reflectOpts, 3)) changed = true;
                    if (rule.npcReflectMeleeMode > 0) {
                        ImGui::Indent();
                        std::string comboLabel = std::string(GetLoc("menu.reflect_calc_type", "Reflect Calculation Type")) + "##Rule_" + std::to_string(i);
                        if (DrawReflectTypeCombo(comboLabel.c_str(), &rule.npcReflectDamageType)) changed = true;

                        if (rule.npcReflectDamageType == 0) {
                            std::string flatLabel = std::string(GetLoc("menu.flat_damage_val", "Flat Damage Value")) + "##Rule";
                            if (ImGui::SliderFloat(flatLabel.c_str(), &rule.npcReflectValue, 1.0f, 500.0f, "%.0f")) changed = true;
                        }
                        else if (rule.npcReflectDamageType == 1) {
                            std::string pctLabel = std::string(GetLoc("menu.pct_modifier", "Percentage Modifier (%)")) + "##Rule";
                            if (ImGui::SliderFloat(pctLabel.c_str(), &rule.npcReflectValue, 1.0f, 200.0f, "%.0f%%")) changed = true;
                        }
                        std::string reflectCostLabel = std::string(GetLoc("menu.reflect_cost_type", "Reflect Cost Type")) + "##Rule";
                        if (ImGui::Combo(reflectCostLabel.c_str(), (int*)&rule.npcReflectMeleeCostType, costOpts, 3)) changed = true;
                        ImGui::Unindent();
                    }

                    std::string staggerAttackerLabel = std::string(GetLoc("menu.stagger_attacker", "Stagger Attacker")) + "##Rule";
                    if (ImGui::Combo(staggerAttackerLabel.c_str(), &rule.npcMeleeStagger, reflectOpts, 3)) changed = true;
                    std::string visualEffectsLabel = std::string(GetLoc("menu.visual_effects", "Visual Effects")) + "##Rule";
                    if (ImGui::Combo(visualEffectsLabel.c_str(), &rule.npcVisualMode, reflectOpts, 3)) changed = true;
                    std::string soundEffectsLabel = std::string(GetLoc("menu.sound_effects", "Sound Effects")) + "##Rule";
                    if (ImGui::Combo(soundEffectsLabel.c_str(), &rule.npcSoundMode, reflectOpts, 3)) changed = true;
                }

                ImGui::Spacing();
                if (ImGui::Button(GetLoc("menu.remove_rule", "Remove Rule"), { 150, 0 })) {
                    std::string path = RULES_DIR + rule.ruleName + ".json";
                    if (std::filesystem::exists(path)) std::filesystem::remove(path);
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
        const char* costOpts[] = { GetLoc("menu.opt_none", "None"), GetLoc("menu.opt_stamina", "Stamina"), GetLoc("menu.opt_magicka", "Magicka") };

        if (ImGuiMCP::CollapsingHeader(GetLoc("menu.player_settings_header", "Player Settings"), ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGuiMCP::Checkbox(GetLoc("menu.player_enable_parry", "Enable Player Parry"), &playerParryEnabled)) changed = true;

            if (playerParryEnabled) {
                ImGuiMCP::Indent();
                ImGuiMCP::SetNextItemWidth(200.0f);
                if (ImGuiMCP::SliderInt(GetLoc("menu.player_normal_window", "Normal Window (ms)"), &playerNormalParryMS, 50, 2000)) changed = true;
                ImGuiMCP::SameLine();
                ImGuiMCP::SetNextItemWidth(60.0f);
                if (ImGuiMCP::InputInt("##NormalPrecise", &playerNormalParryMS, 0, 0)) {
                    playerNormalParryMS = std::clamp(playerNormalParryMS, 50, 2000);
                    changed = true;
                }

                // Perfect Window
                ImGuiMCP::SetNextItemWidth(200.0f);
                if (ImGuiMCP::SliderInt(GetLoc("menu.player_perfect_window", "Perfect Window (ms)"), &playerPerfectParryMS, 10, 500)) changed = true;
                ImGuiMCP::SameLine();
                ImGuiMCP::SetNextItemWidth(60.0f);
                if (ImGuiMCP::InputInt("##PerfectPrecise", &playerPerfectParryMS, 0, 0)) {
                    playerPerfectParryMS = std::clamp(playerPerfectParryMS, 10, 500);
                    changed = true;
                }

                ImGuiMCP::Spacing();
                if (ImGuiMCP::Checkbox(GetLoc("menu.player_enable_commitment", "Enable Parry Commitment"), &playerParryCommitmentEnabled)) changed = true;
                if (playerParryCommitmentEnabled) {
                    ImGuiMCP::Indent();

                    // Slider 1: Commitment Time
                    ImGuiMCP::SetNextItemWidth(200.0f);
                    if (ImGuiMCP::SliderInt(GetLoc("menu.player_commitment_time", "Commitment Time (ms)"), &playerParryCommitmentMS, 100, 2000)) changed = true;
                    ImGuiMCP::SameLine();
                    ImGuiMCP::SetNextItemWidth(60.0f);
                    if (ImGuiMCP::InputInt("##CommitPrecise", &playerParryCommitmentMS, 0, 0)) {
                        playerParryCommitmentMS = std::clamp(playerParryCommitmentMS, 100, 2000);
                        changed = true;
                    }

                    // Slider 2: Reduction Normal
                    ImGuiMCP::SetNextItemWidth(200.0f);
                    if (ImGuiMCP::SliderInt(GetLoc("menu.player_reduc_normal", "Reduction on Normal Parry (ms)"), &playerCommitmentReductionNormalMS, 0, 2000)) changed = true;
                    ImGuiMCP::SameLine();
                    ImGuiMCP::SetNextItemWidth(60.0f);
                    if (ImGuiMCP::InputInt("##ReducNormPrecise", &playerCommitmentReductionNormalMS, 0, 0)) {
                        playerCommitmentReductionNormalMS = std::clamp(playerCommitmentReductionNormalMS, 0, 2000);
                        changed = true;
                    }

                    // Slider 3: Reduction Perfect
                    ImGuiMCP::SetNextItemWidth(200.0f);
                    if (ImGuiMCP::SliderInt(GetLoc("menu.player_reduc_perfect", "Reduction on Perfect Parry (ms)"), &playerCommitmentReductionPerfectMS, 0, 2000)) changed = true;
                    ImGuiMCP::SameLine();
                    ImGuiMCP::SetNextItemWidth(60.0f);
                    if (ImGuiMCP::InputInt("##ReducPerfPrecise", &playerCommitmentReductionPerfectMS, 0, 0)) {
                        playerCommitmentReductionPerfectMS = std::clamp(playerCommitmentReductionPerfectMS, 0, 2000);
                        changed = true;
                    }

                    ImGuiMCP::Unindent();
                }

                ImGuiMCP::Spacing();
                if (ImGuiMCP::Combo(GetLoc("menu.player_visual_effects", "Visual Effects"), &playerVisualMode, reflectOpts, 3)) changed = true;
                if (ImGuiMCP::Combo(GetLoc("menu.player_sound_effects", "Sound Effects"), &playerSoundMode, reflectOpts, 3)) changed = true;
                ImGuiMCP::TextDisabled("%s", GetLoc("menu.advanced_features", "Advanced Features"));
                if (ImGuiMCP::Combo(GetLoc("menu.player_stagger_melee", "Stagger Attacker on Melee Parry"), &playerMeleeStagger, reflectOpts, 3)) changed = true;
                if (ImGuiMCP::Combo(GetLoc("menu.player_reflect_melee", "Reflect Melee Damage"), &playerReflectMeleeMode, reflectOpts, 3)) changed = true;
                if (playerReflectMeleeMode > 0) {
                    ImGuiMCP::Indent();
                    std::string comboLabel = std::string(GetLoc("menu.reflect_calc_type", "Reflect Calculation Type")) + "##Player";
                    if (DrawReflectTypeCombo(comboLabel.c_str(), &playerReflectDamageType)) changed = true;
                    if (playerReflectDamageType == 0) {
                        std::string flatLabel = std::string(GetLoc("menu.flat_damage_val", "Flat Damage Value")) + "##Player";
                        if (ImGuiMCP::SliderFloat(flatLabel.c_str(), &playerReflectValue, 1.0f, 500.0f, "%.0f")) changed = true;
                    }
                    else if (playerReflectDamageType == 1) {
                        std::string pctLabel = std::string(GetLoc("menu.pct_modifier", "Percentage Modifier (%)")) + "##Player";
                        if (ImGuiMCP::SliderFloat(pctLabel.c_str(), &playerReflectValue, 1.0f, 200.0f, "%.0f%%")) changed = true;
                    }
                    if (ImGuiMCP::Combo(GetLoc("menu.player_reflect_cost_type", "Reflect Cost Type"), (int*)&playerReflectMeleeCostType, costOpts, 3)) changed = true;
                    ImGuiMCP::Unindent();
                }

                // Arrow
                if (ImGuiMCP::Combo(GetLoc("menu.player_parry_arrows", "Parry Arrows"), &playerArrowMode, modeOpts, 3)) changed = true;
                if (playerArrowMode > 0) {
                    ImGuiMCP::Indent();
                    if (ImGuiMCP::Combo(GetLoc("menu.player_reflect_arrows", "Reflect Arrows"), &playerArrowReflectMode, reflectOpts, 3)) changed = true;
                    if (playerArrowMode == 2) {
                        if (ImGuiMCP::Combo(GetLoc("menu.player_arrow_cost", "Arrow Cost"), (int*)&playerArrowCostType, costOpts, 3)) changed = true;
                    }
                    if (ImGuiMCP::Combo(GetLoc("menu.player_stagger_shooter", "Stagger Shooter"), &playerArrowStagger, reflectOpts, 3)) changed = true;
                    ImGuiMCP::Unindent();
                }

                // Magic
                if (ImGuiMCP::Combo(GetLoc("menu.player_parry_magic", "Parry Magic"), &playerMagicMode, modeOpts, 3)) changed = true;
                if (playerMagicMode > 0) {
                    ImGuiMCP::Indent();
                    if (ImGuiMCP::Combo(GetLoc("menu.player_reflect_magic", "Reflect Magic"), &playerMagicReflectMode, reflectOpts, 3)) changed = true;
                    if (playerMagicMode == 2) {
                        if (ImGuiMCP::Combo(GetLoc("menu.player_magic_cost", "Magic Cost"), (int*)&playerMagicCostType, costOpts, 3)) changed = true;
                    }
                    if (ImGuiMCP::Combo(GetLoc("menu.player_stagger_caster", "Stagger Caster"), &playerMagicStagger, reflectOpts, 3)) changed = true;
                    ImGuiMCP::Unindent();
                }

                ImGuiMCP::Unindent();
            }
        }

        ImGuiMCP::Spacing();
        if (ImGuiMCP::CollapsingHeader(GetLoc("menu.slow_motion_header", "Slow Motion Parry"))) {
            // Multiplier (Float)
            ImGuiMCP::SetNextItemWidth(200.0f);
            if (ImGuiMCP::SliderFloat(GetLoc("menu.slow_time_multiplier", "Time Multiplier"), &slowTimeMultiplier, 0.02f, 1.0f, "%.2f")) changed = true;
            ImGuiMCP::SameLine();
            ImGuiMCP::SetNextItemWidth(60.0f);
            if (ImGuiMCP::InputFloat("##SlowMultPrecise", &slowTimeMultiplier, 0.0f, 0.0f, "%.2f")) {
                slowTimeMultiplier = std::clamp(slowTimeMultiplier, 0.02f, 1.0f);
                changed = true;
            }

            // Duration
            ImGuiMCP::SetNextItemWidth(200.0f);
            if (ImGuiMCP::SliderInt(GetLoc("menu.slow_time_duration", "Duration (ms)"), &slowTimeDurationMS, 100, 5000)) changed = true;
            ImGuiMCP::SameLine();
            ImGuiMCP::SetNextItemWidth(60.0f);
            if (ImGuiMCP::InputInt("##SlowDurPrecise", &slowTimeDurationMS, 0, 0)) {
                slowTimeDurationMS = std::clamp(slowTimeDurationMS, 100, 5000);
                changed = true;
            }
        }

        if (changed) Save();
    }

    void GeneralMenu()
    {
        bool changed = false;

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
                ImGuiMCP::SetNextItemWidth(250.0f);
                std::string baseLabel = std::string(GetLoc("menu.w2w_base_value", "Base Value")) + "##W2W";
                if (ImGuiMCP::SliderFloat(baseLabel.c_str(), &w2wBaseChance, 0.0f, 100.0f, "%.1f")) changed = true;
                ImGuiMCP::SameLine();
                ImGuiMCP::SetNextItemWidth(70.0f);
                if (ImGuiMCP::InputFloat("##W2WBasePrecise", &w2wBaseChance, 0.0f, 0.0f, "%.1f")) {
                    w2wBaseChance = std::clamp(w2wBaseChance, 0.0f, 500.0f);
                    changed = true;
                }

                // --- Damage Mult ---
                ImGuiMCP::SetNextItemWidth(250.0f);
                std::string dmgLabel = std::string(GetLoc("menu.w2w_damage_mult", "Damage Mult")) + "##W2W";
                if (ImGuiMCP::SliderFloat(dmgLabel.c_str(), &w2wDamageMult, 0.0f, 10.0f, "%.2f")) changed = true;
                ImGuiMCP::SameLine();
                ImGuiMCP::SetNextItemWidth(70.0f);
                if (ImGuiMCP::InputFloat("##W2WDmgPrecise", &w2wDamageMult, 0.0f, 0.0f, "%.2f")) {
                    w2wDamageMult = std::clamp(w2wDamageMult, 0.0f, 100.0f);
                    changed = true;
                }

                // --- Skill Mult ---
                ImGuiMCP::SetNextItemWidth(250.0f);
                std::string skillLabel = std::string(GetLoc("menu.w2w_skill_mult", "Skill Mult")) + "##W2W";
                if (ImGuiMCP::SliderFloat(skillLabel.c_str(), &w2wSkillMult, 0.0f, 10.0f, "%.2f")) changed = true;
                ImGuiMCP::SameLine();
                ImGuiMCP::SetNextItemWidth(70.0f);
                if (ImGuiMCP::InputFloat("##W2WSkillPrecise", &w2wSkillMult, 0.0f, 0.0f, "%.2f")) {
                    w2wSkillMult = std::clamp(w2wSkillMult, 0.0f, 100.0f);
                    changed = true;
                }

                // --- Global Difficulty ---
                ImGuiMCP::SetNextItemWidth(250.0f);
                std::string diffLabel = std::string(GetLoc("menu.w2w_global_difficulty", "Global Difficulty")) + "##W2W";
                if (ImGuiMCP::SliderFloat(diffLabel.c_str(), &w2wGlobalDifficulty, 1.0f, 1000.0f, "%.1f")) changed = true;
                ImGuiMCP::SameLine();
                ImGuiMCP::SetNextItemWidth(70.0f);
                if (ImGuiMCP::InputFloat("##W2WDiffPrecise", &w2wGlobalDifficulty, 0.0f, 0.0f, "%.1f")) {
                    w2wGlobalDifficulty = std::clamp(w2wGlobalDifficulty, 1.0f, 5000.0f);
                    changed = true;
                }

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
        SKSEMenuFramework::SetSection("Parry for all");
        SKSEMenuFramework::AddSectionItem("Player Settings", PlayerMenu);
        SKSEMenuFramework::AddSectionItem("NPC Settings", NPCMenu);
        SKSEMenuFramework::AddSectionItem("General Settings", GeneralMenu);
    }
}

