#include "Events.h"
#include "Settings.h"
#include "DelayedDispatcher.h"
#include "ClibUtil/editorID.hpp"
#include <array>
#include <unordered_set>

RE::TESIdleForm* anim = nullptr;
RE::TESEffectShader* parry = nullptr;
RE::TESEffectShader* PerfParry = nullptr;

namespace {
    std::mutex g_slowTimeMutex;
    std::uint64_t g_slowTimeGeneration = 0;
    bool g_slowTimeActive = false;
    std::mutex g_parrySoundMutex;
    std::unordered_map<RE::FormID, RE::BSSoundHandle> g_parrySoundHandles;
    std::shared_mutex g_parryFormsMutex;
    std::mutex g_visualWarningMutex;
    std::unordered_set<std::string> g_visualWarnings;

    struct VisualEffect {
        RE::BGSExplosion* form;
        std::string_view editorID;
    };

    void WarnVisualOnce(std::string a_key, std::string a_message)
    {
        std::scoped_lock lock(g_visualWarningMutex);
        if (g_visualWarnings.insert(std::move(a_key)).second) {
            logger::warn("{}", a_message);
        }
    }

    bool HasEquippedShield(RE::Actor* a_actor)
    {
        if (!a_actor) {
            return false;
        }

        auto* leftHandObject = a_actor->GetEquippedObject(true);
        if (!leftHandObject) {
            return false;
        }

        auto* armor = leftHandObject->As<RE::TESObjectARMO>();
        return armor && armor->IsShield();
    }

    bool UsesLeftHandVisualAnchor(RE::Actor* a_actor)
    {
        if (HasEquippedShield(a_actor)) {
            return true;
        }

        // Dual-wield and left-hand-only loadouts should anchor the visual to the
        // hand that performs the modded block instead of always using WEAPON.
        auto* leftHandObject = a_actor ? a_actor->GetEquippedObject(true) : nullptr;
        return leftHandObject && leftHandObject->IsWeapon();
    }

    bool ResolveParryVisualNode(RE::Actor* a_actor, bool a_preferLeftHand, RE::BSFixedString& a_nodeName)
    {
        auto* root = a_actor ? a_actor->Get3D() : nullptr;
        if (!root) {
            return false;
        }

        constexpr std::array<std::string_view, 4> leftCandidates{
            "SHIELD", "NPC L Hand [LHnd]", "WEAPON", "NPC R Hand [RHnd]"
        };
        constexpr std::array<std::string_view, 4> rightCandidates{
            "WEAPON", "NPC R Hand [RHnd]", "SHIELD", "NPC L Hand [LHnd]"
        };
        const auto& candidates = a_preferLeftHand ? leftCandidates : rightCandidates;

        for (const auto candidate : candidates) {
            RE::BSFixedString nodeName(candidate);
            if (root->GetObjectByName(nodeName)) {
                a_nodeName = std::move(nodeName);
                return true;
            }
        }

        return false;
    }

    std::array<VisualEffect, 3> SelectParryVisuals(bool a_isPlayer, bool a_leftHand, Sink::ParryType a_type)
    {
        const bool perfect = a_type == Sink::ParryType::Perfect;

        if (a_isPlayer) {
            if (a_leftHand) {
                return perfect ?
                    std::array<VisualEffect, 3>{ {
                        { Sink::ParryPPlayerS, "ParryPerfectPlayerShield" },
                        { Sink::ParryPPlayerS2, "ParryPerfectPlayerShield2" },
                        { Sink::ParryPPlayerS3, "ParryPerfectPlayerShield3" } } } :
                    std::array<VisualEffect, 3>{ {
                        { Sink::ParryNPlayerS, "ParryNormalPlayerShield" },
                        { Sink::ParryNPlayerS2, "ParryNormalPlayerShield2" },
                        { Sink::ParryNPlayerS3, "ParryNormalPlayerShield3" } } };
            }

            return perfect ?
                std::array<VisualEffect, 3>{ {
                    { Sink::ParryPPlayer, "ParryPerfectPlayer" },
                    { Sink::ParryPPlayer2, "ParryPerfectPlayer2" },
                    { Sink::ParryPPlayer3, "ParryPerfectPlayer3" } } } :
                std::array<VisualEffect, 3>{ {
                    { Sink::ParryNPlayer, "ParryNormalPlayer" },
                    { Sink::ParryNPlayer2, "ParryNormalPlayer2" },
                    { Sink::ParryNPlayer3, "ParryNormalPlayer3" } } };
        }

        if (a_leftHand) {
            return perfect ?
                std::array<VisualEffect, 3>{ {
                    { Sink::ParryPNPCS, "ParryPerfectNPCShield" },
                    { Sink::ParryPNPCS2, "ParryPerfectNPCShield2" },
                    { Sink::ParryPNPCS3, "ParryPerfectNPCShield3" } } } :
                std::array<VisualEffect, 3>{ {
                    { Sink::ParryNNPCS, "ParryNormalNPCShield" },
                    { Sink::ParryNNPCS2, "ParryNormalNPCShield2" },
                    { Sink::ParryNNPCS3, "ParryNormalNPCShield3" } } };
        }

        return perfect ?
            std::array<VisualEffect, 3>{ {
                { Sink::ParryPNPC, "ParryPerfectNPC" },
                { Sink::ParryPNPC2, "ParryPerfectNPC2" },
                { Sink::ParryPNPC3, "ParryPerfectNPC3" } } } :
            std::array<VisualEffect, 3>{ {
                { Sink::ParryNNPC, "ParryNormalNPC" },
                { Sink::ParryNNPC2, "ParryNormalNPC2" },
                { Sink::ParryNNPC3, "ParryNormalNPC3" } } };
    }

