#include "Events.h"
#include "Settings.h"

RE::TESIdleForm* anim = nullptr;
RE::TESEffectShader* parry = nullptr;
RE::TESEffectShader* PerfParry = nullptr;


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

    bool PlayPaired = false;
    bool PlayParry = false;
    float StaggerAmount = 1.0f;
    float StaggerDirection = 0.0f;
    ParryType type = ParryTimerManager::GetParryType(target->GetFormID());

    if (type != ParryType::None) {
        if (!g_IsSlowed && (attacker == player || target == player)) {
            ApplySlowTime(g_SlowTimeMultiplier);
            ResetTimeTask();
        }
        // Aplica os efeitos e seta as variáveis no alvo
        PlayParryEffects(target, type);

        // Faz o ATACANTE entrar em stagger (quem bateu no escudo/arma)
        if (attacker) {
            attacker->SetGraphVariableFloat("staggerMagnitude", 1.0f); //
            attacker->NotifyAnimationGraph("staggerStart"); //

            // Opcional: Se for perfeito, o atacante fica mais tempo em stagger
            if (type == ParryType::Perfect) {
                attacker->SetGraphVariableFloat("staggerMagnitude", 2.0f); //
            }
        }

        // Limpa a janela para evitar múltiplos acionamentos no mesmo hit
        ParryTimerManager::RemoveWindow(target->GetFormID());
        return RE::BSEventNotifyControl::kContinue;
    }
   
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
    if (a_event && a_event->holder) {
        auto actor = a_event->holder->As<RE::Actor>();
        if (!actor) {
            return RE::BSEventNotifyControl::kContinue;
        }
        auto npc = const_cast<RE::Actor*>(actor);
        const RE::FormID formID = actor->GetFormID();
        const std::string_view eventName = a_event->tag;
        //logger::info("[NPC Anim Event] Ator: '{}' ({:08X}), Evento: '{}'", actor->GetName(), actor->GetFormID(),eventName);

        if (eventName == "blockStartOut") {
            if (!Settings::npcParryEnabled && !actor->IsPlayerRef()) {
                return RE::BSEventNotifyControl::kContinue;
            }
            ParryTimerManager::StartWindow(formID);
            logger::info("Janela de Parry iniciada para: {:08X}", formID);
        }

    }
    return RE::BSEventNotifyControl::kContinue;
}

void Sink::ParryTimerManager::StartWindow(RE::FormID a_formID) {
    std::unique_lock lock(g_parryMutex);
    g_parryWindows[a_formID] = std::chrono::steady_clock::now();
}

Sink::ParryType Sink::ParryTimerManager::GetParryType(RE::FormID a_formID) {
    std::shared_lock lock(g_parryMutex);
    auto it = g_parryWindows.find(a_formID);

    if (it != g_parryWindows.end()) {
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second).count();

        auto* actor = RE::TESForm::LookupByID<RE::Actor>(a_formID);
        bool isPlayer = actor && actor->IsPlayerRef();

        // Seleciona os valores baseados no tipo de ator
        long perfectMS = isPlayer ? Settings::playerPerfectParryMS : Settings::npcPerfectParryMS;
        long normalMS = isPlayer ? Settings::playerNormalParryMS : Settings::npcNormalParryMS;

        if (duration <= perfectMS) return ParryType::Perfect;
        if (duration <= normalMS) return ParryType::Normal;
    }
    return ParryType::None;
}



void Sink::ParryTimerManager::RemoveWindow(RE::FormID a_formID) {
    std::unique_lock lock(g_parryMutex);
    g_parryWindows.erase(a_formID);
}

void Sink::InitializeForms() {
    auto* dataHandler = RE::TESDataHandler::GetSingleton();
    if (!dataHandler) return;

    g_ParryExplosion = dataHandler->LookupForm<RE::BGSExplosion>(0x900, "SekiroCombat_II.esp");
    g_ParryExplosion2 = dataHandler->LookupForm<RE::BGSExplosion>(0xC06, "SekiroCombat_II.esp");
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
        std::this_thread::sleep_for(std::chrono::milliseconds(g_SlowTimeDurationMS));

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


void Sink::PlayParryEffects(RE::Actor* a_target, ParryType a_type) {
    if (!a_target) return;

    if (a_type == ParryType::Perfect) {

		a_target->PlaceObjectAtMe(g_ParryExplosion,false);

        a_target->ApplyEffectShader(PerfParry, 10.5f, nullptr, false, false);
        // 2. Som de Parry Perfeito (ex: um "tink" mais agudo ou eco)
        RE::PlaySound("WPNMagicalWeaponImpactMetal");
        logger::info("PARRY PERFEITO executado por: {:08X}", a_target->GetFormID());

    }
    else if (a_type == ParryType::Normal) {
        a_target->PlaceObjectAtMe(g_ParryExplosion, false);
        a_target->ApplyEffectShader(parry, 10.5f, nullptr, false, false);
        logger::info("Efeito visual de Parry aplicado em: {}", a_target->GetName());
    }

}
