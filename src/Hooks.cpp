#include "Hooks.h"
#include "Settings.h"
#include "MCP.h"
#include "PrecisionAPI.h"
#include "Serialization.h"

struct NPCParryCapability {
	bool parryEnabled = true;
	int normalParryMS = 400;
	int perfectParryMS = 100;
	int arrowMode = 1;
	int arrowReflectMode = 1;
	ParrySettings::CostType arrowCostType = ParrySettings::CostType::None;
	int magicMode = 1;
	int magicReflectMode = 1;
	ParrySettings::CostType magicCostType = ParrySettings::CostType::None;
	int reflectMeleeMode = 1;
	ParrySettings::CostType reflectMeleeCostType = ParrySettings::CostType::None;
	int reflectDamageType = 3;
	float reflectValue = 50.0f;
	int meleeStagger = 1;
	int arrowStagger = 1;
	int magicStagger = 1;
	int visualMode = 1;
	int soundMode = 1;
};

static NPCParryCapability GetNPCParryCapability(RE::Actor* a_actor)
{
	NPCParryCapability cap;

	// 1. Se o Parry estiver desligado globalmente para NPCs
	if (!ParrySettings::npcParryEnabled) {
		cap.parryEnabled = false;
		return cap;
	}

	// 2. Busca e avalia prioridade por Perks (Regras Dinâmicas)
	for (const auto& rule : ParrySettings::npcRules) {
		if (rule.perkID != 0) {
			auto perkForm = RE::TESForm::LookupByID(rule.perkID);
			if (perkForm && perkForm->GetFormType() == RE::FormType::Perk) {
				auto perk = perkForm->As<RE::BGSPerk>();
				if (perk && a_actor->HasPerk(perk)) {
					// Copia os dados específicos da regra correspondente
					cap.parryEnabled = rule.npcParryEnabled;
					cap.normalParryMS = rule.npcNormalParryMS;
					cap.perfectParryMS = rule.npcPerfectParryMS;
					cap.arrowMode = rule.npcArrowMode;
					cap.arrowReflectMode = rule.npcArrowReflectMode;
					cap.arrowCostType = rule.npcArrowCostType;
					cap.magicMode = rule.npcMagicMode;
					cap.magicReflectMode = rule.npcMagicReflectMode;
					cap.magicCostType = rule.npcMagicCostType;
					cap.reflectMeleeMode = rule.npcReflectMeleeMode;
					cap.reflectMeleeCostType = rule.npcReflectMeleeCostType;
					cap.reflectDamageType = rule.npcReflectDamageType;
					cap.reflectValue = rule.npcReflectValue;
					cap.meleeStagger = rule.npcMeleeStagger;
					cap.arrowStagger = rule.npcArrowStagger;
					cap.magicStagger = rule.npcMagicStagger;
					cap.visualMode = rule.npcVisualMode;
					cap.soundMode = rule.npcSoundMode;
					return cap;
				}
			}
		}
	}

	// 3. Fallback: Se não achar nenhum perk, puxa as variáveis globais padrões do mod
	cap.parryEnabled = ParrySettings::npcParryEnabled;
	cap.normalParryMS = ParrySettings::npcNormalParryMS;
	cap.perfectParryMS = ParrySettings::npcPerfectParryMS;
	cap.arrowMode = ParrySettings::npcArrowMode;
	cap.arrowReflectMode = ParrySettings::npcArrowReflectMode;
	cap.arrowCostType = ParrySettings::npcArrowCostType;
	cap.magicMode = ParrySettings::npcMagicMode;
	cap.magicReflectMode = ParrySettings::npcMagicReflectMode;
	cap.magicCostType = ParrySettings::npcMagicCostType;
	cap.reflectMeleeMode = ParrySettings::npcReflectMeleeMode;
	cap.reflectMeleeCostType = ParrySettings::npcReflectMeleeCostType;
	cap.reflectDamageType = ParrySettings::npcReflectDamageType;
	cap.reflectValue = ParrySettings::npcReflectValue;
	cap.meleeStagger = ParrySettings::npcMeleeStagger;
	cap.arrowStagger = ParrySettings::npcArrowStagger;
	cap.magicStagger = ParrySettings::npcMagicStagger;
	cap.visualMode = ParrySettings::npcVisualMode;
	cap.soundMode = ParrySettings::npcSoundMode;

	return cap;
}

