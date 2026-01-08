#include "Settings.h"

const char* ParryPath = "Data/SKSE/Plugins/ParryAll.json";

// Strings para os Dropdowns
const char* ModeOptions = "Disabled\0Enabled (Free)\0Enabled (Cost)\0\0";
const char* ReflectOptions = "Disable\0Enable\0Enable only in perfect\0\0";
const char* CostOptionsUI = "None\0Stamina\0Magicka\0\0"; // Health oculto conforme solicitado

void ParrySettings::Save() {
    rapidjson::Document doc;
    doc.SetObject();
    auto& allocator = doc.GetAllocator();

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
    doc.AddMember("playerMeleeStagger", playerMeleeStagger, allocator);
    doc.AddMember("playerArrowStagger", playerArrowStagger, allocator);
    doc.AddMember("playerMagicStagger", playerMagicStagger, allocator);

    // NPC
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
    doc.AddMember("npcMeleeStagger", npcMeleeStagger, allocator);
    doc.AddMember("npcArrowStagger", npcArrowStagger, allocator);
    doc.AddMember("npcMagicStagger", npcMagicStagger, allocator);

    doc.AddMember("npcParrySlowTime", npcParrySlowTime, allocator);
    doc.AddMember("slowTimeMultiplier", slowTimeMultiplier, allocator);
    doc.AddMember("slowTimeDurationMS", slowTimeDurationMS, allocator);

    FILE* fp = nullptr;
    fopen_s(&fp, ParryPath, "wb");
    if (fp) {
        char writeBuffer[65536];
        rapidjson::FileWriteStream os(fp, writeBuffer, sizeof(writeBuffer));
        rapidjson::Writer<rapidjson::FileWriteStream> writer(os);
        doc.Accept(writer);
        fclose(fp);
    }
}

