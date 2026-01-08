#pragma once
#include "Events.h"



class Hook_OnMeleeHit
{
public:
    static void install()
    {
        auto& trampoline = SKSE::GetTrampoline();
        constexpr size_t size_per_hook = 14;
        constexpr size_t NUM_TRAMPOLINE_HOOKS = 1;
        trampoline.create(size_per_hook * NUM_TRAMPOLINE_HOOKS);
        REL::Relocation<uintptr_t> hook{ RELOCATION_ID(37673, 38627) };  //140628C20       14064E760
        _ProcessHit = trampoline.write_call<5>(hook.address() + REL::Relocate(0x3C0, 0x4A8), processHit);
        logger::info("hook:OnMeleeHit");
    }

private:
    static void processHit(RE::Actor* victim, RE::HitData& hitData);
    static inline REL::Relocation<decltype(processHit)> _ProcessHit;  //140626400       14064BAB0
};

class Hook_OnProjectileCollision
{
public:
	static void install()
	{
		REL::Relocation<std::uintptr_t> arrowProjectileVtbl{ RE::VTABLE_ArrowProjectile[0] };
		REL::Relocation<std::uintptr_t> missileProjectileVtbl{ RE::VTABLE_MissileProjectile[0] };

		_arrowCollission = arrowProjectileVtbl.write_vfunc(190, OnArrowCollision);
		_missileCollission = missileProjectileVtbl.write_vfunc(190, OnMissileCollision);
		logger::info("hook:OnProjectileCollision");
	};

private:
	static void OnArrowCollision(RE::Projectile* a_this, RE::hkpAllCdPointCollector* a_AllCdPointCollector);

	static void OnMissileCollision(RE::Projectile* a_this, RE::hkpAllCdPointCollector* a_AllCdPointCollector);
	static inline REL::Relocation<decltype(OnArrowCollision)> _arrowCollission;
	static inline REL::Relocation<decltype(OnMissileCollision)> _missileCollission;
};

class Hook_OnMeleeCollision
{
public:
	static void install()
	{
		auto& trampoline = SKSE::GetTrampoline();
        constexpr size_t size_per_hook = 14;
        constexpr size_t NUM_TRAMPOLINE_HOOKS = 1;
        trampoline.create(size_per_hook * NUM_TRAMPOLINE_HOOKS);
        REL::Relocation<uintptr_t> hook{ RELOCATION_ID(37650, 38603) };  //SE:627930 + 38B AE:64D350 +  45A
		_ProcessHit = trampoline.write_call<5>(hook.address() + REL::Relocate(0x38B, 0x45A), processHit);

		logger::info("hook:OnMeleeCollision");
	}
private:
	static void processHit(RE::Actor* a_aggressor, RE::Actor* a_victim, std::int64_t a_int1, bool a_bool, void* a_unkptr);

	static inline REL::Relocation<decltype(processHit)> _ProcessHit;
};


template <typename FormType>
class HandleDamageHook : public FormType {
    // Movemos a implementação para cá (dentro da classe ou logo abaixo)
    static void thunk(FormType* a_this, RE::Actor* a_attacker, float a_dmg) {
        RE::Actor* victim = a_this->As<RE::Actor>();
        Sink::ParryType type = Sink::ParryTimerManager::GetParryType(victim->GetFormID());
		if (type == Sink::ParryType::Normal) {
            a_attacker->DoDamage(-a_dmg, a_attacker, false);
            a_dmg = 0.0f;
		}
		else if (type == Sink::ParryType::Perfect) {
            a_attacker->DoDamage(-a_dmg, a_attacker, false);
            a_dmg = 0.0f;
		}
        _HandleDamage(a_this, a_attacker, a_dmg);
    };

    static inline REL::Relocation<decltype(&FormType::HandleHealthDamage)> _HandleDamage;

public:
    static void install() {
        REL::Relocation<std::uintptr_t> _vtbl{ FormType::VTABLE[0] };
        _HandleDamage = _vtbl.write_vfunc(REL::Relocate(0x104, 0x104, 0x106), thunk);
    }
};