RE::ActorValue GetAttackerMeleeSkill(RE::Actor* a_attacker) {
	if (!a_attacker) return RE::ActorValue::kOneHanded;

	// Checa a arma equipada na mão direita
	auto weaponObj = a_attacker->GetEquippedObject(false);
	if (weaponObj && weaponObj->IsWeapon()) {
		auto weapon = weaponObj->As<RE::TESObjectWEAP>();
		switch (weapon->GetWeaponType()) {
		case RE::WEAPON_TYPE::kTwoHandAxe:
		case RE::WEAPON_TYPE::kTwoHandSword:
			return RE::ActorValue::kTwoHanded;
		case RE::WEAPON_TYPE::kHandToHandMelee:
			return RE::ActorValue::kUnarmedDamage;
		case RE::WEAPON_TYPE::kBow:
		case RE::WEAPON_TYPE::kCrossbow:
			return RE::ActorValue::kArchery;
		default:
			return RE::ActorValue::kOneHanded;
		}
	}
	return RE::ActorValue::kOneHanded; // Padrão caso não consiga ler a arma
}

void ApplyStagger(RE::Actor* a_target, float a_magnitude, int a_parryValue) {
	if (!a_target) return;
	//a_target->SetGraphVariableInt("GotParriedCMF", a_parryValue);
	a_target->SetGraphVariableFloat("staggerMagnitude", a_magnitude);
	a_target->NotifyAnimationGraph("staggerStart");
}

RE::ActorValue GetAVFromCostType(ParrySettings::CostType a_type) {
	switch (a_type) {
	case ParrySettings::CostType::Stamina: return RE::ActorValue::kStamina;
	case ParrySettings::CostType::Magicka: return RE::ActorValue::kMagicka;
	case ParrySettings::CostType::Health:  return RE::ActorValue::kHealth;
	default: return RE::ActorValue::kNone;
	}
}

bool HasEnoughResource(RE::Actor* a_actor, ParrySettings::CostType a_type, float a_cost) {
	if (a_type == ParrySettings::CostType::None || a_cost <= 0.0f) return true;

	RE::ActorValue resourceAV = RE::ActorValue::kNone;
	switch (a_type) {
	case ParrySettings::CostType::Stamina: resourceAV = RE::ActorValue::kStamina; break;
	case ParrySettings::CostType::Magicka: resourceAV = RE::ActorValue::kMagicka; break;
	case ParrySettings::CostType::Health:  resourceAV = RE::ActorValue::kHealth;  break;
	}

	if (resourceAV != RE::ActorValue::kNone) {
		float currentVal = a_actor->AsActorValueOwner()->GetActorValue(resourceAV);
		return currentVal >= a_cost;
	}
	return true;
}

