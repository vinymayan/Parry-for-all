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

    enum class ValueSource {
        Flat = 0,
        Global = 1,
        ActorValue = 2
    };

    enum class RestoreMode {
        Flat = 0,
        Percentage = 1
    };

    struct ParryRestoreConfig {
        CostType resource = CostType::None;
        RestoreMode mode = RestoreMode::Flat;
        float value = 0.0f;
        RE::FormID valueGlobal = 0;
        ValueSource valueSource = ValueSource::Flat;
        std::string valueActorValue = "Stamina";
        RE::FormID perk = 0;
    };

    inline bool cancelParryOnBlockStop = true;
    inline bool bashOpensParryWindow = false;

    // Player Timings
    inline int playerNormalParryMS = 300;
    inline int playerPerfectParryMS = 150;
    inline bool playerParryEnabled = true;
    inline RE::FormID playerParryPerk = 0;
    inline RE::FormID playerDisableParryPerk = 0;
    inline RE::FormID playerNormalParryGlobal = 0;
    inline RE::FormID playerPerfectParryGlobal = 0;
    inline ValueSource playerNormalParrySource = ValueSource::Flat;
    inline ValueSource playerPerfectParrySource = ValueSource::Flat;
    inline std::string playerNormalParryActorValue = "Block";
    inline std::string playerPerfectParryActorValue = "Block";
    inline ParryRestoreConfig playerNormalRestore;
    inline ParryRestoreConfig playerPerfectRestore;

    // Player Advanced Options
    // Mode: 0 = Disabled, 1 = Free, 2 = Cost
    inline int playerArrowMode = 1;
    inline RE::FormID playerArrowParryPerk = 0;
    inline int playerArrowReflectMode = 1; // 0: Disable, 1: Enable, 2: Perfect
    inline int playerArrowReflectTriggers = 1; // 1: Block, 2: Bash, 4: Power Bash
    inline RE::FormID playerArrowReflectPerk = 0;
    inline CostType playerArrowCostType = CostType::None;
    inline float playerArrowCost = 0.0f;
    inline RE::FormID playerArrowCostGlobal = 0;
    inline ValueSource playerArrowCostSource = ValueSource::Flat;
    inline std::string playerArrowCostActorValue = "Stamina";

    inline int playerMagicMode = 1;
    inline RE::FormID playerMagicParryPerk = 0;
    inline int playerMagicReflectMode = 1; // 0: Disable, 1: Enable, 2: Perfect
    inline int playerMagicReflectTriggers = 1;
    inline RE::FormID playerMagicReflectPerk = 0;
    inline CostType playerMagicCostType = CostType::None;
    inline float playerMagicCost = 0.0f;
    inline RE::FormID playerMagicCostGlobal = 0;
    inline ValueSource playerMagicCostSource = ValueSource::Flat;
    inline std::string playerMagicCostActorValue = "Magicka";

    inline int playerReflectMeleeMode = 1; // 0: Disable, 1: Enable, 2: Perfect
    inline int playerMeleeReflectTriggers = 1;
    inline RE::FormID playerReflectMeleePerk = 0;
    inline CostType playerReflectMeleeCostType = CostType::None;
    inline float playerReflectMeleeCost = 0.0f;
    inline RE::FormID playerReflectMeleeCostGlobal = 0;
    inline ValueSource playerReflectMeleeCostSource = ValueSource::Flat;
    inline std::string playerReflectMeleeCostActorValue = "Stamina";
    inline int playerReflectDamageType = 3; // 0 = Flat, 1 = %, 2 = Full, 3 = Skill Scaling
    inline float playerReflectValue = 50.0f;
    inline RE::FormID playerReflectValueGlobal = 0;
    inline ValueSource playerReflectValueSource = ValueSource::Flat;
    inline std::string playerReflectValueActorValue = "Block";

    // NPC Timings
    inline bool npcParryEnabled = true;
    inline int npcNormalParryMS = 400;
    inline int npcPerfectParryMS = 100;
    inline RE::FormID npcParryPerk = 0;
    inline RE::FormID npcDisableParryPerk = 0;
    inline RE::FormID npcNormalParryGlobal = 0;
    inline RE::FormID npcPerfectParryGlobal = 0;
    inline ValueSource npcNormalParrySource = ValueSource::Flat;
    inline ValueSource npcPerfectParrySource = ValueSource::Flat;
    inline std::string npcNormalParryActorValue = "Block";
    inline std::string npcPerfectParryActorValue = "Block";
    inline ParryRestoreConfig npcNormalRestore;
    inline ParryRestoreConfig npcPerfectRestore;

    // NPC Advanced Options
    inline int npcArrowMode = 1;
    inline RE::FormID npcArrowParryPerk = 0;
    inline int npcArrowReflectMode = 1;
    inline int npcArrowReflectTriggers = 1;
    inline RE::FormID npcArrowReflectPerk = 0;
    inline CostType npcArrowCostType = CostType::None;
    inline float npcArrowCost = 0.0f;
    inline RE::FormID npcArrowCostGlobal = 0;
    inline ValueSource npcArrowCostSource = ValueSource::Flat;
    inline std::string npcArrowCostActorValue = "Stamina";

    inline int npcMagicMode = 1;
    inline RE::FormID npcMagicParryPerk = 0;
    inline int npcMagicReflectMode = 1;
    inline int npcMagicReflectTriggers = 1;
    inline RE::FormID npcMagicReflectPerk = 0;
    inline CostType npcMagicCostType = CostType::None;
    inline float npcMagicCost = 0.0f;
    inline RE::FormID npcMagicCostGlobal = 0;
    inline ValueSource npcMagicCostSource = ValueSource::Flat;
    inline std::string npcMagicCostActorValue = "Magicka";

    inline int npcReflectMeleeMode = 1;
    inline int npcMeleeReflectTriggers = 1;
    inline RE::FormID npcReflectMeleePerk = 0;
    inline CostType npcReflectMeleeCostType = CostType::None;
    inline float npcReflectMeleeCost = 0.0f;
    inline RE::FormID npcReflectMeleeCostGlobal = 0;
    inline ValueSource npcReflectMeleeCostSource = ValueSource::Flat;
    inline std::string npcReflectMeleeCostActorValue = "Stamina";
    inline int npcReflectDamageType = 3;
    inline float npcReflectValue = 50.0f;
    inline RE::FormID npcReflectValueGlobal = 0;
    inline ValueSource npcReflectValueSource = ValueSource::Flat;
    inline std::string npcReflectValueActorValue = "Block";

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
    inline int playerMeleeStaggerSize = 1;
    inline int playerArrowStaggerSize = 1;
    inline int playerMagicStaggerSize = 1;
    inline RE::FormID playerMeleeStaggerPerk = 0;
    inline RE::FormID playerArrowStaggerPerk = 0;
    inline RE::FormID playerMagicStaggerPerk = 0;

    inline int npcMeleeStagger = 1;
    inline int npcArrowStagger = 1;
    inline int npcMagicStagger = 1;
    inline int npcMeleeStaggerSize = 1;
    inline int npcArrowStaggerSize = 1;
    inline int npcMagicStaggerSize = 1;
    inline RE::FormID npcMeleeStaggerPerk = 0;
    inline RE::FormID npcArrowStaggerPerk = 0;
    inline RE::FormID npcMagicStaggerPerk = 0;

    // Slow Time
    inline bool npcParrySlowTime = true;
    inline bool npcParryPlayerEnabled = true;
    inline float slowTimeMultiplier = 0.3f;
    inline int slowTimeDurationMS = 200;
    inline RE::FormID slowTimeMultiplierGlobal = 0;
    inline RE::FormID slowTimeDurationGlobal = 0;
    inline ValueSource slowTimeMultiplierSource = ValueSource::Flat;
    inline ValueSource slowTimeDurationSource = ValueSource::Flat;
    inline std::string slowTimeMultiplierActorValue = "SpeedMult";
    inline std::string slowTimeDurationActorValue = "Block";

    // Parry Commitment
    inline bool playerParryCommitmentEnabled = false;
    inline int playerParryCommitmentMS = 500;
    inline int playerCommitmentReductionNormalMS = 200;
    inline int playerCommitmentReductionPerfectMS = 500;
    inline RE::FormID playerParryCommitmentGlobal = 0;
    inline RE::FormID playerCommitmentReductionNormalGlobal = 0;
    inline RE::FormID playerCommitmentReductionPerfectGlobal = 0;
    inline ValueSource playerParryCommitmentSource = ValueSource::Flat;
    inline ValueSource playerCommitmentReductionNormalSource = ValueSource::Flat;
    inline ValueSource playerCommitmentReductionPerfectSource = ValueSource::Flat;
    inline std::string playerParryCommitmentActorValue = "Block";
    inline std::string playerCommitmentReductionNormalActorValue = "Block";
    inline std::string playerCommitmentReductionPerfectActorValue = "Block";

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
        RE::FormID npcNormalParryGlobal = 0;
        RE::FormID npcPerfectParryGlobal = 0;
        ValueSource npcNormalParrySource = ValueSource::Flat;
        ValueSource npcPerfectParrySource = ValueSource::Flat;
        std::string npcNormalParryActorValue = "Block";
        std::string npcPerfectParryActorValue = "Block";
        ParryRestoreConfig npcNormalRestore;
        ParryRestoreConfig npcPerfectRestore;

        int npcArrowMode = 1;
        int npcArrowReflectMode = 1;
        int npcArrowReflectTriggers = 1;
        CostType npcArrowCostType = CostType::None;
        float npcArrowCost = 0.0f;
        RE::FormID npcArrowCostGlobal = 0;
        ValueSource npcArrowCostSource = ValueSource::Flat;
        std::string npcArrowCostActorValue = "Stamina";

        int npcMagicMode = 1;
        int npcMagicReflectMode = 1;
        int npcMagicReflectTriggers = 1;
        CostType npcMagicCostType = CostType::None;
        float npcMagicCost = 0.0f;
        RE::FormID npcMagicCostGlobal = 0;
        ValueSource npcMagicCostSource = ValueSource::Flat;
        std::string npcMagicCostActorValue = "Magicka";

        int npcReflectMeleeMode = 1;
        int npcMeleeReflectTriggers = 1;
        CostType npcReflectMeleeCostType = CostType::None;
        float npcReflectMeleeCost = 0.0f;
        RE::FormID npcReflectMeleeCostGlobal = 0;
        ValueSource npcReflectMeleeCostSource = ValueSource::Flat;
        std::string npcReflectMeleeCostActorValue = "Stamina";
        int npcReflectDamageType = 3;
        float npcReflectValue = 50.0f;
        RE::FormID npcReflectValueGlobal = 0;
        ValueSource npcReflectValueSource = ValueSource::Flat;
        std::string npcReflectValueActorValue = "Block";

        int npcMeleeStagger = 1;
        int npcArrowStagger = 1;
        int npcMagicStagger = 1;
        int npcMeleeStaggerSize = 1;
        int npcArrowStaggerSize = 1;
        int npcMagicStaggerSize = 1;

        int npcVisualMode = 1;
        int npcSoundMode = 1;
    };

    inline std::vector<NPCParryRule> npcRules;

    void PlayerMenu();
    void NPCMenu();
    void RulesMenu();
    void GeneralMenu();
    void MmRegister();
    void Load();
    void Save();
    bool HasRequiredPerk(RE::Actor* a_actor, RE::FormID a_perkID);
    bool CanActorUseParry(RE::Actor* a_actor);
    void ApplyParryRestore(RE::Actor* a_actor, bool a_perfect);
    void GetParryWindowsForActor(RE::Actor* a_actor, int& a_normalMS, int& a_perfectMS);
    float ResolveParryCost(float a_localCost, RE::FormID a_costGlobal);
    float ResolveConfigValue(float a_flatValue, ValueSource a_source, RE::FormID a_globalID, const std::string& a_actorValue, RE::Actor* a_actor);
    int ResolveConfigInt(int a_flatValue, ValueSource a_source, RE::FormID a_globalID, const std::string& a_actorValue, RE::Actor* a_actor, int a_minValue = 0);
    float ResolveParryCost(float a_localCost, ValueSource a_source, RE::FormID a_costGlobal, const std::string& a_actorValue, RE::Actor* a_actor);
    float GetStaggerMagnitude(int a_size, bool a_perfect);
}
