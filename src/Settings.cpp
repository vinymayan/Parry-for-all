#include "SKSEMCP/SKSEMenuFramework.hpp"
#include "Settings.h"

void Settings::Save() {
    rapidjson::Document doc;
    doc.SetObject();
    auto& allocator = doc.GetAllocator();

    doc.AddMember("playerNormalParryMS", playerNormalParryMS, allocator);
    doc.AddMember("playerPerfectParryMS", playerPerfectParryMS, allocator);
    doc.AddMember("npcParryEnabled", npcParryEnabled, allocator);
    doc.AddMember("npcNormalParryMS", npcNormalParryMS, allocator);
    doc.AddMember("npcPerfectParryMS", npcPerfectParryMS, allocator);
    doc.AddMember("slowTimeMultiplier", slowTimeMultiplier, allocator);
    doc.AddMember("slowTimeDurationMS", slowTimeDurationMS, allocator);

    FILE* fp = nullptr;
    fopen_s(&fp, SETTINGS_PATH, "wb");
    if (fp) {
        char writeBuffer[65536];
        rapidjson::FileWriteStream os(fp, writeBuffer, sizeof(writeBuffer));
        rapidjson::Writer<rapidjson::FileWriteStream> writer(os);
        doc.Accept(writer);
        fclose(fp);
    }
}

void Settings::MmRender()
{
    bool changed = false;

    ImGuiMCP::Text("Parry System Configuration");
    ImGuiMCP::Separator();

    // Seção Player
    if (ImGuiMCP::CollapsingHeader("Player Settings", ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGuiMCP::SliderInt("Normal Parry Window (ms)", (int*)&Settings::playerNormalParryMS, 50, 2000)) changed = true;
        if (ImGuiMCP::SliderInt("Perfect Parry Window (ms)", (int*)&Settings::playerPerfectParryMS, 10, 500)) changed = true;
    }

    ImGuiMCP::Spacing();

    // Seção NPC
    if (ImGuiMCP::CollapsingHeader("NPC Settings", ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGuiMCP::Checkbox("Enable NPC Parry", &Settings::npcParryEnabled)) changed = true;
        if (Settings::npcParryEnabled) {
            ImGuiMCP::Indent();
            if (ImGuiMCP::SliderInt("NPC Normal Window (ms)", (int*)&Settings::npcNormalParryMS, 50, 2000)) changed = true;
            if (ImGuiMCP::SliderInt("NPC Perfect Window (ms)", (int*)&Settings::npcPerfectParryMS, 10, 500)) changed = true;
            ImGuiMCP::Unindent();
        }
    }

    ImGuiMCP::Spacing();
    ImGuiMCP::Separator();

    // Seção Slow Time
    if (ImGuiMCP::CollapsingHeader("Slow Motion Settings")) {
        if (ImGuiMCP::SliderFloat("Time Multiplier", &Settings::slowTimeMultiplier, 0.05f, 1.0f, "%.2f")) changed = true;
        if (ImGuiMCP::SliderInt("Duration (ms)", &Settings::slowTimeDurationMS, 100, 5000)) changed = true;
    }

    if (changed) {
        Settings::Save();
    }
}

void Settings::MmRegister()
{
    if (SKSEMenuFramework::IsInstalled()) {
        SKSEMenuFramework::SetSection("Parry For All");
        SKSEMenuFramework::AddSectionItem("General Settings", MmRender);
    }
}

void Settings::Load() {
    FILE* fp = nullptr;
    fopen_s(&fp, SETTINGS_PATH, "rb");
    if (fp) {
        char readBuffer[65536];
        rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
        rapidjson::Document doc;
        doc.ParseStream(is);
        fclose(fp);

        if (doc.IsObject()) {
            if (doc.HasMember("playerNormalParryMS")) playerNormalParryMS = doc["playerNormalParryMS"].GetInt();
            if (doc.HasMember("playerPerfectParryMS")) playerPerfectParryMS = doc["playerPerfectParryMS"].GetInt();
            if (doc.HasMember("npcParryEnabled")) npcParryEnabled = doc["npcParryEnabled"].GetBool();
            if (doc.HasMember("npcNormalParryMS")) npcNormalParryMS = doc["npcNormalParryMS"].GetInt();
            if (doc.HasMember("npcPerfectParryMS")) npcPerfectParryMS = doc["npcPerfectParryMS"].GetInt();
            if (doc.HasMember("slowTimeMultiplier")) slowTimeMultiplier = doc["slowTimeMultiplier"].GetFloat();
            if (doc.HasMember("slowTimeDurationMS")) slowTimeDurationMS = doc["slowTimeDurationMS"].GetInt();
        }
    }
}