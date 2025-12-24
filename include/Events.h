#pragma once
#include <shared_mutex>
#include <chrono> // Novo: para controle de tempo
#include <unordered_map> // Novo: para mapear atores e tempos

extern RE::TESEffectShader* parry;
extern RE::TESEffectShader* PerfParry;


namespace Sink {
    inline static long long g_NormalParryMS = 500;
    inline static long long g_PerfectParryMS = 150;
    inline static float g_SlowTimeMultiplier = 0.2f;
    inline static int g_SlowTimeDurationMS = 1000;
	inline bool g_IsSlowed = false;

    inline RE::BGSExplosion* g_ParryExplosion = nullptr;
    inline RE::BGSExplosion* g_ParryExplosion2 = nullptr;
    inline RE::BGSSoundDescriptorForm* g_ParrySound = nullptr;
    inline RE::TESObjectACTI* g_ParryVisualActivator = nullptr;

    // Função auxiliar para carregar tudo no início
    void InitializeForms();
    RE::TESEffectShader* GetEffectShaderByFormID(RE::FormID a_formID, const std::string& a_pluginName);

    enum class ParryType {
        None,
        Normal,
        Perfect
    };

    class ParryTimerManager {
    public:
        static void StartWindow(RE::FormID a_formID);
        static ParryType GetParryType(RE::FormID a_formID);
        static void RemoveWindow(RE::FormID a_formID);

    private:
        // Define a duração da janela (ex: 0.5 segundos)
        inline static std::unordered_map<RE::FormID, std::chrono::steady_clock::time_point> g_parryWindows;
        inline static std::shared_mutex g_parryMutex;
    };

    void ApplySlowTime(float a_multiplier);
    void ResetTimeTask();
    void PlayParryEffects(RE::Actor* a_target, ParryType a_type);

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

    struct ProcessHitHook
    {
        static void Install()
        {
            REL::Relocation<std::uintptr_t> actorVtable{ RE::VTABLE_Actor[0] };

            // Realiza o hook no índice da VTable
            _HandleHealthDamage = actorVtable.write_vfunc((0x104, 0x104, 0x106), HandleHealthDamage);
            logger::info("Hook de HandleHealthDamage instalado com sucesso.");
        }


    private:
        static void HandleHealthDamage(RE::Actor* a_self, RE::Actor* a_attacker, float a_damage) {
            logger::info("HandleHealthDamage chamado para ator {:08X} com dano {:.2f}", a_self->GetFormID(), a_damage);

            // 1. Verificamos se o alvo está na janela de Parry
            ParryType type = ParryTimerManager::GetParryType(a_self->GetFormID());

            if (type != ParryType::None) {
                // Se estiver em Parry, zeramos o dano antes de processar!
                a_damage = 0.0f;


            }


            // 2. Chama a função original (com dano 0 se foi parry, ou dano normal se não foi)
            _HandleHealthDamage(a_self, a_attacker, a_damage);
        }

        static void HandleParrySuccess(RE::Actor* a_victim, RE::HitData& a_hitData, ParryType a_type)
        {
            auto attacker = a_hitData.aggressor.get()->As<RE::Actor>();

            // Aplica Slow Time se for o player
            if (a_victim->IsPlayerRef() || (attacker && attacker->IsPlayerRef())) {
                // Suas funções de tempo aqui (ApplySlowTime, etc)
            }

            // Faz o atacante entrar em Stagger
            if (attacker) {
                float magnitude = (a_type == ParryType::Perfect) ? 2.0f : 1.0f;
                attacker->SetGraphVariableFloat("staggerMagnitude", magnitude);
                attacker->NotifyAnimationGraph("staggerStart");
            }

            // Limpa a janela para evitar bugs
            ParryTimerManager::RemoveWindow(a_victim->GetFormID());

            logger::info("Parry com sucesso via ProcessHit! Dano anulado.");
        }

        static inline REL::Relocation<decltype(HandleHealthDamage)> _HandleHealthDamage;
    };
}

