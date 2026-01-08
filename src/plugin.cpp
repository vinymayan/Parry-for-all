#include "logger.h"
#include "Hooks.h"
#include "Settings.h"

const std::string dawn = "Dawnguard.esm";

void OnMessage(SKSE::MessagingInterface::Message* message) {
    if (message->type == SKSE::MessagingInterface::kDataLoaded) {
        parry = Sink::GetEffectShaderByFormID(0x802, "Trigger Combat Behaviour.esp");
        PerfParry = Sink::GetEffectShaderByFormID(0x802, "Trigger Combat Behaviour.esp");
        ParrySettings::Load();
        ParrySettings::MmRegister();
        Sink::InitializeForms();
       // HandleDamageHook<RE::PlayerCharacter>::install();
        //HandleDamageHook<RE::Character>::install();
        //HandleDamageHook<RE::Actor>::install();
        Hook_OnMeleeHit::install();
        Hook_OnProjectileCollision::install();

		//Hook_OnMeleeCollision::install();
    }
    if (message->type == SKSE::MessagingInterface::kNewGame || message->type == SKSE::MessagingInterface::kPostLoadGame) {
        //RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESHitEvent>(Sink::HitEventHandler::GetSingleton());
        RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink(Sink::NpcCombatTracker::GetSingleton());
        auto player = RE::PlayerCharacter::GetSingleton();
        player->AddAnimationGraphEventSink(Sink::NpcCycleSink::GetSingleton());
        
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* skse) {

    SetupLog();
    logger::info("Plugin loaded");
    auto& trampoline = SKSE::GetTrampoline();
    constexpr size_t size_per_hook = 14;
    constexpr size_t NUM_TRAMPOLINE_HOOKS = 4;
    trampoline.create(size_per_hook * NUM_TRAMPOLINE_HOOKS);
    SKSE::Init(skse);
    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);

    return true;
}
