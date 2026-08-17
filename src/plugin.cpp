#include "logger.h"
#include "Hooks.h"
#include "Settings.h"
#include "Serialization.h"
#include "Manager.h"

namespace Hook_Precision { void Initialize(); }
namespace {
    bool hasDFG = false;

    class DynamicFormsGeneratorListener : public RE::BSTEventSink<SKSE::ModCallbackEvent> {
    public:
        static DynamicFormsGeneratorListener* GetSingleton()
        {
            static DynamicFormsGeneratorListener singleton;
            return &singleton;
        }

        void Register()
        {
            if (auto dispatcher = SKSE::GetModCallbackEventSource()) {
                dispatcher->AddEventSink(this);
            }
        }

        RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event, RE::BSTEventSource<SKSE::ModCallbackEvent>*) override
        {
            if (!a_event) return RE::BSEventNotifyControl::kContinue;

            std::string_view eventName = a_event->eventName.c_str();
            if (eventName == "DynamicFormsGeneratorLoaded") {
                Sink::InitializeForms();
                Manager::GetSingleton()->PopulateAllLists();
                return RE::BSEventNotifyControl::kContinue;
            }
            if (eventName == "DynamicFormsGeneratorUpdated") {
                Sink::InitializeForms();
                Manager::GetSingleton()->RefreshLists(a_event->strArg.c_str());
                return RE::BSEventNotifyControl::kContinue;
            }

            return RE::BSEventNotifyControl::kContinue;
        }
    };
}

void OnMessage(SKSE::MessagingInterface::Message* message) {
    if (message->type == SKSE::MessagingInterface::kPostLoad) {
        hasDFG = GetModuleHandleA("DynamicFormsGenerator.dll") != nullptr;
        if (hasDFG) {
            logger::info("DynamicFormsGenerator.dll found");
        }
    }
    if (message->type == SKSE::MessagingInterface::kDataLoaded) {
        if (!hasDFG) {
            logger::critical("DynamicFormsGenerator.dll is required. Parry for All will remain disabled.");
            return;
        }
        ParrySettings::MmRegister();
        RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink(Sink::PC3DLoadEventHandler::GetSingleton());
        Hook_OnMeleeHit::install();
        Hook_OnProjectileCollision::install();
        Hook_Precision::Initialize();
    }
    if (hasDFG && (message->type == SKSE::MessagingInterface::kNewGame || message->type == SKSE::MessagingInterface::kPostLoadGame)) {
        RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink(Sink::NpcCombatTracker::GetSingleton());
        auto player = RE::PlayerCharacter::GetSingleton();
        player->AddAnimationGraphEventSink(Sink::NpcCycleSink::GetSingleton());
        Sink::NpcCombatTracker::RegisterSinksForExistingCombatants();
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* skse) {

    SetupLog();
    logger::info("Plugin loaded ");
    SKSE::Init(skse);
    auto serialization = SKSE::GetSerializationInterface();
    serialization->SetUniqueID(Tracking::Serialization::kSerializationID);
    serialization->SetSaveCallback(Tracking::Serialization::SaveCallback);
    serialization->SetLoadCallback(Tracking::Serialization::LoadCallback);
    serialization->SetRevertCallback(Tracking::Serialization::RevertCallback);
    DynamicFormsGeneratorListener::GetSingleton()->Register();
    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);

    return true;
}
