#include "Events.h"
#include "Settings.h"
#include "DelayedDispatcher.h"
#include "ClibUtil/editorID.hpp"

RE::TESIdleForm* anim = nullptr;
RE::TESEffectShader* parry = nullptr;
RE::TESEffectShader* PerfParry = nullptr;

namespace {
    std::mutex g_slowTimeMutex;
    std::uint64_t g_slowTimeGeneration = 0;
    bool g_slowTimeActive = false;
    std::mutex g_parrySoundMutex;
    std::unordered_map<RE::FormID, RE::BSSoundHandle> g_parrySoundHandles;

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
    ParryNPlayer = LookupFormByEditorID<RE::BGSExplosion>("ParryNormalPlayer");
    ParryNPlayer2 = LookupFormByEditorID<RE::BGSExplosion>("ParryNormalPlayer2");
    ParryNPlayer3 = LookupFormByEditorID<RE::BGSExplosion>("ParryNormalPlayer3");
    ParryNPlayerS = LookupFormByEditorID<RE::BGSExplosion>("ParryNormalPlayerShield");
    ParryNPlayerS2 = LookupFormByEditorID<RE::BGSExplosion>("ParryNormalPlayerShield2");
    ParryNPlayerS3 = LookupFormByEditorID<RE::BGSExplosion>("ParryNormalPlayerShield3");
    ParryPPlayer = LookupFormByEditorID<RE::BGSExplosion>("ParryPerfectPlayer");
    ParryPPlayer2 = LookupFormByEditorID<RE::BGSExplosion>("ParryPerfectPlayer2");
    ParryPPlayer3 = LookupFormByEditorID<RE::BGSExplosion>("ParryPerfectPlayer3");
    ParryPPlayerS = LookupFormByEditorID<RE::BGSExplosion>("ParryPerfectPlayerShield");
    ParryPPlayerS2 = LookupFormByEditorID<RE::BGSExplosion>("ParryPerfectPlayerShield2");
    ParryPPlayerS3 = LookupFormByEditorID<RE::BGSExplosion>("ParryPerfectPlayerShield3");
    ParryNNPC = LookupFormByEditorID<RE::BGSExplosion>("ParryNormalNPC");
    ParryNNPC2 = LookupFormByEditorID<RE::BGSExplosion>("ParryNormalNPC2");
    ParryNNPC3 = LookupFormByEditorID<RE::BGSExplosion>("ParryNormalNPC3");
    ParryNNPCS = LookupFormByEditorID<RE::BGSExplosion>("ParryNormalNPCShield");
    ParryNNPCS2 = LookupFormByEditorID<RE::BGSExplosion>("ParryNormalNPCShield2");
    ParryNNPCS3 = LookupFormByEditorID<RE::BGSExplosion>("ParryNormalNPCShield3");
    ParryPNPC = LookupFormByEditorID<RE::BGSExplosion>("ParryPerfectNPC");
    ParryPNPC2 = LookupFormByEditorID<RE::BGSExplosion>("ParryPerfectNPC2");
    ParryPNPC3 = LookupFormByEditorID<RE::BGSExplosion>("ParryPerfectNPC3");
    ParryPNPCS = LookupFormByEditorID<RE::BGSExplosion>("ParryPerfectNPCShield");
    ParryPNPCS2 = LookupFormByEditorID<RE::BGSExplosion>("ParryPerfectNPCShield2");
    ParryPNPCS3 = LookupFormByEditorID<RE::BGSExplosion>("ParryPerfectNPCShield3");

    ParryNormalSound = LookupFormByEditorID<RE::BGSSoundDescriptorForm>("ParryNormalSound");
    ParryNormalShieldSound = LookupFormByEditorID<RE::BGSSoundDescriptorForm>("ParryNormalSoundShield");
    ParryPerfectSound = LookupFormByEditorID<RE::BGSSoundDescriptorForm>("ParryPerfectSound");
    ParryPerfectShieldSound = LookupFormByEditorID<RE::BGSSoundDescriptorForm>("ParryPerfectSoundShield");

    bool soundsReady = true;
    soundsReady &= ValidateSoundDescriptor("ParryNormalSound", ParryNormalSound);
    soundsReady &= ValidateSoundDescriptor("ParryNormalSoundShield", ParryNormalShieldSound);
    soundsReady &= ValidateSoundDescriptor("ParryPerfectSound", ParryPerfectSound);
    soundsReady &= ValidateSoundDescriptor("ParryPerfectSoundShield", ParryPerfectShieldSound);



    //Marker = dataHandler->LookupForm<RE::TESObjectACTI>(0x81E, "ParryAll.esp");

    // Para o marker (verifique se o final é 5901 mesmo no xEdit)