void Hook_OnMeleeHit::processHit(RE::Actor* victim, RE::HitData& hitData)
{
	logger::debug("Melee hit processed");
	auto aggressor = hitData.aggressor.get().get();
	bool isUnblockble = false;
	if (aggressor) {
		aggressor->GetGraphVariableBool("isUnblockableHitCMF", isUnblockble);
	}
	if (!victim || !aggressor || isUnblockble || !victim->IsBlocking()) {
		_ProcessHit(victim, hitData);
		return;
	}

	bool isPlayer = (victim == RE::PlayerCharacter::GetSingleton());
	NPCParryCapability cap;
	if (!isPlayer) {
		cap = GetNPCParryCapability(victim);
		if (!cap.parryEnabled) {
			_ProcessHit(victim, hitData);
			return;
		}
	}

	Sink::ParryType type = Sink::ParryTimerManager::GetParryType(victim->GetFormID());
	bool isParryingExternal = false;
	victim->GetGraphVariableBool("isParryingCMF", isParryingExternal);
	bool perfectParry = false;
	victim->GetGraphVariableBool("isPerfectParryingCMF", perfectParry);

	if (type == Sink::ParryType::None && isParryingExternal) {
		type = Sink::ParryType::Normal;
	}
	else if (perfectParry) {
		type = Sink::ParryType::Perfect;
	}

	if (type == Sink::ParryType::None) {
		_ProcessHit(victim, hitData);
		return;
	}

	if (!isPlayer && aggressor->IsPlayerRef() && !ParrySettings::npcParryPlayerEnabled) {
		_ProcessHit(victim, hitData);
		return;
	}

	int reflectMode = isPlayer ? ParrySettings::playerReflectMeleeMode : cap.reflectMeleeMode;
	int staggerMode = isPlayer ? ParrySettings::playerMeleeStagger : cap.meleeStagger;
	bool shouldStagger = (staggerMode == 1) || (staggerMode == 2 && type == Sink::ParryType::Perfect);
	ParrySettings::CostType costType = isPlayer ? ParrySettings::playerReflectMeleeCostType : cap.reflectMeleeCostType;
	bool shouldReflect = (reflectMode == 1) || (reflectMode == 2 && type == Sink::ParryType::Perfect);

	float cost = hitData.totalDamage;

	if (HasEnoughResource(victim, costType, cost)) {
		if (isPlayer) {
			if (type == Sink::ParryType::Perfect) {
				Tracking::PerfectParry++;
				victim->SetGraphVariableInt("PerfectParriedHitsSTM", static_cast<int>(Tracking::PerfectParry));
			}
			else {
				Tracking::NormalParry++;
				victim->SetGraphVariableInt("ParriedHitsSTM", static_cast<int>(Tracking::NormalParry));
			}
		}

		const char* parriedEvent = (type == Sink::ParryType::Perfect) ? "PerfParriedCMF" : "ParriedCMF";
		const char* gotParriedEvent = (type == Sink::ParryType::Perfect) ? "GotPerfParriedCMF" : "GotParriedCMF";

		victim->NotifyAnimationGraph(parriedEvent);
		aggressor->NotifyAnimationGraph(gotParriedEvent);

		if (isPlayer && ParrySettings::playerParryCommitmentEnabled) {
			int reduction = (type == Sink::ParryType::Perfect) ? ParrySettings::playerCommitmentReductionPerfectMS : ParrySettings::playerCommitmentReductionNormalMS;
			Sink::ParryTimerManager::ReduceCommitment(victim->GetFormID(), reduction);
		}

		auto player = RE::PlayerCharacter::GetSingleton();
		if (!Sink::g_IsSlowed) {
			if (victim == player || (aggressor == player && ParrySettings::npcParrySlowTime)) {
				Sink::ApplySlowTime(ParrySettings::slowTimeMultiplier);
				Sink::ResetTimeTask();
			}
		}

		if (costType != ParrySettings::CostType::None) {
			RE::ActorValue av = GetAVFromCostType(costType);
			if (av != RE::ActorValue::kNone) {
				victim->AsActorValueOwner()->DamageActorValue(av, cost);
			}
		}

		if (shouldReflect) {
			float damageToReflect = hitData.totalDamage;
			// Árvore Condicional Matemática de Modos de Reflexão (Nativo)
			int damageType = isPlayer ? ParrySettings::playerReflectDamageType : cap.reflectDamageType;
			float modifierValue = isPlayer ? ParrySettings::playerReflectValue : cap.reflectValue;

			switch (damageType) {
			case 0: // Flat Damage
				damageToReflect = modifierValue;
				break;
			case 1: // Percentage Damage
				damageToReflect = damageToReflect * (modifierValue / 100.0f);
				break;
			case 2: // Full Damage
				// damageToReflect mantém o valor base total intacto (100%)
				break;
			case 3: // Skill Scaling
			default:
			{
				float defSkill = victim->AsActorValueOwner()->GetActorValue(RE::ActorValue::kBlock);
				RE::ActorValue atkSkillAV = GetAttackerMeleeSkill(aggressor);
				float atkSkill = aggressor->AsActorValueOwner()->GetActorValue(atkSkillAV);
				float multiplier = defSkill / std::max(atkSkill, 1.0f);
				damageToReflect *= multiplier;
			}
			break;
			}
			aggressor->DoDamage(damageToReflect, aggressor, false);
		}

		if (shouldStagger) {
			int parryVal = (type == Sink::ParryType::Perfect ? 2 : 1);
			float magnitude = (type == Sink::ParryType::Perfect ? 1.0f : 0.5f);
			ApplyStagger(aggressor, magnitude, parryVal);
		}

		Sink::PlayParryEffects(victim, nullptr, Sink::ParrySource::Melee, type);
		hitData.totalDamage = 0;
		return;
	}
	_ProcessHit(victim, hitData);
}

