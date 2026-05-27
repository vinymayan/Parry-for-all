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
    inline int playerReflectDamageType = 3; // 0 = Flat, 1 = %, 2 = Full, 3 = Skill Scaling
    inline float playerReflectValue = 50.0f;

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
    inline int npcReflectDamageType = 3;
    inline float npcReflectValue = 50.0f;

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
    inline bool npcParryPlayerEnabled = true;
    inline float slowTimeMultiplier = 0.3f;
    inline int slowTimeDurationMS = 200;

    // Parry Commitment
    inline bool playerParryCommitmentEnabled = false;
    inline int playerParryCommitmentMS = 500;
    inline int playerCommitmentReductionNormalMS = 200;
    inline int playerCommitmentReductionPerfectMS = 500;

    // Weapon to Weapon (W2W) Settings
    inline int w2wMode = 0; // 0 = Disabled, 1 = NPCs Only, 2 = All
    inline float w2wBaseChance = 10.0f;
    inline float w2wDamageMult = 1.0f;
    inline float w2wSkillMult = 1.0f;
    inline float w2wGlobalDifficulty = 100.0f;
    inline int w2wReactionTarget = 3; // 0 = Disabled, 1 = Attacker Only, 2 = Defender Only, 3 = Both
    inline int w2wReactionType = 0; // 0 = Recoil, 1 = Stagger

    struct NPCParryRule {
        std::string ruleName = "New Rule";
        RE::FormID perkID = 0;

        bool npcParryEnabled = true;
        int npcNormalParryMS = 400;
        int npcPerfectParryMS = 100;

        int npcArrowMode = 1;
        int npcArrowReflectMode = 1;
        CostType npcArrowCostType = CostType::None;

        int npcMagicMode = 1;
        int npcMagicReflectMode = 1;
        CostType npcMagicCostType = CostType::None;

        int npcReflectMeleeMode = 1;
        CostType npcReflectMeleeCostType = CostType::None;
        int npcReflectDamageType = 3;
        float npcReflectValue = 50.0f;

        int npcMeleeStagger = 1;
        int npcArrowStagger = 1;
        int npcMagicStagger = 1;

        int npcVisualMode = 1;
        int npcSoundMode = 1;
    };

    inline std::vector<NPCParryRule> npcRules;

    void PlayerMenu();
    void NPCMenu();
    void GeneralMenu();
    void MmRegister();
    void Load();
    void Save();
}