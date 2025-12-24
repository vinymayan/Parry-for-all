#pragma once
#include <string>
#include <cstdio>
#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/filewritestream.h"
#include "rapidjson/writer.h"


namespace Settings {
    // Player Timings
    inline long playerNormalParryMS = 500;
    inline long playerPerfectParryMS = 150;

    // NPC Timings
    inline bool npcParryEnabled = true;
    inline long npcNormalParryMS = 400;
    inline long npcPerfectParryMS = 100;

    // Slow Time
    inline float slowTimeMultiplier = 0.2f;
    inline int slowTimeDurationMS = 1000;

    const char* SETTINGS_PATH = "Data/SKSE/Plugins/ParrySystem.json";

    void MmRender();
    void MmRegister();
    void Load();
    void Save();
}