namespace Hook_Precision
{
	static PRECISION_API::PreHitCallbackReturn PreHitCallback(const PRECISION_API::PrecisionHitData& a_hitData)
	{
		PRECISION_API::PreHitCallbackReturn ret;

		if (!a_hitData.target || a_hitData.target->formType != RE::FormType::ActorCharacter) {
			return ret;
		}

		auto victim = const_cast<RE::TESObjectREFR*>(a_hitData.target)->As<RE::Actor>();
		auto aggressor = a_hitData.attacker;

		if (!victim || !aggressor || !victim->IsBlocking()) {
			return ret;
		}

		bool isUnblockable = false;
		aggressor->GetGraphVariableBool("isUnblockableHitCMF", isUnblockable);
		if (isUnblockable) {
			return ret;
		}

		bool isPlayer = (victim == RE::PlayerCharacter::GetSingleton());
		NPCParryCapability cap;
		if (!isPlayer) {
			cap = GetNPCParryCapability(victim);
			if (!cap.parryEnabled) {
				return ret;
			}
		}

		Sink::ParryType type = Sink::ParryTimerManager::GetParryType(victim->GetFormID());
		bool isParryingExternal = false;
		victim->GetGraphVariableBool("isParryingCMF", isParryingExternal);
		bool perfectParry = false;
		victim->GetGraphVariableBool("isPerfectParryingCMF", perfectParry);

		if (type == Sink::ParryType::None && isParryingExternal) {
			type = Sink::ParryType::Normal;
		}
		else if (perfectParry) {
			type = Sink::ParryType::Perfect;
		}

		if (type == Sink::ParryType::None) {
			return ret;
		}

		if (!isPlayer && aggressor->IsPlayerRef() && !ParrySettings::npcParryPlayerEnabled) {
			return ret;
		}

		int reflectMode = isPlayer ? ParrySettings::playerReflectMeleeMode : cap.reflectMeleeMode;
		int staggerMode = isPlayer ? ParrySettings::playerMeleeStagger : cap.meleeStagger;
		bool shouldStagger = (staggerMode == 1) || (staggerMode == 2 && type == Sink::ParryType::Perfect);
		ParrySettings::CostType costType = isPlayer ? ParrySettings::playerReflectMeleeCostType : cap.reflectMeleeCostType;
		bool shouldReflect = (reflectMode == 1) || (reflectMode == 2 && type == Sink::ParryType::Perfect);

		float cost = 0.0f;
		auto attackerWeaponObj = aggressor->GetEquippedObject(false);
		if (attackerWeaponObj && attackerWeaponObj->IsWeapon()) {
			cost = attackerWeaponObj->As<RE::TESObjectWEAP>()->GetAttackDamage();
		}

		if (HasEnoughResource(victim, costType, cost)) {
			if (isPlayer) {
				if (type == Sink::ParryType::Perfect) {
					Tracking::PerfectParry++;
					victim->SetGraphVariableInt("PerfectParriedHitsSTM", static_cast<int>(Tracking::PerfectParry));
				}
				else {
					Tracking::NormalParry++;
					victim->SetGraphVariableInt("ParriedHitsSTM", static_cast<int>(Tracking::NormalParry));
				}
			}

			const char* parriedEvent = (type == Sink::ParryType::Perfect) ? "PerfParriedCMF" : "ParriedCMF";
			const char* gotParriedEvent = (type == Sink::ParryType::Perfect) ? "GotPerfParriedCMF" : "GotParriedCMF";

			victim->NotifyAnimationGraph(parriedEvent);
			aggressor->NotifyAnimationGraph(gotParriedEvent);

			if (isPlayer && ParrySettings::playerParryCommitmentEnabled) {
				int reduction = (type == Sink::ParryType::Perfect) ? ParrySettings::playerCommitmentReductionPerfectMS : ParrySettings::playerCommitmentReductionNormalMS;
				Sink::ParryTimerManager::ReduceCommitment(victim->GetFormID(), reduction);
			}

			if (!Sink::g_IsSlowed) {
				if (isPlayer || (aggressor->IsPlayerRef() && ParrySettings::npcParrySlowTime)) {
					Sink::ApplySlowTime(ParrySettings::slowTimeMultiplier);
					Sink::ResetTimeTask();
				}
			}

			if (costType != ParrySettings::CostType::None) {
				RE::ActorValue av = GetAVFromCostType(costType);
				if (av != RE::ActorValue::kNone) {
					victim->AsActorValueOwner()->DamageActorValue(av, cost);
				}
			}

			if (shouldReflect) {
				float damageToReflect = cost;
				// NOVO: Árvore Condicional Matemática de Modos de Reflexão (Precision API)
				int damageType = isPlayer ? ParrySettings::playerReflectDamageType : cap.reflectDamageType;
				float modifierValue = isPlayer ? ParrySettings::playerReflectValue : cap.reflectValue;

				switch (damageType) {
				case 0: // Flat
					damageToReflect = modifierValue;
					break;
				case 1: // Percentage
					damageToReflect = damageToReflect * (modifierValue / 100.0f);
					break;
				case 2: // Full
					break;
				case 3: // Skill
				default:
				{
					float defSkill = victim->AsActorValueOwner()->GetActorValue(RE::ActorValue::kBlock);
					float atkSkill = aggressor->AsActorValueOwner()->GetActorValue(GetAttackerMeleeSkill(aggressor));
					damageToReflect *= (defSkill / std::max(atkSkill, 1.0f));
				}
				break;
				}
				aggressor->DoDamage(damageToReflect, aggressor, false);
			}

			if (shouldStagger) {
				int parryVal = (type == Sink::ParryType::Perfect ? 2 : 1);
				float magnitude = (type == Sink::ParryType::Perfect ? 1.0f : 0.5f);
				ApplyStagger(aggressor, magnitude, parryVal);
			}

			Sink::PlayParryEffects(victim, nullptr, Sink::ParrySource::Melee, type);
			ret.bIgnoreHit = true;
		}

		return ret;
	}