    bool SpawnAndAttachParryEffect(
        RE::Actor* a_blocker,
        const VisualEffect& a_effect,
        const RE::BSFixedString& a_nodeName)
    {
        if (!a_blocker || !a_effect.form) {
            WarnVisualOnce(
                std::format("missing:{}", a_effect.editorID),
                std::format("VFX de parry '{}' indisponível; o efeito será ignorado com segurança.", a_effect.editorID));
            return false;
        }

        auto spawned = a_blocker->PlaceObjectAtMe(a_effect.form, false);
        if (!spawned) {
            WarnVisualOnce(
                std::format("spawn:{}", a_effect.editorID),
                std::format(
                    "PlaceObjectAtMe falhou para o VFX '{}' no ator {:08X}; o efeito será ignorado com segurança.",
                    a_effect.editorID,
                    a_blocker->GetFormID()));
            return false;
        }

        if (!spawned->MoveToNode(a_blocker, a_nodeName)) {
            WarnVisualOnce(
                std::format("node:{}:{}", a_effect.editorID, a_nodeName.c_str()),
                std::format(
                    "Não foi possível anexar o VFX '{}' ao node '{}'; o efeito permanecerá sem ancoragem.",
                    a_effect.editorID,
                    a_nodeName.c_str()));
            return false;
        }

        return true;
    }

    template <class T>
    T* LookupFormByEditorID(std::string_view editorID)
    {
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler || editorID.empty()) {
            return nullptr;
        }

        const auto& forms = dataHandler->GetFormArray(T::FORMTYPE);
        for (auto* rawForm : forms) {
            if (!rawForm || rawForm->IsDeleted() || rawForm->IsIgnored()) {
                continue;
            }

            auto* form = rawForm->As<T>();
            if (!form) {
                continue;
            }

            try {
                if (clib_util::editorID::get_editorID(rawForm) == editorID) {
                    logger::info(
                        "Form '{}' resolvido via CLibUtil: FormID={:08X}, FormType={}",
                        editorID,
                        rawForm->GetFormID(),
                        static_cast<std::uint32_t>(rawForm->GetFormType()));
                    return form;
                }
            } catch (const std::exception& e) {
                logger::warn(
                    "Falha ao consultar o EditorID via CLibUtil para o FormID {:08X}: {}",
                    rawForm->GetFormID(),
                    e.what());
            }
        }

