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
    inline int playerNormalParryMS = 500;
    inline int playerPerfectParryMS = 150;
    inline bool playerParryEnabled = true;

    // Player Advanced Options
    // Mode: 0 = Disabled, 1 = Free, 2 = Cost
    inline int playerArrowMode = 0;
    inline int playerArrowReflectMode = 0; // 0: Disable, 1: Enable, 2: Perfect
    inline CostType playerArrowCostType = CostType::None;

    inline int playerMagicMode = 0;
    inline int playerMagicReflectMode = 0; // 0: Disable, 1: Enable, 2: Perfect
    inline CostType playerMagicCostType = CostType::None;

    inline int playerReflectMeleeMode = 0; // 0: Disable, 1: Enable, 2: Perfect
    inline CostType playerReflectMeleeCostType = CostType::None;

    // NPC Timings
    inline bool npcParryEnabled = true;
    inline int npcNormalParryMS = 400;
    inline int npcPerfectParryMS = 100;

    // NPC Advanced Options
    inline int npcArrowMode = 0;
    inline int npcArrowReflectMode = 0;
    inline CostType npcArrowCostType = CostType::None;

    inline int npcMagicMode = 0;
    inline int npcMagicReflectMode = 0;
    inline CostType npcMagicCostType = CostType::None;

    inline int npcReflectMeleeMode = 0;
    inline CostType npcReflectMeleeCostType = CostType::None;

    // Stagger Options 
    inline bool playerMeleeStagger = true;
    inline bool playerArrowStagger = false;
    inline bool playerMagicStagger = false;

    inline bool npcMeleeStagger = true;
    inline bool npcArrowStagger = false;
    inline bool npcMagicStagger = false;

    // Slow Time
    inline bool npcParrySlowTime = true;
    inline float slowTimeMultiplier = 0.2f;
    inline int slowTimeDurationMS = 1000;

    void MmRender();
    void MmRegister();
    void Load();
    void Save();
}