	void Initialize()
	{
		auto precisionAPI = static_cast<PRECISION_API::IVPrecision4*>(PRECISION_API::RequestPluginAPI(PRECISION_API::InterfaceVersion::V4));
		if (precisionAPI) {
			// Unificação das chamadas da API sob o mesmo escopo lógico
			precisionAPI->AddPreHitCallback(SKSE::GetPluginHandle(), PreHitCallback);
			precisionAPI->AddWeaponWeaponCollisionCallback(SKSE::GetPluginHandle(), W2W_System::PrecisionWeaponsCallback);
			SKSE::log::info("[Precision API Unificada]: Registros de PreHit e Weapon Collision concluídos com sucesso!");
		}
		else {
			SKSE::log::warn("[Precision API]: Plugin não encontrado. Rodando puramente por hooks nativos do jogo.");
		}
	}
}

void SetRotationFromVector(RE::Projectile* a_projectile, const RE::NiPoint3& a_direction) {
	if (!a_projectile || !a_projectile->Get3D()) return;

	float yaw = atan2f(a_direction.x, a_direction.y);
	float pitch = atan2f(a_direction.z, sqrtf(a_direction.x * a_direction.x + a_direction.y * a_direction.y));

	RE::NiMatrix3 rotation;
	rotation.SetEulerAnglesXYZ(-pitch, 0.0f, yaw); // Ajuste de eixos pode variar dependendo do modelo
	a_projectile->Get3D()->local.rotate = rotation;
	RE::NiUpdateData updateData;
	a_projectile->Get3D()->UpdateWorldData(&updateData);
}

void ReflectProjectileBack(RE::Actor* a_blocker, RE::Projectile* a_projectile, RE::hkpCollidable* a_projectile_collidable)
{
	if (!a_projectile) return;

	auto& runtimeData = a_projectile->GetProjectileRuntimeData();
	RE::TESObjectREFR* originalShooter = nullptr;

	if (runtimeData.shooter && runtimeData.shooter.get()) {
		originalShooter = runtimeData.shooter.get().get();
	}

	// 1. Mudar a "propriedade" do projétil
	a_projectile->SetActorCause(a_blocker->GetActorCause());
	runtimeData.shooter = a_blocker->GetHandle();

	// 2. Atualizar Filtro de Colisão para ignorar o Blocker
	if (a_projectile_collidable) {
		RE::CFilter blockerFilter;
		a_blocker->GetCollisionFilterInfo(blockerFilter);

		uint32_t currentInfoVal = a_projectile_collidable->broadPhaseHandle.collisionFilterInfo.filter;
		uint32_t blockerInfoVal = blockerFilter.filter;

		// Mantém flags de colisão da flecha/magia, mas assume o System Group do Blocker
		uint32_t newInfoVal = (currentInfoVal & 0x0000FFFF) | (blockerInfoVal & 0xFFFF0000);
		a_projectile_collidable->broadPhaseHandle.collisionFilterInfo.filter = newInfoVal;
	}

	// 3. Calcular Nova Direção
	RE::NiPoint3 currentPos = a_projectile->GetPosition();
	RE::NiPoint3 direction;

	if (originalShooter) {
		RE::NiPoint3 targetPos = originalShooter->GetPosition();
		// Ajusta para o peito/cabeça do alvo
		//targetPos.z += originalShooter->GetHeight() * 0.70f;
		direction = targetPos - currentPos;
	}
	else {
		// Se não houver atirador (armadilha), inverte a velocidade atual
		RE::NiPoint3 currentVel;
		a_projectile->GetLinearVelocity(currentVel);
		direction = currentVel * -1.0f;
	}

	direction.Unitize();

	// 5. Aplicar Velocidade e Rotação
	float speed = 3500.0f;
	runtimeData.linearVelocity = direction * speed;
	runtimeData.velocity = direction * speed;
	//a_projectile->SetLinearVelocity(direction * speed);

	// Supondo que você tenha essa helper function definida:
	SetRotationFromVector(a_projectile, direction);

	// 6. Reinicializar estados
	runtimeData.livingTime = 0.0f;
	// Para magias: resetar o hit timer para permitir que atinja quem disparou
	if (runtimeData.spell) {
		// Algumas magias precisam limpar a lista de alvos já atingidos
		// Isso depende da complexidade do projétil (Missile vs Beam)
	}
}