        logger::warn("Form '{}' não encontrado via CLibUtil.", editorID);
        return nullptr;
    }

    bool PlaySoundOnActor(RE::Actor* actor, RE::BGSSoundDescriptorForm* sound)
    {
        if (!actor || !sound) {
            return false;
        }

        std::scoped_lock lock(g_parrySoundMutex);

        for (auto it = g_parrySoundHandles.begin(); it != g_parrySoundHandles.end();) {
            if (it->first != actor->GetFormID() &&
                (!it->second.IsValid() || !it->second.IsPlaying())) {
                it = g_parrySoundHandles.erase(it);
            } else {
                ++it;
            }
        }

        auto& handle = g_parrySoundHandles[actor->GetFormID()];
        if (handle.IsValid()) {
            handle.Stop();
        }
        handle = RE::BSSoundHandle{};

        auto* audio = RE::BSAudioManager::GetSingleton();
        if (!audio) {
            logger::warn("BSAudioManager indisponível ao tocar o som {:08X}.", sound->GetFormID());
            g_parrySoundHandles.erase(actor->GetFormID());
            return false;
        }

        if (!audio->GetSoundHandle(handle, sound)) {
            logger::warn(
                "GetSoundHandle falhou: SoundDescriptor={:08X}, Actor={:08X}.",
                sound->GetFormID(),
                actor->GetFormID());
            g_parrySoundHandles.erase(actor->GetFormID());
            return false;
        }

        if (auto* node = actor->Get3D()) {
            handle.SetObjectToFollow(node);
        } else {
            handle.SetPosition(actor->GetPosition());
        }
        if (!handle.Play()) {
            logger::warn(
                "BSSoundHandle::Play falhou: SoundDescriptor={:08X}, Actor={:08X}, HandleValid={}.",
                sound->GetFormID(),
                actor->GetFormID(),
                handle.IsValid());
            g_parrySoundHandles.erase(actor->GetFormID());
            return false;
        }
        logger::debug(
            "Som de parry iniciado: SoundDescriptor={:08X}, Actor={:08X}, HandleValid={}, Playing={}.",
            sound->GetFormID(),
            actor->GetFormID(),
            handle.IsValid(),
            handle.IsPlaying());
        return true;
    }

    bool ValidateSoundDescriptor(std::string_view editorID, RE::BGSSoundDescriptorForm* sound)
    {
        if (!sound || !sound->soundDescriptor) {
            logger::warn("SoundDescriptor '{}' não possui definição de som runtime.", editorID);
            return false;
        }

        auto* soundDef = static_cast<RE::BGSStandardSoundDef*>(sound->soundDescriptor);
        const bool ready = !soundDef->soundFiles.empty();
        logger::info(
            "SoundDescriptor '{}': FormID={:08X}, arquivos={}, OutputModel={}, pronto={}.",
            editorID,
            sound->GetFormID(),
            soundDef->soundFiles.size(),
            fmt::ptr(soundDef->outputModel),
            ready);
        return ready;
    }

}

void Sink::ParryTimerManager::CleanupExpiredWindows() {
    std::unique_lock lock(g_parryMutex);
    if (g_parryWindows.empty() && g_parryCommitments.empty()) return;

    auto now = std::chrono::steady_clock::now();

    for (auto it = g_parryWindows.begin(); it != g_parryWindows.end(); ) {
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second).count();
        if (duration > 2000) {
            auto* actor = RE::TESForm::LookupByID<RE::Actor>(it->first);
            if (actor) {
                actor->SetGraphVariableBool("isParryingCMF", false);
                actor->SetGraphVariableBool("isPerfParryingCMF", false);
            }
            g_parryTriggers.erase(it->first);
            it = g_parryWindows.erase(it);
        }
        else {
            ++it;
        }
    }

    // Limpa os commitments expirados
    for (auto it = g_parryCommitments.begin(); it != g_parryCommitments.end(); ) {
        if (now > it->second) {
            it = g_parryCommitments.erase(it);
        }
        else {
            ++it;
        }
    }
}



RE::BSEventNotifyControl Sink::NpcCombatTracker::ProcessEvent(const RE::TESCombatEvent* a_event, RE::BSTEventSource<RE::TESCombatEvent>*)
{
    if (!a_event || !a_event->actor) {
        return RE::BSEventNotifyControl::kContinue;
    }

    auto actor = a_event->actor.get();
    auto* npc = actor->As<RE::Actor>();
    if (npc && npc != RE::PlayerCharacter::GetSingleton()) {  
        switch (a_event->newState.get()) {
        case RE::ACTOR_COMBAT_STATE::kCombat:
            NpcCombatTracker::RegisterSink(npc);
            break;
        case RE::ACTOR_COMBAT_STATE::kNone:
            NpcCombatTracker::UnregisterSink(npc);
            break;
        }
    }
    return RE::BSEventNotifyControl::kContinue;
}

void Sink::NpcCombatTracker::RegisterSink(RE::Actor* a_actor)
{
    std::unique_lock lock(g_mutex);
    if (g_trackedNPCs.find(a_actor->GetFormID()) == g_trackedNPCs.end()) {
        a_actor->AddAnimationGraphEventSink(&g_npcSink);
        g_trackedNPCs.insert(a_actor->GetFormID());
        //SKSE::log::info("[NpcCombatTracker] Começando a rastrear animações do ator {:08X}", a_actor->GetFormID());
    }
}