void ParrySettings::MmRender()
{
    bool changed = false;
    ImGuiMCP::Text("Parry System Configuration");
    ImGuiMCP::Separator();

    // --- PLAYER SECTION ---
    if (ImGuiMCP::CollapsingHeader("Player Settings", ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGuiMCP::Checkbox("Enable Player Parry", &playerParryEnabled)) changed = true;

        if (playerParryEnabled) {
            ImGuiMCP::Indent();
            if (ImGuiMCP::SliderInt("Normal Window (ms)", &playerNormalParryMS, 50, 2000)) changed = true;
            if (ImGuiMCP::SliderInt("Perfect Window (ms)", &playerPerfectParryMS, 10, 500)) changed = true;

            ImGuiMCP::Spacing();
            ImGuiMCP::TextDisabled("Advanced Features");
            if (ImGuiMCP::Checkbox("Stagger Attacker on Melee Parry", &playerMeleeStagger)) changed = true;
            if (ImGuiMCP::Combo("Reflect Melee Damage", &playerReflectMeleeMode, ReflectOptions)) changed = true;
            if (playerReflectMeleeMode > 0) {
                ImGuiMCP::Indent();
                if (ImGuiMCP::Combo("Reflect Cost Type", (int*)&playerReflectMeleeCostType, CostOptionsUI)) changed = true;
                ImGuiMCP::Unindent();
            }
            // Arrow
            if (ImGuiMCP::Combo("Parry Arrows", &playerArrowMode, ModeOptions)) changed = true;
            if (playerArrowMode > 0) {
                ImGuiMCP::Indent();
                if (ImGuiMCP::Combo("Reflect Arrows", &playerArrowReflectMode, ReflectOptions)) changed = true;
                if (playerArrowMode == 2) {
                    if (ImGuiMCP::Combo("Arrow Cost", (int*)&playerArrowCostType, CostOptionsUI)) changed = true;
                }
                if (ImGuiMCP::Checkbox("Stagger Shooter", &playerArrowStagger)) changed = true;
                ImGuiMCP::Unindent();
            }

            // Magic
            if (ImGuiMCP::Combo("Parry Magic", &playerMagicMode, ModeOptions)) changed = true;
            if (playerMagicMode > 0) {
                ImGuiMCP::Indent();
                if (ImGuiMCP::Combo("Reflect Magic", &playerMagicReflectMode, ReflectOptions)) changed = true;
                if (playerMagicMode == 2) {
                    if (ImGuiMCP::Combo("Magic Cost", (int*)&playerMagicCostType, CostOptionsUI)) changed = true;
                }
                if (ImGuiMCP::Checkbox("Stagger Caster", &playerMagicStagger)) changed = true;
                ImGuiMCP::Unindent();
            }

            ImGuiMCP::Unindent();
        }
    }

    // --- NPC SECTION ---
    if (ImGuiMCP::CollapsingHeader("NPC Settings", ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGuiMCP::Checkbox("Enable NPC Parry", &npcParryEnabled)) changed = true;

        if (npcParryEnabled) {
            ImGuiMCP::Indent();
            if (ImGuiMCP::SliderInt("NPC Normal Window (ms)", &npcNormalParryMS, 50, 2000)) changed = true;
            if (ImGuiMCP::SliderInt("NPC Perfect Window (ms)", &npcPerfectParryMS, 10, 500)) changed = true;
            if (ImGuiMCP::Checkbox("Slow Time on NPC Parry", &npcParrySlowTime)) changed = true;
            if (ImGuiMCP::Checkbox("NPC Stagger Attacker on Melee Parry", &npcMeleeStagger)) changed = true;

            if (ImGuiMCP::Combo("NPC Reflect Melee Damage", &npcReflectMeleeMode, ReflectOptions)) changed = true;
            if (npcReflectMeleeMode > 0) {
                ImGuiMCP::Indent();
                if (ImGuiMCP::Combo("NPC Reflect Cost Type", (int*)&npcReflectMeleeCostType, CostOptionsUI)) changed = true;
                ImGuiMCP::Unindent();
            }
            // NPC Arrow
            if (ImGuiMCP::Combo("NPC Parry Arrows", &npcArrowMode, ModeOptions)) changed = true;
            if (npcArrowMode > 0) {
                ImGuiMCP::Indent();
                if (ImGuiMCP::Combo("NPC Reflect Arrows", &npcArrowReflectMode, ReflectOptions)) changed = true;
                if (npcArrowMode == 2) {
                    if (ImGuiMCP::Combo("NPC Arrow Cost", (int*)&npcArrowCostType, CostOptionsUI)) changed = true;
                }
                if (ImGuiMCP::Checkbox("NPC Stagger Shooter", &npcArrowStagger)) changed = true;
                ImGuiMCP::Unindent();
            }

            // NPC Magic
            if (ImGuiMCP::Combo("NPC Parry Magic", &npcMagicMode, ModeOptions)) changed = true;
            if (npcMagicMode > 0) {
                ImGuiMCP::Indent();
                if (ImGuiMCP::Combo("NPC Reflect Magic", &npcMagicReflectMode, ReflectOptions)) changed = true;
                if (npcMagicMode == 2) {
                    if (ImGuiMCP::Combo("NPC Magic Cost", (int*)&npcMagicCostType, CostOptionsUI)) changed = true;
                }
                if (ImGuiMCP::Checkbox("NPC Stagger Caster", &npcMagicStagger)) changed = true;
                ImGuiMCP::Unindent();
            }
           
            ImGuiMCP::Unindent();
        }
    }

    ImGuiMCP::Spacing();
    if (ImGuiMCP::CollapsingHeader("Slow Motion Parry")) {
        if (ImGuiMCP::SliderFloat("Time Multiplier", &slowTimeMultiplier, 0.05f, 1.0f, "%.2f")) changed = true;
        if (ImGuiMCP::SliderInt("Duration (ms)", &slowTimeDurationMS, 100, 5000)) changed = true;
    }

    if (changed) Save();
}

void ParrySettings::MmRegister() {
    if (SKSEMenuFramework::IsInstalled()) {
        SKSEMenuFramework::SetSection("Parry For All");
        SKSEMenuFramework::AddSectionItem("General Settings", MmRender);
    }
}

void ParrySettings::Load() {
    FILE* fp = nullptr;
    fopen_s(&fp, ParryPath, "rb");
    if (fp) {
        char readBuffer[65536];
        rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
        rapidjson::Document doc;
        doc.ParseStream(is);
        fclose(fp);

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

            if (doc.HasMember("playerMeleeStagger")) playerMeleeStagger = doc["playerMeleeStagger"].GetBool();
            if (doc.HasMember("playerArrowStagger")) playerArrowStagger = doc["playerArrowStagger"].GetBool();
            if (doc.HasMember("playerMagicStagger")) playerMagicStagger = doc["playerMagicStagger"].GetBool();

            // NPC Load...
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

            if (doc.HasMember("npcMeleeStagger")) npcMeleeStagger = doc["npcMeleeStagger"].GetBool();
            if (doc.HasMember("npcArrowStagger")) npcArrowStagger = doc["npcArrowStagger"].GetBool();
            if (doc.HasMember("npcMagicStagger")) npcMagicStagger = doc["npcMagicStagger"].GetBool();

            if (doc.HasMember("npcParrySlowTime")) npcParrySlowTime = doc["npcParrySlowTime"].GetBool();
            if (doc.HasMember("slowTimeMultiplier")) slowTimeMultiplier = doc["slowTimeMultiplier"].GetFloat();
            if (doc.HasMember("slowTimeDurationMS")) slowTimeDurationMS = doc["slowTimeDurationMS"].GetInt();
        }
    }
}