bool processProjectileBlock(RE::Actor* a_blocker, RE::Projectile* a_projectile, RE::hkpCollidable* a_projectile_collidable)
{
	auto player = RE::PlayerCharacter::GetSingleton();
	bool isPlayer = (a_blocker == player);
	auto shooterHandle = a_projectile->GetProjectileRuntimeData().shooter;
	auto shooter = shooterHandle.get().get() ? shooterHandle.get().get()->As<RE::Actor>() : nullptr;

	if (shooter && shooter->IsPlayerRef() && !ParrySettings::npcParryPlayerEnabled) {
		return false;
	}

	bool isUnblockable = false;
	if (shooter) {
		shooter->GetGraphVariableBool("isUnblockableHitCMF", isUnblockable);
		if (isUnblockable) {
			return false;
		}
	}

	NPCParryCapability cap;
	if (!isPlayer) {
		cap = GetNPCParryCapability(a_blocker);
		if (!cap.parryEnabled) {
			return false;
		}
	}

	Sink::ParryType type = Sink::ParryTimerManager::GetParryType(a_blocker->GetFormID());
	bool isParryingExternal = false;
	a_blocker->GetGraphVariableBool("isParryingCMF", isParryingExternal);
	bool perfectParry = false;
	a_blocker->GetGraphVariableBool("isPerfectParryingCMF", perfectParry);

	if (type == Sink::ParryType::None && isParryingExternal) {
		type = Sink::ParryType::Normal;
	}
	else if (perfectParry) {
		type = Sink::ParryType::Perfect;
	}

	if (type == Sink::ParryType::None) return false;

	bool isArrow = !a_projectile->GetProjectileRuntimeData().spell;

	int currentMode = isArrow ? (isPlayer ? ParrySettings::playerArrowMode : cap.arrowMode)
		: (isPlayer ? ParrySettings::playerMagicMode : cap.magicMode);

	if (currentMode == 0) return false;

	float cost = 0.0f;
	ParrySettings::CostType costType = ParrySettings::CostType::None;

	if (currentMode == 2) {
		costType = isArrow ? (isPlayer ? ParrySettings::playerArrowCostType : cap.arrowCostType)
			: (isPlayer ? ParrySettings::playerMagicCostType : cap.magicCostType);

		if (isArrow) {
			auto launcher = a_projectile->GetProjectileRuntimeData().weaponSource;
			auto ammo = a_projectile->GetProjectileRuntimeData().ammoSource;
			if (launcher) cost += launcher->GetAttackDamage();
			if (ammo) cost += ammo->As<RE::TESAmmo>()->GetRuntimeData().data.damage;
		}
		else {
			cost = a_projectile->GetProjectileRuntimeData().spell->CalculateMagickaCost(a_blocker);
		}
	}

	if (!HasEnoughResource(a_blocker, costType, cost)) {
		return false;
	}

	if (isPlayer) {
		if (type == Sink::ParryType::Perfect) {
			Tracking::PerfectParry++;
			a_blocker->SetGraphVariableInt("PerfectParriedHitsSTM", static_cast<int>(Tracking::PerfectParry));
		}
		else {
			Tracking::NormalParry++;
			a_blocker->SetGraphVariableInt("ParriedHitsSTM", static_cast<int>(Tracking::NormalParry));
		}
	}

	int reflectModeSetting = isArrow ? (isPlayer ? ParrySettings::playerArrowReflectMode : cap.arrowReflectMode)
		: (isPlayer ? ParrySettings::playerMagicReflectMode : cap.magicReflectMode);

	bool shouldReflect = (reflectModeSetting == 1) || (reflectModeSetting == 2 && type == Sink::ParryType::Perfect);
	int staggerMode = isArrow ? (isPlayer ? ParrySettings::playerArrowStagger : cap.arrowStagger)
		: (isPlayer ? ParrySettings::playerMagicStagger : cap.magicStagger);

	bool shouldStagger = (staggerMode == 1) || (staggerMode == 2 && type == Sink::ParryType::Perfect);

	if (shouldStagger) {
		auto shooterActor = a_projectile->GetProjectileRuntimeData().shooter.get().get();
		if (shooterActor && shooterActor->As<RE::Actor>()) {
			int parryVal = (type == Sink::ParryType::Perfect ? 2 : 1);
			shooterActor->SetGraphVariableInt("ShouldStaggerCMF", parryVal);
		}
	}

	if (!Sink::g_IsSlowed) {
		if (a_blocker == player || (shooter == player && ParrySettings::npcParrySlowTime)) {
			Sink::ApplySlowTime(ParrySettings::slowTimeMultiplier);
			Sink::ResetTimeTask();
		}
	}

	Sink::PlayParryEffects(a_blocker, a_projectile, isArrow ? Sink::ParrySource::Arrow : Sink::ParrySource::Magic, type);

	if (cost > 0.0f) {
		RE::ActorValue resourceAV = GetAVFromCostType(costType);
		if (resourceAV != RE::ActorValue::kNone) {
			a_blocker->AsActorValueOwner()->DamageActorValue(resourceAV, cost);
		}
	}

	if (shouldReflect) {
		ReflectProjectileBack(a_blocker, a_projectile, a_projectile_collidable);
	}
	else {
		RE::Offset::destroyProjectile(a_projectile);
	}

	const char* parriedEvent = (type == Sink::ParryType::Perfect) ? "PerfParriedCMF" : "ParriedCMF";
	const char* gotParriedEvent = (type == Sink::ParryType::Perfect) ? "GotPerfParriedCMF" : "GotParriedCMF";

	a_blocker->NotifyAnimationGraph(parriedEvent);
	if (shooter) {
		shooter->NotifyAnimationGraph(gotParriedEvent);
	}

	if (isPlayer && ParrySettings::playerParryCommitmentEnabled) {
		int reduction = (type == Sink::ParryType::Perfect) ? ParrySettings::playerCommitmentReductionPerfectMS : ParrySettings::playerCommitmentReductionNormalMS;
		Sink::ParryTimerManager::ReduceCommitment(a_blocker->GetFormID(), reduction);
	}

	return true;
}