void Sink::NpcCombatTracker::UnregisterSink(RE::Actor* a_actor)
{
    if (!a_actor || a_actor->IsPlayerRef()) return;

    std::unique_lock lock(g_mutex);
    if (g_trackedNPCs.find(a_actor->GetFormID()) != g_trackedNPCs.end()) {
        a_actor->RemoveAnimationGraphEventSink(&g_npcSink);
        g_trackedNPCs.erase(a_actor->GetFormID());
        //SKSE::log::info("[NpcCombatTracker] Parando de rastrear animações do ator {:08X}", a_actor->GetFormID());
    }
}

void Sink::NpcCombatTracker::RegisterSinksForExistingCombatants()
{
    SKSE::log::info("[NpcCombatTracker] Verificando NPCs já em combate após carregar o jogo...");

    auto* processLists = RE::ProcessLists::GetSingleton();
    if (!processLists) {
        SKSE::log::warn("[NpcCombatTracker] Não foi possível obter ProcessLists.");
        return;
    }

    // Itera sobre todos os atores que estão "ativos" no jogo
    for (auto& actorHandle : processLists->highActorHandles) {
        if (auto actor = actorHandle.get().get()) {
            // A função IsInCombat() nos diz se o ator já está em um estado de combate
            if (!actor->IsPlayerRef()) {
                if (actor->IsInCombat()) {
                    SKSE::log::info("[NpcCombatTracker] Ator '{}' ({:08X}) já está em combate. Registrando sink...",
                        actor->GetName(), actor->GetFormID());
                    // Usamos a mesma função de registro que já existe!
                    RegisterSink(actor);
                }
            }

        }
    }

    SKSE::log::info("[NpcCombatTracker] Verificação concluída.");
}

RE::BSEventNotifyControl Sink::NpcCycleSink::ProcessEvent(const RE::BSAnimationGraphEvent* a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>*)
{
    if (!a_event || !a_event->holder) return RE::BSEventNotifyControl::kContinue;

    auto* actor = a_event->holder->As<RE::Actor>();
    if (!actor || actor->IsDead()) return RE::BSEventNotifyControl::kContinue;

    const RE::FormID formID = actor->GetFormID();
    const std::string_view eventName = a_event->tag;
    bool isPlayer = actor->IsPlayerRef();
    auto npc = const_cast<RE::Actor*>(actor);
    const bool blockWindowStart = !ParrySettings::bashOpensParryWindow &&
        (eventName == "SBF_BlockStart" || eventName == "MagicBlockStartCMF");
    const bool bashWindowStart = ParrySettings::bashOpensParryWindow &&
        (eventName == "SBF_BashStart" || eventName == "SBF_BashIntroStart");
    const bool powerBashWindowStart = ParrySettings::bashOpensParryWindow && eventName == "SBF_BashPowerStart";

    if (blockWindowStart || bashWindowStart || powerBashWindowStart) {
        if (ParrySettings::CanActorUseParry(npc)) {
            const auto trigger = powerBashWindowStart ? ParryTrigger::PowerBash :
                (bashWindowStart ? ParryTrigger::Bash : ParryTrigger::Block);
            ParryTimerManager::StartWindow(formID, isPlayer, trigger);
        }
    }
    else if (ParrySettings::cancelParryOnBlockStop &&
        ((!ParrySettings::bashOpensParryWindow && eventName == "SBF_BlockStop") ||
         (ParrySettings::bashOpensParryWindow && eventName == "SBF_BashStop"))) {
        ParryTimerManager::RemoveWindow(formID);
        npc->SetGraphVariableBool("isParryingCMF", false);
        npc->SetGraphVariableBool("isPerfParryingCMF", false);
        npc->SetGraphVariableBool("isPerfectParryingCMF", false);
    }
    else if (eventName == "ParryStartCMF") {
        npc->SetGraphVariableBool("isParryingCMF", true);
    }
    else if (eventName == "ParryEndCMF") {
        npc->SetGraphVariableBool("isParryingCMF", false);
    }
    else if (eventName == "PerfParryStartCMF") {
        npc->SetGraphVariableBool("isPerfParryingCMF", true);
    }
    else if (eventName == "PerfParryEndCMF") {
        npc->SetGraphVariableBool("isPerfParryingCMF", false);
    }

    return RE::BSEventNotifyControl::kContinue;
}

