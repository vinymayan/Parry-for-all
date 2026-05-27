#include "logger.h"
#include "Hooks.h"
#include "Settings.h"
#include "Serialization.h"
#include "Manager.h"

namespace Hook_Precision { void Initialize(); }

void OnMessage(SKSE::MessagingInterface::Message* message) {
    if (message->type == SKSE::MessagingInterface::kDataLoaded) {
        ParrySettings::MmRegister();
        Sink::InitializeForms();
        RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink(Sink::PC3DLoadEventHandler::GetSingleton());
        Hook_OnMeleeHit::install();
        Hook_OnProjectileCollision::install();
        Hook_Precision::Initialize();
        Manager::GetSingleton()->PopulateAllLists();
    }
    if (message->type == SKSE::MessagingInterface::kNewGame || message->type == SKSE::MessagingInterface::kPostLoadGame) {
        RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink(Sink::NpcCombatTracker::GetSingleton());
        auto player = RE::PlayerCharacter::GetSingleton();
        player->AddAnimationGraphEventSink(Sink::NpcCycleSink::GetSingleton());
        Sink::NpcCombatTracker::RegisterSinksForExistingCombatants();
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* skse) {

    SetupLog();
    logger::info("Plugin loaded");
    SKSE::Init(skse);
    auto serialization = SKSE::GetSerializationInterface();
    serialization->SetUniqueID(Tracking::Serialization::kSerializationID);
    serialization->SetSaveCallback(Tracking::Serialization::SaveCallback);
    serialization->SetLoadCallback(Tracking::Serialization::LoadCallback);
    serialization->SetRevertCallback(Tracking::Serialization::RevertCallback);
    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);

    return true;
}
