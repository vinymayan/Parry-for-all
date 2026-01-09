#pragma once
#include <Windows.h>
#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include <map>
#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/filewritestream.h"
#include "rapidjson/writer.h"
#include "SKSEMCP/SKSEMenuFramework.hpp"

namespace ParrySettings {
    enum class CostType {
        None = 0,
        Stamina = 1,
        Magicka = 2,
        Health = 3
    };

    // Player Timings
    inline int playerNormalParryMS = 300;
    inline int playerPerfectParryMS = 150;
    inline bool playerParryEnabled = true;

    // Player Advanced Options
    // Mode: 0 = Disabled, 1 = Free, 2 = Cost
    inline int playerArrowMode = 1;
    inline int playerArrowReflectMode = 1; // 0: Disable, 1: Enable, 2: Perfect
    inline CostType playerArrowCostType = CostType::None;

    inline int playerMagicMode = 1;
    inline int playerMagicReflectMode = 1; // 0: Disable, 1: Enable, 2: Perfect
    inline CostType playerMagicCostType = CostType::None;

    inline int playerReflectMeleeMode = 1; // 0: Disable, 1: Enable, 2: Perfect
    inline CostType playerReflectMeleeCostType = CostType::None;

    // NPC Timings
    inline bool npcParryEnabled = true;
    inline int npcNormalParryMS = 400;
    inline int npcPerfectParryMS = 100;

    // NPC Advanced Options
    inline int npcArrowMode = 1;
    inline int npcArrowReflectMode = 1;
    inline CostType npcArrowCostType = CostType::None;

    inline int npcMagicMode = 1;
    inline int npcMagicReflectMode = 1;
    inline CostType npcMagicCostType = CostType::None;

    inline int npcReflectMeleeMode = 1;
    inline CostType npcReflectMeleeCostType = CostType::None;

    // Player Effects
    inline int playerVisualMode = 1; 
    inline int playerSoundMode = 1;

    // NPC Effects
    inline int npcVisualMode = 1;
    inline int npcSoundMode = 1;

    // Stagger Options 
    inline int playerMeleeStagger = 1;
    inline int playerArrowStagger = 1;
    inline int playerMagicStagger = 1;

    inline int npcMeleeStagger = 1;
    inline int npcArrowStagger = 1;
    inline int npcMagicStagger = 1;

    // Slow Time
    inline bool npcParrySlowTime = true;
    inline float slowTimeMultiplier = 0.3f;
    inline int slowTimeDurationMS = 200;

    void PlayerMenu();
    void NPCMenu();
    void MmRegister();
    void Load();
    void Save();
}