inline bool shouldIgnoreHit(RE::Projectile* a_projectile, RE::hkpAllCdPointCollector* a_AllCdPointCollector)
{
	if (a_AllCdPointCollector) {
		for (auto& hit : a_AllCdPointCollector->hits) {
			auto refrA = RE::TESHavokUtilities::FindCollidableRef(*hit.rootCollidableA);
			auto refrB = RE::TESHavokUtilities::FindCollidableRef(*hit.rootCollidableB);
			RE::Actor* target = nullptr;
			RE::hkpCollidable* projectileCollidable = nullptr;

			if (refrA && refrA->formType == RE::FormType::ActorCharacter) {
				target = refrA->As<RE::Actor>();
				projectileCollidable = const_cast<RE::hkpCollidable*>(hit.rootCollidableB);
			}
			else if (refrB && refrB->formType == RE::FormType::ActorCharacter) {
				target = refrB->As<RE::Actor>();
				projectileCollidable = const_cast<RE::hkpCollidable*>(hit.rootCollidableA);
			}

			if (target) {
				// 1. Tenta processar como um bloqueio/parry
				if (processProjectileBlock(target, a_projectile, projectileCollidable)) {
					return true; // Se bloqueou, ignora a colisão original
				}

				// 2. Se NÃO foi um bloqueio, verifica se este ator tem um stagger pendente (ShouldStaggerCMF)
				int pendingStagger = 0;
				if (target->GetGraphVariableInt("ShouldStaggerCMF", pendingStagger) && pendingStagger > 0) {
					float magnitude = (pendingStagger == 2) ? 1.0f : 0.5f;

					logger::debug("Applying pending stagger to {} (Type: {})", target->GetName(), pendingStagger);
					ApplyStagger(target, magnitude, pendingStagger);

					// Reseta para 0 após aplicar
					target->SetGraphVariableInt("ShouldStaggerCMF", 0);
				}
			}
		}
	}
	return false;
}

void Hook_OnProjectileCollision::OnArrowCollision(RE::Projectile* a_this, RE::hkpAllCdPointCollector* a_AllCdPointCollector)
{
	logger::debug("Arrow collided");
	if(shouldIgnoreHit(a_this, a_AllCdPointCollector)){
		return;
	}
	_arrowCollission(a_this, a_AllCdPointCollector);
}

void Hook_OnProjectileCollision::OnMissileCollision(RE::Projectile* a_this, RE::hkpAllCdPointCollector* a_AllCdPointCollector)
{
	logger::debug("Missile collided");
	if (a_this && (a_this->GetProjectileRuntimeData().spell || a_this->GetProjectileBase())) {
		logger::debug("Processing projectile for parry check");
		if (shouldIgnoreHit(a_this, a_AllCdPointCollector)) {
			logger::debug("Projectile parried, ignoring collision");
			return; // Parry bem-sucedido, ignora a colisão original
		}
	}
	_missileCollission(a_this, a_AllCdPointCollector);
}