void Sink::ParryTimerManager::StartWindow(RE::FormID a_formID, bool a_isPlayer, ParryTrigger a_trigger) {
    std::unique_lock lock(g_parryMutex);
    auto now = std::chrono::steady_clock::now();

    // A single bash animation can emit Intro, BashStart and PowerStart in sequence.
    // Preserve the original window and only refine its trigger to Power Bash.
    if (auto windowIt = g_parryWindows.find(a_formID); windowIt != g_parryWindows.end()) {
        auto triggerIt = g_parryTriggers.find(a_formID);
        if (triggerIt != g_parryTriggers.end() && triggerIt->second == ParryTrigger::Bash &&
            (a_trigger == ParryTrigger::Bash || a_trigger == ParryTrigger::PowerBash)) {
            if (a_trigger == ParryTrigger::PowerBash) {
                triggerIt->second = ParryTrigger::PowerBash;
            }
            return;
        }
    }

    if (a_isPlayer && ParrySettings::playerParryCommitmentEnabled) {
        auto it = g_parryCommitments.find(a_formID);
        if (it != g_parryCommitments.end()) {
            if (now < it->second) {
                return;
            }
        }
        auto actor = RE::TESForm::LookupByID<RE::Actor>(a_formID);
        int commitmentMS = ParrySettings::ResolveConfigInt(
            ParrySettings::playerParryCommitmentMS,
            ParrySettings::playerParryCommitmentSource,
            ParrySettings::playerParryCommitmentGlobal,
            ParrySettings::playerParryCommitmentActorValue,
            actor,
            1);
        g_parryCommitments[a_formID] = now + std::chrono::milliseconds(commitmentMS);
    }

    g_parryWindows[a_formID] = now;
    g_parryTriggers[a_formID] = a_trigger;

    if (auto actor = RE::TESForm::LookupByID<RE::Actor>(a_formID)) {
        int perfectMS = 100;
        int normalMS = 400;
        ParrySettings::GetParryWindowsForActor(actor, normalMS, perfectMS);

        // Ativa ambas as janelas internas no grafo de animação
        actor->SetGraphVariableBool("isParryingCMF", true);
        actor->SetGraphVariableBool("isPerfParryingCMF", true);

        auto actorHandle = actor->CreateRefHandle();

        // 1. Agendamento reativo para encerrar a janela PERFEITA
        Utils::DelayedDispatcher::Get().PostDelayed(std::chrono::milliseconds(perfectMS), [actorHandle, a_formID, now]() {
            SKSE::GetTaskInterface()->AddTask([actorHandle, a_formID, now]() {
                {
                    std::shared_lock lock(g_parryMutex);
                    auto it = g_parryWindows.find(a_formID);
                    if (it != g_parryWindows.end() && it->second != now) {
                        return; // Sobrescrito por uma janela mais nova
                    }
                }
                if (actorHandle && actorHandle.get()) {
                    actorHandle.get()->SetGraphVariableBool("isPerfParryingCMF", false);
                }
                });
            });

        // 2. Agendamento reativo para encerrar a janela NORMAL e limpar mapa
        Utils::DelayedDispatcher::Get().PostDelayed(std::chrono::milliseconds(normalMS), [actorHandle, a_formID, now]() {
            SKSE::GetTaskInterface()->AddTask([actorHandle, a_formID, now]() {
                {
                    std::unique_lock lock(g_parryMutex);
                    auto it = g_parryWindows.find(a_formID);
                    if (it != g_parryWindows.end() && it->second == now) {
                        g_parryTriggers.erase(a_formID);
                        g_parryWindows.erase(it);
                    }
                    else {
                        return;
                    }
                }
                if (actorHandle && actorHandle.get()) {
                    actorHandle.get()->SetGraphVariableBool("isParryingCMF", false);
                }
                });
            });
    }
}

void Sink::ParryTimerManager::ReduceCommitment(RE::FormID a_formID, int a_reductionMS) {
    std::unique_lock lock(g_parryMutex);
    auto it = g_parryCommitments.find(a_formID);
    if (it != g_parryCommitments.end()) {
        it->second -= std::chrono::milliseconds(a_reductionMS);
    }
}

