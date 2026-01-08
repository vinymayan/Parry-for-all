#include "Events.h"
#include "Settings.h"
#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

RE::TESIdleForm* anim = nullptr;
RE::TESEffectShader* parry = nullptr;
RE::TESEffectShader* PerfParry = nullptr;


void Sink::ParryTimerManager::CleanupExpiredWindows() {
    // Usamos unique_lock pois vamos modificar o mapa
    std::unique_lock lock(g_parryMutex);
    if (g_parryWindows.empty()) return;

    auto now = std::chrono::steady_clock::now();

    // Removemos entradas com mais de 2 segundos de forma eficiente
    for (auto it = g_parryWindows.begin(); it != g_parryWindows.end(); ) {
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second).count();
        if (duration > 2000) {
            it = g_parryWindows.erase(it);
        }
        else {
            ++it;
        }
    }
}


RE::BSEventNotifyControl Sink::HitEventHandler::ProcessEvent(const RE::TESHitEvent* a_event,
    RE::BSTEventSource<RE::TESHitEvent>* a_source) {
    auto player = RE::PlayerCharacter::GetSingleton();
    if (!a_event || !a_event->cause || !a_event->target) {
        return RE::BSEventNotifyControl::kContinue;
    }

    auto* target = a_event->target.get()->As<RE::Actor>();
    auto* attacker = a_event->cause.get()->As<RE::Actor>();

    if (!attacker) {
        return RE::BSEventNotifyControl::kContinue;
    }

    if (!target || target->IsDead()) {
        return RE::BSEventNotifyControl::kContinue;
    }

    if (target->IsPlayerRef() && !ParrySettings::playerParryEnabled) {
        return RE::BSEventNotifyControl::kContinue;
	}else if(target != player && !ParrySettings::npcParryEnabled){
        return RE::BSEventNotifyControl::kContinue;
	}

    //bool PlayPaired = false;
    //bool PlayParry = false;
    //ParryType type = ParryTimerManager::GetParryType(target->GetFormID());

    //if (type != ParryType::None) {
		

    //    if (!g_IsSlowed && (attacker == player || target == player)) {
    //        ApplySlowTime(ParrySettings::slowTimeMultiplier);
    //        ResetTimeTask();
    //    }
    //    //setghostnow(target, false);
    //    // Aplica os efeitos e seta as variáveis no alvo
    //    PlayParryEffects(target, type);


    //    if (attacker) {
    //        attacker->SetGraphVariableFloat("staggerMagnitude", 0.75f); //
    //        attacker->NotifyAnimationGraph("staggerStart"); //
    //        attacker->SetGraphVariableInt("GotParriedCMF", 1);
    //        // Opcional: Se for perfeito, o atacante fica mais tempo em stagger
    //        if (type == ParryType::Perfect) {
    //            attacker->SetGraphVariableFloat("staggerMagnitude", 1.0f); //
    //            attacker->SetGraphVariableInt("GotParriedCMF", 2);
    //        }
    //    }

    //    // Limpa a janela para evitar múltiplos acionamentos no mesmo hit
    //    ParryTimerManager::RemoveWindow(target->GetFormID());
    //    return RE::BSEventNotifyControl::kContinue;
    //}
   
    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl Sink::NpcCombatTracker::ProcessEvent(const RE::TESCombatEvent* a_event, RE::BSTEventSource<RE::TESCombatEvent>*)
{
    if (!a_event || !a_event->actor) {
        return RE::BSEventNotifyControl::kContinue;
    }

    auto actor = a_event->actor.get();
    auto* npc = actor->As<RE::Actor>();
    if (npc && npc != RE::PlayerCharacter::GetSingleton()) {  // Garante que é um ator válido
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
    // Otimização: Só tentamos limpar se o evento for relevante
    if (eventName == "blockStartOut") {
        
        bool enabled = isPlayer ? ParrySettings::playerParryEnabled : ParrySettings::npcParryEnabled;

        if (enabled) {
            ParryTimerManager::StartWindow(formID); // Sobrescreve o tempo existente
        }
    }
    else {
            ParryTimerManager::CleanupExpiredWindows();
        
    }

    return RE::BSEventNotifyControl::kContinue;
}

void Sink::ParryTimerManager::StartWindow(RE::FormID a_formID) {
    std::unique_lock lock(g_parryMutex);
    g_parryWindows[a_formID] = std::chrono::steady_clock::now();
}

Sink::ParryType Sink::ParryTimerManager::GetParryType(RE::FormID a_formID) {
    std::chrono::steady_clock::time_point startTime;
    {
        std::shared_lock lock(g_parryMutex);
        auto it = g_parryWindows.find(a_formID);
        if (it == g_parryWindows.end()) return ParryType::None;
        startTime = it->second;
    }
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();

    auto* actor = RE::TESForm::LookupByID<RE::Actor>(a_formID);
    if (!actor) return ParryType::None;

    bool isPlayer = actor->IsPlayerRef();
    long perfectMS = isPlayer ? ParrySettings::playerPerfectParryMS : ParrySettings::npcPerfectParryMS;
    long normalMS = isPlayer ? ParrySettings::playerNormalParryMS : ParrySettings::npcNormalParryMS;

    // Apenas validamos o tempo. Se for maior que a janela, o parry "não existe"
    if (duration <= perfectMS) return ParryType::Perfect;
    if (duration <= normalMS) return ParryType::Normal;

    return ParryType::None;
}



void Sink::ParryTimerManager::RemoveWindow(RE::FormID a_formID) {
    std::unique_lock lock(g_parryMutex);
    g_parryWindows.erase(a_formID);
}

void Sink::InitializeForms() {
    auto* dataHandler = RE::TESDataHandler::GetSingleton();
    if (!dataHandler) return;

    // O correto é pegar os últimos 6 dígitos do xEdit, ignorando os 2 primeiros (load order)
    ParryNPlayer = dataHandler->LookupForm<RE::BGSExplosion>(0x800, "ParryAll.esp");
    ParryNPlayer2 = dataHandler->LookupForm<RE::BGSExplosion>(0x802, "ParryAll.esp");
    ParryNPlayer3 = dataHandler->LookupForm<RE::BGSExplosion>(0x814, "ParryAll.esp");
    ParryNPlayerS = dataHandler->LookupForm<RE::BGSExplosion>(0x806, "ParryAll.esp");
    ParryNPlayerS2 = dataHandler->LookupForm<RE::BGSExplosion>(0x807, "ParryAll.esp");
    ParryNPlayerS3 = dataHandler->LookupForm<RE::BGSExplosion>(0x815, "ParryAll.esp");
    ParryPPlayer = dataHandler->LookupForm<RE::BGSExplosion>(0x808, "ParryAll.esp");
    ParryPPlayer2 = dataHandler->LookupForm<RE::BGSExplosion>(0x809, "ParryAll.esp");
    ParryPPlayer3 = dataHandler->LookupForm<RE::BGSExplosion>(0x816, "ParryAll.esp");
    ParryPPlayerS = dataHandler->LookupForm<RE::BGSExplosion>(0x80A, "ParryAll.esp");
    ParryPPlayerS2 = dataHandler->LookupForm<RE::BGSExplosion>(0x80B, "ParryAll.esp");
    ParryPPlayerS3 = dataHandler->LookupForm<RE::BGSExplosion>(0x817, "ParryAll.esp");
    ParryNNPC = dataHandler->LookupForm<RE::BGSExplosion>(0x80C, "ParryAll.esp");
    ParryNNPC2 = dataHandler->LookupForm<RE::BGSExplosion>(0x80D, "ParryAll.esp");
    ParryNNPC3 = dataHandler->LookupForm<RE::BGSExplosion>(0x818, "ParryAll.esp");
    ParryNNPCS = dataHandler->LookupForm<RE::BGSExplosion>(0x80E, "ParryAll.esp");
    ParryNNPCS2 = dataHandler->LookupForm<RE::BGSExplosion>(0x80F, "ParryAll.esp");
    ParryNNPCS3 = dataHandler->LookupForm<RE::BGSExplosion>(0x819, "ParryAll.esp");
    ParryPNPC = dataHandler->LookupForm<RE::BGSExplosion>(0x808, "ParryAll.esp");
    ParryPNPC2 = dataHandler->LookupForm<RE::BGSExplosion>(0x810, "ParryAll.esp");
    ParryPNPC3 = dataHandler->LookupForm<RE::BGSExplosion>(0x81A, "ParryAll.esp");
    ParryPNPCS = dataHandler->LookupForm<RE::BGSExplosion>(0x812, "ParryAll.esp");
    ParryPNPCS2 = dataHandler->LookupForm<RE::BGSExplosion>(0x813, "ParryAll.esp");
    ParryPNPCS3 = dataHandler->LookupForm<RE::BGSExplosion>(0x81B, "ParryAll.esp");

    Marker = dataHandler->LookupForm<RE::TESObjectACTI>(0x81E, "ParryAll.esp");

    // Para o marker (verifique se o final é 5901 mesmo no xEdit)

    if (!ParryNPlayer) {
        SKSE::log::critical("FALHA: não encontrado em ParryAll.esp!");
    }
    else {
        SKSE::log::info("ParryEffects carregado com sucesso.");
    }
    //g_ParrySound = dataHandler->LookupForm<RE::BGSSoundDescriptorForm>(0x456, "SeuPlugin.esp");
    //g_ParryVisualActivator = dataHandler->LookupForm<RE::TESObjectACTI>(0x789, "SeuPlugin.esp");
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

void Sink::ApplySlowTime(float a_multiplier)
{
    auto* timer = RE::BSTimer::GetSingleton();
    if (timer) {
        timer->SetGlobalTimeMultiplier(a_multiplier, true);
		g_IsSlowed = true;
    }

}

void Sink::ResetTimeTask()
{
    std::thread([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(ParrySettings::slowTimeDurationMS));

        // Retorna para a thread principal do SKSE para evitar instabilidade
        SKSE::GetTaskInterface()->AddTask([]() {
            auto* timer = RE::BSTimer::GetSingleton();
            if (timer) {
                timer->SetGlobalTimeMultiplier(1.0f, true);
				g_IsSlowed = false;
            }
            });
        }).detach();

}

void Sink::PlayParryEffects(RE::Actor* a_blocker, RE::Projectile* a_proj, Sink::ParrySource a_source, Sink::ParryType a_type) {
    if (!a_blocker) return;


    RE::BSFixedString nodeName = "WEAPON";
	// 1. Verificar se o ator está segurando um escudo na mão esquerda
    auto leftHandObject = a_blocker->GetEquippedObject(true); // true = mão esquerda
    if (leftHandObject) {
        auto armor = leftHandObject->As<RE::TESObjectARMO>();
        if (armor && armor->IsShield()) {
            nodeName = "SHIELD"; // Altera para o node do escudo se detectado
        }
    }

    if (a_blocker->IsPlayerRef()) {
        if (nodeName == "SHIELD") {
            if (a_type == Sink::ParryType::Perfect) {

                a_blocker->PlaceObjectAtMe(Sink::ParryPPlayerS, false)->MoveToNode(a_blocker, nodeName);
                a_blocker->PlaceObjectAtMe(Sink::ParryPPlayerS2, false)->MoveToNode(a_blocker, nodeName);
                a_blocker->PlaceObjectAtMe(Sink::ParryPPlayerS3, false)->MoveToNode(a_blocker, nodeName);
                //effectMarker->ApplyEffectShader(PerfParry, 10.5f, nullptr, false, false);
                // 2. Som de Parry Perfeito (ex: um "tink" mais agudo ou eco)

            }
            else if (a_type == Sink::ParryType::Normal) {

                a_blocker->PlaceObjectAtMe(Sink::ParryNPlayerS, false)->MoveToNode(a_blocker, nodeName);
                a_blocker->PlaceObjectAtMe(Sink::ParryNPlayerS2, false)->MoveToNode(a_blocker, nodeName);
                a_blocker->PlaceObjectAtMe(Sink::ParryNPlayerS3, false)->MoveToNode(a_blocker, nodeName);

                //a_blocker->ApplyEffectShader(parry, 10.5f, nullptr, false, false);
            }
        }
        else {
            if (a_type == Sink::ParryType::Perfect) {

                a_blocker->PlaceObjectAtMe(Sink::ParryPPlayer, false)->MoveToNode(a_blocker, nodeName);
                a_blocker->PlaceObjectAtMe(Sink::ParryPPlayer2, false)->MoveToNode(a_blocker, nodeName);
                a_blocker->PlaceObjectAtMe(Sink::ParryPPlayer3, false)->MoveToNode(a_blocker, nodeName);
                //effectMarker->ApplyEffectShader(PerfParry, 10.5f, nullptr, false, false);
                // 2. Som de Parry Perfeito (ex: um "tink" mais agudo ou eco)

            }
            else if (a_type == Sink::ParryType::Normal) {

                a_blocker->PlaceObjectAtMe(Sink::ParryNPlayer, false)->MoveToNode(a_blocker, nodeName);
                a_blocker->PlaceObjectAtMe(Sink::ParryNPlayer2, false)->MoveToNode(a_blocker, nodeName);
                a_blocker->PlaceObjectAtMe(Sink::ParryNPlayer3, false)->MoveToNode(a_blocker, nodeName);

                //a_blocker->ApplyEffectShader(parry, 10.5f, nullptr, false, false);
            }
        }
        
    }
    else {
        if (nodeName == "SHIELD") {
            if (a_type == Sink::ParryType::Perfect) {

                a_blocker->PlaceObjectAtMe(Sink::ParryPNPCS, false)->MoveToNode(a_blocker, nodeName);
                a_blocker->PlaceObjectAtMe(Sink::ParryPNPCS2, false)->MoveToNode(a_blocker, nodeName);
                a_blocker->PlaceObjectAtMe(Sink::ParryPNPCS3, false)->MoveToNode(a_blocker, nodeName);
                //effectMarker->ApplyEffectShader(PerfParry, 10.5f, nullptr, false, false);
                // 2. Som de Parry Perfeito (ex: um "tink" mais agudo ou eco)

            }
            else if (a_type == Sink::ParryType::Normal) {

                a_blocker->PlaceObjectAtMe(Sink::ParryNNPCS, false)->MoveToNode(a_blocker, nodeName);
                a_blocker->PlaceObjectAtMe(Sink::ParryNNPCS2, false)->MoveToNode(a_blocker, nodeName);
                a_blocker->PlaceObjectAtMe(Sink::ParryNNPCS3, false)->MoveToNode(a_blocker, nodeName);

                //a_blocker->ApplyEffectShader(parry, 10.5f, nullptr, false, false);
            }
        }
        else {
            if (a_type == Sink::ParryType::Perfect) {

                a_blocker->PlaceObjectAtMe(Sink::ParryPNPC, false)->MoveToNode(a_blocker, nodeName);
                a_blocker->PlaceObjectAtMe(Sink::ParryPNPC2, false)->MoveToNode(a_blocker, nodeName);
                a_blocker->PlaceObjectAtMe(Sink::ParryPNPC3, false)->MoveToNode(a_blocker, nodeName);
                //effectMarker->ApplyEffectShader(PerfParry, 10.5f, nullptr, false, false);
                // 2. Som de Parry Perfeito (ex: um "tink" mais agudo ou eco)

            }
            else if (a_type == Sink::ParryType::Normal) {

                a_blocker->PlaceObjectAtMe(Sink::ParryNNPC, false)->MoveToNode(a_blocker, nodeName);
                a_blocker->PlaceObjectAtMe(Sink::ParryNNPC2, false)->MoveToNode(a_blocker, nodeName);
                a_blocker->PlaceObjectAtMe(Sink::ParryNNPC3, false)->MoveToNode(a_blocker, nodeName);

                //a_blocker->ApplyEffectShader(parry, 10.5f, nullptr, false, false);
            }
        }
    }

    
}