    if (!ParryNPlayer) {
        SKSE::log::critical("FALHA: não encontrado em ParryAll.esp!");
    }
    else {
        SKSE::log::info("ParryEffects carregado com sucesso.");
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

    bool isPlayer = a_blocker->IsPlayerRef();

    int visualMode = isPlayer ? ParrySettings::playerVisualMode : ParrySettings::npcVisualMode;
    int soundMode = isPlayer ? ParrySettings::playerSoundMode : ParrySettings::npcSoundMode;

    bool showVisuals = (visualMode == 1) || (visualMode == 2 && a_type == Sink::ParryType::Perfect);
    bool playSounds = (soundMode == 1) || (soundMode == 2 && a_type == Sink::ParryType::Perfect);

    RE::BSFixedString nodeName = "WEAPON";
    auto leftHandObject = a_blocker->GetEquippedObject(true);
    if (leftHandObject) {
        auto armor = leftHandObject->As<RE::TESObjectARMO>();
        if (armor && armor->IsShield()) {
            nodeName = "SHIELD";
        }
    }

    if (playSounds) {
        RE::BGSSoundDescriptorForm* sound = nullptr;
        if (a_type == Sink::ParryType::Perfect) {
            sound = nodeName == "SHIELD" ? Sink::ParryPerfectShieldSound : Sink::ParryPerfectSound;
        } else if (a_type == Sink::ParryType::Normal) {
            sound = nodeName == "SHIELD" ? Sink::ParryNormalShieldSound : Sink::ParryNormalSound;
        }
        if (sound && !PlaySoundOnActor(a_blocker, sound)) {
            SKSE::log::warn("Não foi possível tocar o som de parry no ator {:08X}.", a_blocker->GetFormID());
        }
    }

    if (isPlayer) {
        if (nodeName == "SHIELD") {
            if (a_type == Sink::ParryType::Perfect) {
                if (showVisuals) {
                    a_blocker->PlaceObjectAtMe(Sink::ParryPPlayerS, false)->MoveToNode(a_blocker, nodeName);
                    a_blocker->PlaceObjectAtMe(Sink::ParryPPlayerS2, false)->MoveToNode(a_blocker, nodeName);
                    a_blocker->PlaceObjectAtMe(Sink::ParryPPlayerS3, false)->MoveToNode(a_blocker, nodeName);
                }
            }
            else if (a_type == Sink::ParryType::Normal) {
                if (showVisuals) {
                    a_blocker->PlaceObjectAtMe(Sink::ParryNPlayerS, false)->MoveToNode(a_blocker, nodeName);
                    a_blocker->PlaceObjectAtMe(Sink::ParryNPlayerS2, false)->MoveToNode(a_blocker, nodeName);
                    a_blocker->PlaceObjectAtMe(Sink::ParryNPlayerS3, false)->MoveToNode(a_blocker, nodeName);
                }
            }
        }
        else { // WEAPON
            if (a_type == Sink::ParryType::Perfect) {
                if (showVisuals) {
                    a_blocker->PlaceObjectAtMe(Sink::ParryPPlayer, false)->MoveToNode(a_blocker, nodeName);
                    a_blocker->PlaceObjectAtMe(Sink::ParryPPlayer2, false)->MoveToNode(a_blocker, nodeName);
                    a_blocker->PlaceObjectAtMe(Sink::ParryPPlayer3, false)->MoveToNode(a_blocker, nodeName);
                }
            }
            else if (a_type == Sink::ParryType::Normal) {
                if (showVisuals) {
                    a_blocker->PlaceObjectAtMe(Sink::ParryNPlayer, false)->MoveToNode(a_blocker, nodeName);
                    a_blocker->PlaceObjectAtMe(Sink::ParryNPlayer2, false)->MoveToNode(a_blocker, nodeName);
                    a_blocker->PlaceObjectAtMe(Sink::ParryNPlayer3, false)->MoveToNode(a_blocker, nodeName);
                }
            }
        }
    }
    else { // NPC
        if (nodeName == "SHIELD") {
            if (a_type == Sink::ParryType::Perfect) {
                if (showVisuals) {
                    a_blocker->PlaceObjectAtMe(Sink::ParryPNPCS, false)->MoveToNode(a_blocker, nodeName);
                    a_blocker->PlaceObjectAtMe(Sink::ParryPNPCS2, false)->MoveToNode(a_blocker, nodeName);
                    a_blocker->PlaceObjectAtMe(Sink::ParryPNPCS3, false)->MoveToNode(a_blocker, nodeName);
                }
            }
            else if (a_type == Sink::ParryType::Normal) {
                if (showVisuals) {
                    a_blocker->PlaceObjectAtMe(Sink::ParryNNPCS, false)->MoveToNode(a_blocker, nodeName);
                    a_blocker->PlaceObjectAtMe(Sink::ParryNNPCS2, false)->MoveToNode(a_blocker, nodeName);
                    a_blocker->PlaceObjectAtMe(Sink::ParryNNPCS3, false)->MoveToNode(a_blocker, nodeName);
                }
            }
        }
        else { // WEAPON
            if (a_type == Sink::ParryType::Perfect) {
                if (showVisuals) {
                    a_blocker->PlaceObjectAtMe(Sink::ParryPNPC, false)->MoveToNode(a_blocker, nodeName);
                    a_blocker->PlaceObjectAtMe(Sink::ParryPNPC2, false)->MoveToNode(a_blocker, nodeName);
                    a_blocker->PlaceObjectAtMe(Sink::ParryPNPC3, false)->MoveToNode(a_blocker, nodeName);
                }
            }
            else if (a_type == Sink::ParryType::Normal) {
                if (showVisuals) {
                    a_blocker->PlaceObjectAtMe(Sink::ParryNNPC, false)->MoveToNode(a_blocker, nodeName);
                    a_blocker->PlaceObjectAtMe(Sink::ParryNNPC2, false)->MoveToNode(a_blocker, nodeName);
                    a_blocker->PlaceObjectAtMe(Sink::ParryNNPC3, false)->MoveToNode(a_blocker, nodeName);
                }
            }
        }
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