Sink::ParryType Sink::ParryTimerManager::GetParryType(RE::FormID a_formID) {
    std::chrono::steady_clock::time_point startTime;
    auto* actor = RE::TESForm::LookupByID<RE::Actor>(a_formID);

    {
        std::shared_lock lock(g_parryMutex);
        auto it = g_parryWindows.find(a_formID);
        if (it == g_parryWindows.end()) return ParryType::None;
        startTime = it->second;
    }
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();

    
    if (!actor) return ParryType::None;

    int perfectMS = 100;
    int normalMS = 400;
    ParrySettings::GetParryWindowsForActor(actor, normalMS, perfectMS);

    // Apenas validamos o tempo. Se for maior que a janela, o parry "não existe"
    if (duration <= perfectMS) return ParryType::Perfect;
    if (duration <= normalMS) return ParryType::Normal;

    return ParryType::None;
}

Sink::ParryTrigger Sink::ParryTimerManager::GetParryTrigger(RE::FormID a_formID) {
    std::shared_lock lock(g_parryMutex);
    auto it = g_parryTriggers.find(a_formID);
    return it != g_parryTriggers.end() ? it->second : ParryTrigger::Block;
}

void Sink::ParryTimerManager::RemoveWindow(RE::FormID a_formID) {
    std::unique_lock lock(g_parryMutex);
    g_parryWindows.erase(a_formID);
    g_parryTriggers.erase(a_formID);
}

void Sink::InitializeForms() {
    struct VisualFormTarget {
        RE::BGSExplosion** destination;
        std::string_view editorID;
    };

    const std::array visualTargets{
        VisualFormTarget{ &ParryNPlayer, "ParryNormalPlayer" },
        VisualFormTarget{ &ParryNPlayer2, "ParryNormalPlayer2" },
        VisualFormTarget{ &ParryNPlayer3, "ParryNormalPlayer3" },
        VisualFormTarget{ &ParryNPlayerS, "ParryNormalPlayerShield" },
        VisualFormTarget{ &ParryNPlayerS2, "ParryNormalPlayerShield2" },
        VisualFormTarget{ &ParryNPlayerS3, "ParryNormalPlayerShield3" },
        VisualFormTarget{ &ParryPPlayer, "ParryPerfectPlayer" },
        VisualFormTarget{ &ParryPPlayer2, "ParryPerfectPlayer2" },
        VisualFormTarget{ &ParryPPlayer3, "ParryPerfectPlayer3" },
        VisualFormTarget{ &ParryPPlayerS, "ParryPerfectPlayerShield" },
        VisualFormTarget{ &ParryPPlayerS2, "ParryPerfectPlayerShield2" },
        VisualFormTarget{ &ParryPPlayerS3, "ParryPerfectPlayerShield3" },
        VisualFormTarget{ &ParryNNPC, "ParryNormalNPC" },
        VisualFormTarget{ &ParryNNPC2, "ParryNormalNPC2" },
        VisualFormTarget{ &ParryNNPC3, "ParryNormalNPC3" },
        VisualFormTarget{ &ParryNNPCS, "ParryNormalNPCShield" },
        VisualFormTarget{ &ParryNNPCS2, "ParryNormalNPCShield2" },
        VisualFormTarget{ &ParryNNPCS3, "ParryNormalNPCShield3" },
        VisualFormTarget{ &ParryPNPC, "ParryPerfectNPC" },
        VisualFormTarget{ &ParryPNPC2, "ParryPerfectNPC2" },
        VisualFormTarget{ &ParryPNPC3, "ParryPerfectNPC3" },
        VisualFormTarget{ &ParryPNPCS, "ParryPerfectNPCShield" },
        VisualFormTarget{ &ParryPNPCS2, "ParryPerfectNPCShield2" },
        VisualFormTarget{ &ParryPNPCS3, "ParryPerfectNPCShield3" }
    };

    std::array<RE::BGSExplosion*, visualTargets.size()> resolvedVisuals{};
    std::size_t resolvedVisualCount = 0;
    for (std::size_t i = 0; i < visualTargets.size(); ++i) {
        resolvedVisuals[i] = LookupFormByEditorID<RE::BGSExplosion>(visualTargets[i].editorID);
        if (resolvedVisuals[i]) {
            ++resolvedVisualCount;
        } else {
            logger::critical("VFX de parry obrigatório '{}' não foi resolvido.", visualTargets[i].editorID);
        }
    }

    auto* normalSound = LookupFormByEditorID<RE::BGSSoundDescriptorForm>("ParryNormalSound");
    auto* normalShieldSound = LookupFormByEditorID<RE::BGSSoundDescriptorForm>("ParryNormalSoundShield");
    auto* perfectSound = LookupFormByEditorID<RE::BGSSoundDescriptorForm>("ParryPerfectSound");
    auto* perfectShieldSound = LookupFormByEditorID<RE::BGSSoundDescriptorForm>("ParryPerfectSoundShield");

    {
        std::unique_lock lock(g_parryFormsMutex);
        for (std::size_t i = 0; i < visualTargets.size(); ++i) {
            // DFG updates may temporarily omit a form. Keep a previously valid
            // game-owned form instead of publishing a transient nullptr.
            if (resolvedVisuals[i] || !*visualTargets[i].destination) {
                *visualTargets[i].destination = resolvedVisuals[i];
            }
        }
        if (normalSound || !ParryNormalSound) ParryNormalSound = normalSound;
        if (normalShieldSound || !ParryNormalShieldSound) ParryNormalShieldSound = normalShieldSound;
        if (perfectSound || !ParryPerfectSound) ParryPerfectSound = perfectSound;
        if (perfectShieldSound || !ParryPerfectShieldSound) ParryPerfectShieldSound = perfectShieldSound;
    }

    bool soundsReady = true;
    soundsReady &= ValidateSoundDescriptor("ParryNormalSound", normalSound);
    soundsReady &= ValidateSoundDescriptor("ParryNormalSoundShield", normalShieldSound);
    soundsReady &= ValidateSoundDescriptor("ParryPerfectSound", perfectSound);
    soundsReady &= ValidateSoundDescriptor("ParryPerfectSoundShield", perfectShieldSound);

    if (resolvedVisualCount == visualTargets.size()) {
        SKSE::log::info("Todos os {} VFX de parry foram carregados com sucesso.", resolvedVisualCount);
    } else {
        SKSE::log::critical(
            "Somente {}/{} VFX de parry foram resolvidos. Os ausentes serão ignorados com segurança.",
            resolvedVisualCount,
            visualTargets.size());
    }
    if (!soundsReady) {
        SKSE::log::critical("FALHA: um ou mais sound descriptors do Parry não estão prontos para reprodução.");
    }
    else {
        SKSE::log::info("Todos os sound descriptors do Parry foram resolvidos via CLibUtil.");
    }
}