PRECISION_API::WeaponCollisionCallbackReturn W2W_System::PrecisionWeaponsCallback(const PRECISION_API::PrecisionHitData& a_precisionHitData)
{
	PRECISION_API::WeaponCollisionCallbackReturn ret;
	ret.bIgnoreHit = false; // Hit normal por padrão

	if (ParrySettings::w2wMode == 0) return ret; // Sistema desabilitado

	auto hit_causer = a_precisionHitData.attacker;
	auto hit_target = a_precisionHitData.target ? a_precisionHitData.target->As<RE::Actor>() : nullptr;

	if (!hit_causer || !hit_target) return ret;

	// Verifica a restrição de "Apenas NPCs"
	bool isPlayerInvolved = hit_causer->IsPlayerRef() || hit_target->IsPlayerRef();
	if (ParrySettings::w2wMode == 1 && isPlayerInvolved) return ret;

	// 1. Coleta Dados das Armas e Dano Base
	auto attackerWeaponObj = hit_causer->GetEquippedObject(false);
	auto defenderWeaponObj = hit_target->GetEquippedObject(false);

	float attackerDamage = 0.0f;
	if (attackerWeaponObj && attackerWeaponObj->IsWeapon()) {
		attackerDamage = attackerWeaponObj->As<RE::TESObjectWEAP>()->GetAttackDamage();
	}

	float defenderDamage = 0.0f;
	if (defenderWeaponObj && defenderWeaponObj->IsWeapon()) {
		defenderDamage = defenderWeaponObj->As<RE::TESObjectWEAP>()->GetAttackDamage();
	}

	// 2. Coleta a Skill atual dos atores com suas armas
	// Utilizando a função GetAttackerMeleeSkill que já existe no seu Hooks.cpp
	float attackerSkill = hit_causer->AsActorValueOwner()->GetActorValue(GetAttackerMeleeSkill(hit_causer));
	float defenderSkill = hit_target->AsActorValueOwner()->GetActorValue(GetAttackerMeleeSkill(hit_target));

	// 3. Calcula o Power (Poder de colisão) baseado nos multiplicadores da UI
	float attackerPower = ParrySettings::w2wBaseChance + (attackerDamage * ParrySettings::w2wDamageMult) + (attackerSkill * ParrySettings::w2wSkillMult);
	float defenderPower = ParrySettings::w2wBaseChance + (defenderDamage * ParrySettings::w2wDamageMult) + (defenderSkill * ParrySettings::w2wSkillMult);

	// 4. Determina o Vencedor baseado na lógica de probabilidade do UnblockableManager
	float attackerWinChance = attackerPower / (attackerPower + defenderPower + ParrySettings::w2wGlobalDifficulty);

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<> dis(0.0, 1.0);
	float roll = static_cast<float>(dis(gen));

	bool attackerWins = roll < attackerWinChance;

	RE::Actor* winner = attackerWins ? hit_causer : hit_target;
	RE::Actor* loser = attackerWins ? hit_target : hit_causer;
	// 5. Verifica quem deve receber a reação com base na UI
	bool affectAttacker = (ParrySettings::w2wReactionTarget == 1 || ParrySettings::w2wReactionTarget == 3);
	bool affectDefender = (ParrySettings::w2wReactionTarget == 2 || ParrySettings::w2wReactionTarget == 3);

	const RE::NiPoint3 hitPos = a_precisionHitData.hitPos;

	// Aplica os Graph Events através de uma Task no SKSE para thread safety
	SKSE::GetTaskInterface()->AddTask([winner, loser, hit_causer, hit_target, hitPos, affectAttacker, affectDefender]() {

		// Lambda interna para aplicar a reação correta
		auto applyReaction = [](RE::Actor* actor, bool isWinner) {
			if (!actor) return;

			if (ParrySettings::w2wReactionType == 0) {
				// TIPO: RECOIL
				actor->NotifyAnimationGraph("recoilStop");
				actor->NotifyAnimationGraph("AttackStop");
				actor->NotifyAnimationGraph(isWinner ? "recoilStart" : "recoilLargeStart");
			}
			else {
				// TIPO: STAGGER
				float magnitude = isWinner ? 0.25f : 1.0f;
				int parryVal = isWinner ? 1 : 2;
				ApplyStagger(actor, magnitude, parryVal);
			}
		};

		// Aplica ao atacante (hit_causer) se a UI permitir
		if (affectAttacker && hit_causer) {
			applyReaction(hit_causer, hit_causer == winner);
		}

		// Aplica ao defensor (hit_target) se a UI permitir
		if (affectDefender && hit_target) {
			applyReaction(hit_target, hit_target == winner);
		}
		});

	// Retorna bIgnoreHit = true para que o jogo ignore o dano físico do choque e trate apenas como a colisão das armas
	ret.bIgnoreHit = true;
	return ret;
}