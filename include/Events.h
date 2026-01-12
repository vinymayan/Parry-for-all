#pragma once
#include <shared_mutex>
#include <chrono> // Novo: para controle de tempo
#include <unordered_map> // Novo: para mapear atores e tempos
#include <set>

extern RE::TESEffectShader* parry;
extern RE::TESEffectShader* PerfParry;



namespace Sink {
    inline bool g_IsSlowed = false;

     inline RE::BGSExplosion* ParryNPlayer = nullptr;
     inline RE::BGSExplosion* ParryNPlayer2 = nullptr;
     inline RE::BGSExplosion* ParryNPlayer3 = nullptr;
     inline RE::BGSExplosion* ParryNPlayerS = nullptr;
     inline RE::BGSExplosion* ParryNPlayerS2 = nullptr;
     inline RE::BGSExplosion* ParryNPlayerS3 = nullptr;
     inline RE::BGSExplosion* ParryPPlayer = nullptr;
     inline RE::BGSExplosion* ParryPPlayer2 = nullptr;
     inline RE::BGSExplosion* ParryPPlayer3 = nullptr;
     inline RE::BGSExplosion* ParryPPlayerS = nullptr;
     inline RE::BGSExplosion* ParryPPlayerS2 = nullptr;
     inline RE::BGSExplosion* ParryPPlayerS3 = nullptr;
     inline RE::BGSExplosion* ParryNNPC = nullptr;
     inline RE::BGSExplosion* ParryNNPC2 = nullptr;
     inline RE::BGSExplosion* ParryNNPC3 = nullptr;
     inline RE::BGSExplosion* ParryNNPCS = nullptr;
     inline RE::BGSExplosion* ParryNNPCS2 = nullptr;
     inline RE::BGSExplosion* ParryNNPCS3 = nullptr;
     inline RE::BGSExplosion* ParryPNPC = nullptr;
     inline RE::BGSExplosion* ParryPNPC2 = nullptr;
     inline RE::BGSExplosion* ParryPNPC3 = nullptr;
     inline RE::BGSExplosion* ParryPNPCS = nullptr;
     inline RE::BGSExplosion* ParryPNPCS2 = nullptr;
     inline RE::BGSExplosion* ParryPNPCS3 = nullptr;


     inline RE::TESObjectACTI* Marker = nullptr;

  
    extern inline RE::BGSSoundDescriptorForm* g_ParrySound = nullptr;
    extern inline RE::TESObjectACTI* g_ParryVisualActivator = nullptr;
    inline bool isProcessingRegistration = true;

    class PC3DLoadEventHandler : public RE::BSTEventSink<RE::TESObjectLoadedEvent> {
    public:
        static PC3DLoadEventHandler* GetSingleton() {
            static PC3DLoadEventHandler singleton;
            return &singleton;
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::TESObjectLoadedEvent* a_event, RE::BSTEventSource<RE::TESObjectLoadedEvent>*) override;
    };


    //using _setIsGhost = void(*)(RE::Actor* actor, bool isGhost);

    // // 2. Localização da função usando a Address Library (IDs para SE e AE)
    // // ID 36287 = Skyrim SE | ID 37276 = Skyrim AE
    // static inline REL::Relocation<_setIsGhost> IsGhostFunc{ RELOCATION_ID(36287, 37276) };

    // /**
    //  * Função para alterar o estado de fantasma de um ator imediatamente.
    //  * @param a_actor O ponteiro para o ator (personagem/NPC).
    //  * @param a_ghost 'true' para ativar invulnerabilidade, 'false' para desativar.
    //  */
    // static void setghostnow(RE::Actor * a_actor, bool a_ghost) {
    //     if (a_actor) {
    //         // 3. Execução da chamada direta ao motor do jogo
    //         IsGhostFunc(a_actor, a_ghost);
    //     }
    // }

     // Função auxiliar para carregar tudo no início
    void InitializeForms();
    RE::TESEffectShader* GetEffectShaderByFormID(RE::FormID a_formID, const std::string& a_pluginName);

    enum class ParryType {
        None,
        Normal,
        Perfect
    };

    enum class ParrySource { Melee, Arrow, Magic };

    class ParryTimerManager {
    public:
        static void StartWindow(RE::FormID a_formID);
        static ParryType GetParryType(RE::FormID a_formID);
        static void RemoveWindow(RE::FormID a_formID);

        static void CleanupExpiredWindows();

    private:
        // Define a duração da janela (ex: 0.5 segundos)
        inline static std::unordered_map<RE::FormID, std::chrono::steady_clock::time_point> g_parryWindows;
        inline static std::shared_mutex g_parryMutex;
    };

    void ApplySlowTime(float a_multiplier);
    void ResetTimeTask();

    void PlayParryEffects(RE::Actor* a_blocker, RE::Projectile* a_proj, Sink::ParrySource a_source, Sink::ParryType a_type);

    class HitEventHandler : public RE::BSTEventSink<RE::TESHitEvent> {
    public:
        static HitEventHandler* GetSingleton() {
            static HitEventHandler singleton;
            return &singleton;
        }
        RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent* a_event,
            RE::BSTEventSource<RE::TESHitEvent>* a_source) override;
    };

    class NpcCycleSink : public RE::BSTEventSink<RE::BSAnimationGraphEvent> {
    public:
        static NpcCycleSink* GetSingleton() {
            static NpcCycleSink singleton;
            return &singleton;
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::BSAnimationGraphEvent* a_event,
            RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override;
    };

    class NpcCombatTracker : public RE::BSTEventSink<RE::TESCombatEvent> {
    public:
        static NpcCombatTracker* GetSingleton() {
            static NpcCombatTracker singleton;
            return &singleton;
        }

        // Função chamada quando um evento de combate ocorre
        RE::BSEventNotifyControl ProcessEvent(const RE::TESCombatEvent* a_event,
            RE::BSTEventSource<RE::TESCombatEvent>*) override;

        static void RegisterSink(RE::Actor* a_actor);
        static void UnregisterSink(RE::Actor* a_actor);

        static void RegisterSinksForExistingCombatants();

    private:
        // Instância compartilhada do nosso processador de lógica
        inline static NpcCycleSink g_npcSink;

        // Guarda os FormIDs dos NPCs que já estamos ouvindo
        inline static std::set<RE::FormID> g_trackedNPCs;
        inline static std::shared_mutex g_mutex;
    };
}
   