RE::TESEffectShader* Sink::GetEffectShaderByFormID(RE::FormID a_formID, const std::string& a_pluginName) {
    auto* dataHandler = RE::TESDataHandler::GetSingleton();
    auto* lookupForm = dataHandler ? dataHandler->LookupForm(a_formID, a_pluginName) : nullptr;

    // 0x55 na sua lista é EffectShader (TESEffectShader)
    if (lookupForm && lookupForm->GetFormType() == RE::FormType::EffectShader) {
        return static_cast<RE::TESEffectShader*>(lookupForm);
    }

    SKSE::log::warn("Não foi possível encontrar EffectShader 0x{:X} no plugin {}", a_formID, a_pluginName);
    return nullptr;
}

void Sink::TriggerSlowTime(float a_multiplier, int a_durationMS)
{
    auto* timer = RE::BSTimer::GetSingleton();
    if (!timer) {
        return;
    }

    std::uint64_t generation;
    {
        std::scoped_lock lock(g_slowTimeMutex);
        g_slowTimeActive = true;
        generation = ++g_slowTimeGeneration;
        timer->SetGlobalTimeMultiplier(std::clamp(a_multiplier, 0.02f, 1.0f), true);
    }

    Utils::DelayedDispatcher::Get().PostDelayed(
        std::chrono::milliseconds(std::max(1, a_durationMS)),
        [generation]() {
        SKSE::GetTaskInterface()->AddTask([generation]() {
            std::scoped_lock lock(g_slowTimeMutex);
            if (!g_slowTimeActive || generation != g_slowTimeGeneration) {
                return;
            }

            auto* timer = RE::BSTimer::GetSingleton();
            if (timer) {
                timer->SetGlobalTimeMultiplier(1.0f, true);
            }

            g_slowTimeActive = false;
        });
    });
}

void Sink::PlayParryEffects(RE::Actor* a_blocker, RE::Projectile* a_proj, Sink::ParrySource a_source, Sink::ParryType a_type) {
    if (!a_blocker) return;

    (void)a_proj;
    (void)a_source;

    bool isPlayer = a_blocker->IsPlayerRef();

    int visualMode = isPlayer ? ParrySettings::playerVisualMode : ParrySettings::npcVisualMode;
    int soundMode = isPlayer ? ParrySettings::playerSoundMode : ParrySettings::npcSoundMode;

    bool showVisuals = (visualMode == 1) || (visualMode == 2 && a_type == Sink::ParryType::Perfect);
    bool playSounds = (soundMode == 1) || (soundMode == 2 && a_type == Sink::ParryType::Perfect);

    const bool hasShield = HasEquippedShield(a_blocker);
    const bool useLeftHandAnchor = UsesLeftHandVisualAnchor(a_blocker);
    RE::BSFixedString nodeName;
    const bool hasVisualNode = ResolveParryVisualNode(a_blocker, useLeftHandAnchor, nodeName);

    RE::BGSSoundDescriptorForm* sound = nullptr;
    std::array<VisualEffect, 3> visualEffects{};
    {
        std::shared_lock lock(g_parryFormsMutex);
        if (a_type == Sink::ParryType::Perfect) {
            sound = hasShield ? Sink::ParryPerfectShieldSound : Sink::ParryPerfectSound;
        } else if (a_type == Sink::ParryType::Normal) {
            sound = hasShield ? Sink::ParryNormalShieldSound : Sink::ParryNormalSound;
        }
        visualEffects = SelectParryVisuals(isPlayer, hasShield, a_type);
    }

    if (playSounds) {
        if (sound && !PlaySoundOnActor(a_blocker, sound)) {
            SKSE::log::warn("Não foi possível tocar o som de parry no ator {:08X}.", a_blocker->GetFormID());
        }
    }

    if (!showVisuals) {
        return;
    }

    if (!hasVisualNode) {
        WarnVisualOnce(
            std::format("actor-node:{:08X}", a_blocker->GetFormID()),
            std::format(
                "Ator {:08X} não possui 3D/node compatível para VFX de parry; os visuais serão ignorados com segurança.",
                a_blocker->GetFormID()));
        return;
    }

    for (const auto& effect : visualEffects) {
        SpawnAndAttachParryEffect(a_blocker, effect, nodeName);
    }
}

void Sink::ScheduleSinkRegistration(RE::Actor* actor, int attempts)
{
    if (attempts > 20) {
        SKSE::log::critical("[Actor3DLoadEventHandler] Desistindo após {} tentativas para o ator {:08X}.", attempts, actor->GetFormID());
        return;
    }
    
    auto actorHandle = actor->CreateRefHandle();
    
    Utils::DelayedDispatcher::Get().PostDelayed(std::chrono::milliseconds(100), [actorHandle, attempts]() {
        SKSE::GetTaskInterface()->AddTask([actorHandle, attempts]() {
            if (!actorHandle) return;
            if (!actorHandle.get()) return;
                    
            auto actor = actorHandle.get();
        
            RE::BSTSmartPointer<RE::BSAnimationGraphManager> graphManager;
            actor->GetAnimationGraphManager(graphManager);
        
            if (graphManager) {
                
        
                if (actor->IsPlayerRef()) {
                    actor->RemoveAnimationGraphEventSink(Sink::NpcCycleSink::GetSingleton());
                    if (actor->AddAnimationGraphEventSink(Sink::NpcCycleSink::GetSingleton())) {
                        SKSE::log::info("[Actor3DLoadEventHandler] Sink do PLAYER reconectada com sucesso.");
                    }
        
                }
                else {
                    Sink::NpcCombatTracker::UnregisterSink(actor.get());
                    Sink::NpcCombatTracker::RegisterSink(actor.get());
                    SKSE::log::info("[Actor3DLoadEventHandler] Sink de NPC reconectada (via CombatTracker).");
                }
            }
            else {
                // Graph ainda nulo, tenta de novo
                ScheduleSinkRegistration(actor.get(), attempts + 1);
            }
            });
    });
}

RE::BSEventNotifyControl Sink::PC3DLoadEventHandler::ProcessEvent(const RE::TESObjectLoadedEvent* a_event, RE::BSTEventSource<RE::TESObjectLoadedEvent>*)
{
    if (!a_event || !a_event->loaded) {
        return RE::BSEventNotifyControl::kContinue;
    }

    // Em vez de pegar o Player Singleton, buscamos o formulário pelo ID do evento
    auto* form = RE::TESForm::LookupByID(a_event->formID);
    if (!form) return RE::BSEventNotifyControl::kContinue;

    // Tentamos converter para Ator. Se não for ator (ex: uma parede), ignoramos.
    auto* actor = form->As<RE::Actor>();
    
    if (actor) {
        ScheduleSinkRegistration(actor, 0);
    }

    return RE::BSEventNotifyControl::kContinue;
}

