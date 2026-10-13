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
	int arrowReflectTriggers = 1;
	ParrySettings::CostType arrowCostType = ParrySettings::CostType::None;
	float arrowCost = 0.0f;
	RE::FormID arrowCostGlobal = 0;
	ParrySettings::ValueSource arrowCostSource = ParrySettings::ValueSource::Flat;
	std::string arrowCostActorValue = "Stamina";
	int magicMode = 1;
	int magicReflectMode = 1;
	int magicReflectTriggers = 1;
	ParrySettings::CostType magicCostType = ParrySettings::CostType::None;
	float magicCost = 0.0f;
	RE::FormID magicCostGlobal = 0;
	ParrySettings::ValueSource magicCostSource = ParrySettings::ValueSource::Flat;
	std::string magicCostActorValue = "Magicka";
	int reflectMeleeMode = 1;
	int meleeReflectTriggers = 1;
	ParrySettings::CostType reflectMeleeCostType = ParrySettings::CostType::None;
	float reflectMeleeCost = 0.0f;
	RE::FormID reflectMeleeCostGlobal = 0;
	ParrySettings::ValueSource reflectMeleeCostSource = ParrySettings::ValueSource::Flat;
	std::string reflectMeleeCostActorValue = "Stamina";
	int reflectDamageType = 3;
	float reflectValue = 50.0f;
	RE::FormID reflectValueGlobal = 0;
	ParrySettings::ValueSource reflectValueSource = ParrySettings::ValueSource::Flat;
	std::string reflectValueActorValue = "Block";
	int meleeStagger = 1;
	int arrowStagger = 1;
	int magicStagger = 1;
	int meleeStaggerSize = 1;
	int arrowStaggerSize = 1;
	int magicStaggerSize = 1;
	int visualMode = 1;
	int soundMode = 1;
};

static NPCParryCapability GetNPCParryCapability(RE::Actor* a_actor)
{
	NPCParryCapability cap;
	if (!a_actor || (ParrySettings::npcDisableParryPerk != 0 &&
		ParrySettings::HasRequiredPerk(a_actor, ParrySettings::npcDisableParryPerk))) {
		cap.parryEnabled = false;
		return cap;
	}

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
					cap.arrowReflectTriggers = rule.npcArrowReflectTriggers;
					cap.arrowCostType = rule.npcArrowCostType;
					cap.arrowCost = rule.npcArrowCost;
					cap.arrowCostGlobal = rule.npcArrowCostGlobal;
					cap.arrowCostSource = rule.npcArrowCostSource;
					cap.arrowCostActorValue = rule.npcArrowCostActorValue;
					cap.magicMode = rule.npcMagicMode;
					cap.magicReflectMode = rule.npcMagicReflectMode;
					cap.magicReflectTriggers = rule.npcMagicReflectTriggers;
					cap.magicCostType = rule.npcMagicCostType;
					cap.magicCost = rule.npcMagicCost;
					cap.magicCostGlobal = rule.npcMagicCostGlobal;
					cap.magicCostSource = rule.npcMagicCostSource;
					cap.magicCostActorValue = rule.npcMagicCostActorValue;
					cap.reflectMeleeMode = rule.npcReflectMeleeMode;
					cap.meleeReflectTriggers = rule.npcMeleeReflectTriggers;
					cap.reflectMeleeCostType = rule.npcReflectMeleeCostType;
					cap.reflectMeleeCost = rule.npcReflectMeleeCost;
					cap.reflectMeleeCostGlobal = rule.npcReflectMeleeCostGlobal;
					cap.reflectMeleeCostSource = rule.npcReflectMeleeCostSource;
					cap.reflectMeleeCostActorValue = rule.npcReflectMeleeCostActorValue;
					cap.reflectDamageType = rule.npcReflectDamageType;
					cap.reflectValue = rule.npcReflectValue;
					cap.reflectValueGlobal = rule.npcReflectValueGlobal;
					cap.reflectValueSource = rule.npcReflectValueSource;
					cap.reflectValueActorValue = rule.npcReflectValueActorValue;
					cap.meleeStagger = rule.npcMeleeStagger;
					cap.arrowStagger = rule.npcArrowStagger;
					cap.magicStagger = rule.npcMagicStagger;
					cap.meleeStaggerSize = rule.npcMeleeStaggerSize;
					cap.arrowStaggerSize = rule.npcArrowStaggerSize;
					cap.magicStaggerSize = rule.npcMagicStaggerSize;
					cap.visualMode = rule.npcVisualMode;
					cap.soundMode = rule.npcSoundMode;
					return cap;
				}
			}
		}
	}

	// 3. Fallback: Se não achar nenhum perk, puxa as variáveis globais padrões do mod
	cap.parryEnabled = ParrySettings::npcParryEnabled && ParrySettings::HasRequiredPerk(a_actor, ParrySettings::npcParryPerk);
	cap.normalParryMS = ParrySettings::npcNormalParryMS;
	cap.perfectParryMS = ParrySettings::npcPerfectParryMS;
	cap.arrowMode = ParrySettings::HasRequiredPerk(a_actor, ParrySettings::npcArrowParryPerk) ? ParrySettings::npcArrowMode : 0;
	cap.arrowReflectMode = ParrySettings::HasRequiredPerk(a_actor, ParrySettings::npcArrowReflectPerk) ? ParrySettings::npcArrowReflectMode : 0;
	cap.arrowReflectTriggers = ParrySettings::npcArrowReflectTriggers;
	cap.arrowCostType = ParrySettings::npcArrowCostType;
	cap.arrowCost = ParrySettings::npcArrowCost;
	cap.arrowCostGlobal = ParrySettings::npcArrowCostGlobal;
	cap.arrowCostSource = ParrySettings::npcArrowCostSource;
	cap.arrowCostActorValue = ParrySettings::npcArrowCostActorValue;
	cap.magicMode = ParrySettings::HasRequiredPerk(a_actor, ParrySettings::npcMagicParryPerk) ? ParrySettings::npcMagicMode : 0;
	cap.magicReflectMode = ParrySettings::HasRequiredPerk(a_actor, ParrySettings::npcMagicReflectPerk) ? ParrySettings::npcMagicReflectMode : 0;
	cap.magicReflectTriggers = ParrySettings::npcMagicReflectTriggers;
	cap.magicCostType = ParrySettings::npcMagicCostType;
	cap.magicCost = ParrySettings::npcMagicCost;
	cap.magicCostGlobal = ParrySettings::npcMagicCostGlobal;
	cap.magicCostSource = ParrySettings::npcMagicCostSource;
	cap.magicCostActorValue = ParrySettings::npcMagicCostActorValue;
	cap.reflectMeleeMode = ParrySettings::HasRequiredPerk(a_actor, ParrySettings::npcReflectMeleePerk) ? ParrySettings::npcReflectMeleeMode : 0;
	cap.meleeReflectTriggers = ParrySettings::npcMeleeReflectTriggers;
	cap.reflectMeleeCostType = ParrySettings::npcReflectMeleeCostType;
	cap.reflectMeleeCost = ParrySettings::npcReflectMeleeCost;
	cap.reflectMeleeCostGlobal = ParrySettings::npcReflectMeleeCostGlobal;
	cap.reflectMeleeCostSource = ParrySettings::npcReflectMeleeCostSource;
	cap.reflectMeleeCostActorValue = ParrySettings::npcReflectMeleeCostActorValue;
	cap.reflectDamageType = ParrySettings::npcReflectDamageType;
	cap.reflectValue = ParrySettings::npcReflectValue;
	cap.reflectValueGlobal = ParrySettings::npcReflectValueGlobal;
	cap.reflectValueSource = ParrySettings::npcReflectValueSource;
	cap.reflectValueActorValue = ParrySettings::npcReflectValueActorValue;
	cap.meleeStagger = ParrySettings::HasRequiredPerk(a_actor, ParrySettings::npcMeleeStaggerPerk) ? ParrySettings::npcMeleeStagger : 0;
	cap.arrowStagger = ParrySettings::HasRequiredPerk(a_actor, ParrySettings::npcArrowStaggerPerk) ? ParrySettings::npcArrowStagger : 0;
	cap.magicStagger = ParrySettings::HasRequiredPerk(a_actor, ParrySettings::npcMagicStaggerPerk) ? ParrySettings::npcMagicStagger : 0;
	cap.meleeStaggerSize = ParrySettings::npcMeleeStaggerSize;
	cap.arrowStaggerSize = ParrySettings::npcArrowStaggerSize;
	cap.magicStaggerSize = ParrySettings::npcMagicStaggerSize;
	cap.visualMode = ParrySettings::npcVisualMode;
	cap.soundMode = ParrySettings::npcSoundMode;

	return cap;
}

RE::TESObjectWEAP* GetEquippedWeapon(RE::Actor* a_actor)
{
	if (!a_actor) {
		return nullptr;
	}

	for (const bool leftHand : { false, true }) {
		auto* equippedObject = a_actor->GetEquippedObject(leftHand);
		if (equippedObject && equippedObject->IsWeapon()) {
			if (auto* weapon = equippedObject->As<RE::TESObjectWEAP>()) {
				return weapon;
			}
		}
	}

	return nullptr;
}

RE::ActorValue GetAttackerMeleeSkill(RE::Actor* a_attacker) {
    if (!a_attacker) return RE::ActorValue::kUnarmedDamage;

	if (auto* weapon = GetEquippedWeapon(a_attacker)) {
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
	return RE::ActorValue::kUnarmedDamage;
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
	if (!a_actor) return false;

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

bool MatchesReflectTrigger(RE::Actor* a_actor, int a_allowedTriggers) {
	if (!a_actor) return false;
	const int activeTrigger = static_cast<int>(Sink::ParryTimerManager::GetParryTrigger(a_actor->GetFormID()));
	return (a_allowedTriggers & activeTrigger) != 0;
}

bool HasIFrame(RE::Actor* a_actor) {
	bool hasIFrame = false;
	return a_actor && a_actor->GetGraphVariableBool("hasIframeCMF", hasIFrame) && hasIFrame;
}

bool IsUnblockableAttack(RE::Actor* a_attacker) {
	bool isUnblockable = false;
	return a_attacker && a_attacker->GetGraphVariableBool("isUnblockableHit", isUnblockable) && isUnblockable;
}

// Optional block-angle check (off by default): when enabled, a parry only
// happens if the attacker is inside the defender's block cone, so attacks
// from behind are not parried. The cone is the half-angle in degrees on each
// side of the defender's heading: ParrySettings::blockAngle, or the game
// setting fCombatHitConeAngle when blockAngle is 0 (fallback 35).
bool IsAttackerInBlockCone(RE::Actor* a_defender, RE::TESObjectREFR* a_attacker) {
	if (!ParrySettings::respectBlockAngle || !a_defender || !a_attacker) {
		return true;
	}
	float cone = ParrySettings::blockAngle;
	if (cone <= 0.0f) {
		cone = 35.0f;
		if (auto gmst = RE::GameSettingCollection::GetSingleton()) {
			if (auto setting = gmst->GetSetting("fCombatHitConeAngle")) {
				cone = setting->GetFloat();
			}
		}
	}
	const float heading = a_defender->GetHeadingAngle(a_attacker->GetPosition(), false);
	return std::fabs(heading) <= cone;
}

void Hook_OnMeleeHit::processHit(RE::Actor* victim, RE::HitData& hitData)
{
	logger::debug("Melee hit processed");
	auto aggressor = hitData.aggressor.get().get();
	if (!victim || !aggressor) {
		_ProcessHit(victim, hitData);
		return;
	}
	if (HasIFrame(victim)) {
		hitData.totalDamage = 0.0f;
		hitData.physicalDamage = 0.0f;
		hitData.stagger = 0;
		return;
	}
	if (IsUnblockableAttack(aggressor) || (!victim->IsBlocking() && !ParrySettings::bashOpensParryWindow)) {
		_ProcessHit(victim, hitData);
		return;
	}
	if (!IsAttackerInBlockCone(victim, aggressor)) {
		_ProcessHit(victim, hitData);
		return;
	}

	bool isPlayer = (victim == RE::PlayerCharacter::GetSingleton());
	NPCParryCapability cap;
	if (isPlayer) {
		if (!ParrySettings::CanActorUseParry(victim)) {
			_ProcessHit(victim, hitData);
			return;
		}
	}
	else {
		cap = GetNPCParryCapability(victim);
		if (!cap.parryEnabled) {
			_ProcessHit(victim, hitData);
			return;
		}
	}

	Sink::ParryType type = Sink::ParryTimerManager::GetParryType(victim->GetFormID());
	bool perfectParry = false;
	victim->GetGraphVariableBool("isPerfectParryingCMF", perfectParry);
	bool isParryingExternal = false;
	victim->GetGraphVariableBool("isParryingCMF", isParryingExternal);

	if (perfectParry) {
		type = Sink::ParryType::Perfect;
	}
	else if (type == Sink::ParryType::None && isParryingExternal) {
		type = Sink::ParryType::Normal;
	}

	if (type == Sink::ParryType::None) {
		_ProcessHit(victim, hitData);
		return;
	}

	if (!isPlayer && aggressor->IsPlayerRef() && !ParrySettings::npcParryPlayerEnabled) {
		_ProcessHit(victim, hitData);
		return;
	}

	int reflectMode = isPlayer ? (ParrySettings::HasRequiredPerk(victim, ParrySettings::playerReflectMeleePerk) ? ParrySettings::playerReflectMeleeMode : 0) : cap.reflectMeleeMode;
	int reflectTriggers = isPlayer ? ParrySettings::playerMeleeReflectTriggers : cap.meleeReflectTriggers;
	int staggerMode = isPlayer ? (ParrySettings::HasRequiredPerk(victim, ParrySettings::playerMeleeStaggerPerk) ? ParrySettings::playerMeleeStagger : 0) : cap.meleeStagger;
	int staggerSize = isPlayer ? ParrySettings::playerMeleeStaggerSize : cap.meleeStaggerSize;
	bool shouldStagger = (staggerMode == 1) || (staggerMode == 2 && type == Sink::ParryType::Perfect);
	ParrySettings::CostType costType = isPlayer ? ParrySettings::playerReflectMeleeCostType : cap.reflectMeleeCostType;
	float localCost = isPlayer ? ParrySettings::playerReflectMeleeCost : cap.reflectMeleeCost;
	RE::FormID costGlobal = isPlayer ? ParrySettings::playerReflectMeleeCostGlobal : cap.reflectMeleeCostGlobal;
	auto costSource = isPlayer ? ParrySettings::playerReflectMeleeCostSource : cap.reflectMeleeCostSource;
	const std::string& costActorValue = isPlayer ? ParrySettings::playerReflectMeleeCostActorValue : cap.reflectMeleeCostActorValue;
	bool shouldReflect = ((reflectMode == 1) || (reflectMode == 2 && type == Sink::ParryType::Perfect)) &&
		MatchesReflectTrigger(victim, reflectTriggers);

	float cost = ParrySettings::ResolveParryCost(localCost, costSource, costGlobal, costActorValue, victim);

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
			int reduction = (type == Sink::ParryType::Perfect) ?
				ParrySettings::ResolveConfigInt(ParrySettings::playerCommitmentReductionPerfectMS, ParrySettings::playerCommitmentReductionPerfectSource, ParrySettings::playerCommitmentReductionPerfectGlobal, ParrySettings::playerCommitmentReductionPerfectActorValue, victim, 0) :
				ParrySettings::ResolveConfigInt(ParrySettings::playerCommitmentReductionNormalMS, ParrySettings::playerCommitmentReductionNormalSource, ParrySettings::playerCommitmentReductionNormalGlobal, ParrySettings::playerCommitmentReductionNormalActorValue, victim, 0);
			Sink::ParryTimerManager::ReduceCommitment(victim->GetFormID(), reduction);
		}

		auto player = RE::PlayerCharacter::GetSingleton();
		if (victim == player || (aggressor == player && ParrySettings::npcParrySlowTime)) {
			float slowTime = ParrySettings::ResolveConfigValue(ParrySettings::slowTimeMultiplier, ParrySettings::slowTimeMultiplierSource, ParrySettings::slowTimeMultiplierGlobal, ParrySettings::slowTimeMultiplierActorValue, victim);
			int slowDuration = ParrySettings::ResolveConfigInt(ParrySettings::slowTimeDurationMS, ParrySettings::slowTimeDurationSource, ParrySettings::slowTimeDurationGlobal, ParrySettings::slowTimeDurationActorValue, victim, 1);
			Sink::TriggerSlowTime(slowTime, slowDuration);
		}

		if (costType != ParrySettings::CostType::None) {
			RE::ActorValue av = GetAVFromCostType(costType);
			if (av != RE::ActorValue::kNone) {
				victim->AsActorValueOwner()->DamageActorValue(av, cost);
			}
		}
		ParrySettings::ApplyParryRestore(victim, type == Sink::ParryType::Perfect);

		if (shouldReflect) {
			float damageToReflect = hitData.totalDamage;
			// Árvore Condicional Matemática de Modos de Reflexão (Nativo)
			int damageType = isPlayer ? ParrySettings::playerReflectDamageType : cap.reflectDamageType;
			float modifierValue = isPlayer ?
				ParrySettings::ResolveConfigValue(ParrySettings::playerReflectValue, ParrySettings::playerReflectValueSource, ParrySettings::playerReflectValueGlobal, ParrySettings::playerReflectValueActorValue, victim) :
				ParrySettings::ResolveConfigValue(cap.reflectValue, cap.reflectValueSource, cap.reflectValueGlobal, cap.reflectValueActorValue, victim);

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
			float magnitude = ParrySettings::GetStaggerMagnitude(staggerSize, type == Sink::ParryType::Perfect);
			ApplyStagger(aggressor, magnitude, parryVal);
		}

		Sink::PlayParryEffects(victim, nullptr, Sink::ParrySource::Melee, type);
		hitData.totalDamage = 0;
		if (ParrySettings::parryFeedback == 1) {
			victim->NotifyAnimationGraph("recoilStart");
		}
		else if (ParrySettings::parryFeedback == 2) {
			// Keep the native block feedback, but remove the incoming hit's payload.
			hitData.physicalDamage = 0.0f;
			hitData.targetedLimbDamage = 0.0f;
			hitData.resistedPhysicalDamage = 0.0f;
			hitData.resistedTypedDamage = 0.0f;
			hitData.stagger = 0.0f;
			hitData.pushBack = 0.0f;
			hitData.reflectedDamage = 0.0f;
			hitData.sneakAttackBonus = 0.0f;
			hitData.bonusHealthDamageMult = 0.0f;
			hitData.criticalDamageMult = 0.0f;
			hitData.criticalEffect = nullptr;
			hitData.attackDataSpell = nullptr;
			hitData.percentBlocked = 1.0f;
			hitData.flags.reset(RE::HitData::Flag::kCritical, RE::HitData::Flag::kCriticalOnDeath,
				RE::HitData::Flag::kFatal, RE::HitData::Flag::kDismemberLimb,
				RE::HitData::Flag::kExplodeLimb, RE::HitData::Flag::kCrippleLimb,
				RE::HitData::Flag::kDisarm, RE::HitData::Flag::kDisableWeapon);
			hitData.flags.set(RE::HitData::Flag::kBlocked);
			_ProcessHit(victim, hitData);
		}
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

		if (!victim || !aggressor) {
			return ret;
		}
		if (HasIFrame(victim)) {
			ret.bIgnoreHit = true;
			return ret;
		}
		if (ParrySettings::parryFeedback == 2) {
			// Let the native hit hook validate and resolve Hit Block once, with the
			// original damage available for reflection before it sanitizes HitData.
			return ret;
		}
		if ((!victim->IsBlocking() && !ParrySettings::bashOpensParryWindow) || IsUnblockableAttack(aggressor)) {
			return ret;
		}
		if (!IsAttackerInBlockCone(victim, aggressor)) {
			return ret;
		}

		bool isPlayer = (victim == RE::PlayerCharacter::GetSingleton());
		NPCParryCapability cap;
		if (isPlayer) {
			if (!ParrySettings::CanActorUseParry(victim)) {
				return ret;
			}
		}
		else {
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

		if (perfectParry) {
			type = Sink::ParryType::Perfect;
		}
		else if (type == Sink::ParryType::None && isParryingExternal) {
			type = Sink::ParryType::Normal;
		}

		if (type == Sink::ParryType::None) {
			return ret;
		}

		if (!isPlayer && aggressor->IsPlayerRef() && !ParrySettings::npcParryPlayerEnabled) {
			return ret;
		}

		int reflectMode = isPlayer ? (ParrySettings::HasRequiredPerk(victim, ParrySettings::playerReflectMeleePerk) ? ParrySettings::playerReflectMeleeMode : 0) : cap.reflectMeleeMode;
		int reflectTriggers = isPlayer ? ParrySettings::playerMeleeReflectTriggers : cap.meleeReflectTriggers;
		int staggerMode = isPlayer ? (ParrySettings::HasRequiredPerk(victim, ParrySettings::playerMeleeStaggerPerk) ? ParrySettings::playerMeleeStagger : 0) : cap.meleeStagger;
		int staggerSize = isPlayer ? ParrySettings::playerMeleeStaggerSize : cap.meleeStaggerSize;
		bool shouldStagger = (staggerMode == 1) || (staggerMode == 2 && type == Sink::ParryType::Perfect);
		ParrySettings::CostType costType = isPlayer ? ParrySettings::playerReflectMeleeCostType : cap.reflectMeleeCostType;
		float localCost = isPlayer ? ParrySettings::playerReflectMeleeCost : cap.reflectMeleeCost;
		RE::FormID costGlobal = isPlayer ? ParrySettings::playerReflectMeleeCostGlobal : cap.reflectMeleeCostGlobal;
		auto costSource = isPlayer ? ParrySettings::playerReflectMeleeCostSource : cap.reflectMeleeCostSource;
		const std::string& costActorValue = isPlayer ? ParrySettings::playerReflectMeleeCostActorValue : cap.reflectMeleeCostActorValue;
		bool shouldReflect = ((reflectMode == 1) || (reflectMode == 2 && type == Sink::ParryType::Perfect)) &&
			MatchesReflectTrigger(victim, reflectTriggers);

		float cost = ParrySettings::ResolveParryCost(localCost, costSource, costGlobal, costActorValue, victim);

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
				int reduction = (type == Sink::ParryType::Perfect) ?
					ParrySettings::ResolveConfigInt(ParrySettings::playerCommitmentReductionPerfectMS, ParrySettings::playerCommitmentReductionPerfectSource, ParrySettings::playerCommitmentReductionPerfectGlobal, ParrySettings::playerCommitmentReductionPerfectActorValue, victim, 0) :
					ParrySettings::ResolveConfigInt(ParrySettings::playerCommitmentReductionNormalMS, ParrySettings::playerCommitmentReductionNormalSource, ParrySettings::playerCommitmentReductionNormalGlobal, ParrySettings::playerCommitmentReductionNormalActorValue, victim, 0);
				Sink::ParryTimerManager::ReduceCommitment(victim->GetFormID(), reduction);
			}

			if (isPlayer || (aggressor->IsPlayerRef() && ParrySettings::npcParrySlowTime)) {
				float slowTime = ParrySettings::ResolveConfigValue(ParrySettings::slowTimeMultiplier, ParrySettings::slowTimeMultiplierSource, ParrySettings::slowTimeMultiplierGlobal, ParrySettings::slowTimeMultiplierActorValue, victim);
				int slowDuration = ParrySettings::ResolveConfigInt(ParrySettings::slowTimeDurationMS, ParrySettings::slowTimeDurationSource, ParrySettings::slowTimeDurationGlobal, ParrySettings::slowTimeDurationActorValue, victim, 1);
				Sink::TriggerSlowTime(slowTime, slowDuration);
			}

			if (costType != ParrySettings::CostType::None) {
				RE::ActorValue av = GetAVFromCostType(costType);
				if (av != RE::ActorValue::kNone) {
					victim->AsActorValueOwner()->DamageActorValue(av, cost);
				}
			}
			ParrySettings::ApplyParryRestore(victim, type == Sink::ParryType::Perfect);

			if (shouldReflect) {
				float damageToReflect = cost;
				// NOVO: Árvore Condicional Matemática de Modos de Reflexão (Precision API)
				int damageType = isPlayer ? ParrySettings::playerReflectDamageType : cap.reflectDamageType;
				float modifierValue = isPlayer ?
					ParrySettings::ResolveConfigValue(ParrySettings::playerReflectValue, ParrySettings::playerReflectValueSource, ParrySettings::playerReflectValueGlobal, ParrySettings::playerReflectValueActorValue, victim) :
					ParrySettings::ResolveConfigValue(cap.reflectValue, cap.reflectValueSource, cap.reflectValueGlobal, cap.reflectValueActorValue, victim);

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
				float magnitude = ParrySettings::GetStaggerMagnitude(staggerSize, type == Sink::ParryType::Perfect);
				ApplyStagger(aggressor, magnitude, parryVal);
			}

			Sink::PlayParryEffects(victim, nullptr, Sink::ParrySource::Melee, type);
			if (ParrySettings::parryFeedback == 1) {
				victim->NotifyAnimationGraph("recoilStart");
			}
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
	if (!a_blocker || !a_projectile) return false;
	if (HasIFrame(a_blocker)) {
		RE::Offset::destroyProjectile(a_projectile);
		return true;
	}

	auto player = RE::PlayerCharacter::GetSingleton();
	bool isPlayer = (a_blocker == player);
	auto shooterHandle = a_projectile->GetProjectileRuntimeData().shooter;
	auto shooterRef = shooterHandle.get();
	auto* shooter = shooterRef ? shooterRef->As<RE::Actor>() : nullptr;

	if (shooter && shooter->IsPlayerRef() && !ParrySettings::npcParryPlayerEnabled) {
		return false;
	}

	if (IsUnblockableAttack(shooter)) {
		return false;
	}
	if (shooter && !IsAttackerInBlockCone(a_blocker, shooter)) {
		return false;
	}

	NPCParryCapability cap;
	if (isPlayer) {
		if (!ParrySettings::CanActorUseParry(a_blocker)) {
			return false;
		}
	}
	else {
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

	if (perfectParry) {
		type = Sink::ParryType::Perfect;
	}
	else if (type == Sink::ParryType::None && isParryingExternal) {
		type = Sink::ParryType::Normal;
	}

	if (type == Sink::ParryType::None) return false;

	bool isArrow = !a_projectile->GetProjectileRuntimeData().spell;

	int currentMode = 0;
	if (isArrow) {
		currentMode = isPlayer ? (ParrySettings::HasRequiredPerk(a_blocker, ParrySettings::playerArrowParryPerk) ? ParrySettings::playerArrowMode : 0) : cap.arrowMode;
	}
	else {
		currentMode = isPlayer ? (ParrySettings::HasRequiredPerk(a_blocker, ParrySettings::playerMagicParryPerk) ? ParrySettings::playerMagicMode : 0) : cap.magicMode;
	}

	if (currentMode == 0) return false;

	float cost = 0.0f;
	ParrySettings::CostType costType = ParrySettings::CostType::None;

	if (currentMode == 2) {
		costType = isArrow ? (isPlayer ? ParrySettings::playerArrowCostType : cap.arrowCostType)
			: (isPlayer ? ParrySettings::playerMagicCostType : cap.magicCostType);
		float localCost = isArrow ? (isPlayer ? ParrySettings::playerArrowCost : cap.arrowCost)
			: (isPlayer ? ParrySettings::playerMagicCost : cap.magicCost);
		RE::FormID costGlobal = isArrow ? (isPlayer ? ParrySettings::playerArrowCostGlobal : cap.arrowCostGlobal)
			: (isPlayer ? ParrySettings::playerMagicCostGlobal : cap.magicCostGlobal);
		auto costSource = isArrow ? (isPlayer ? ParrySettings::playerArrowCostSource : cap.arrowCostSource)
			: (isPlayer ? ParrySettings::playerMagicCostSource : cap.magicCostSource);
		const std::string& costActorValue = isArrow ? (isPlayer ? ParrySettings::playerArrowCostActorValue : cap.arrowCostActorValue)
			: (isPlayer ? ParrySettings::playerMagicCostActorValue : cap.magicCostActorValue);

		cost = ParrySettings::ResolveParryCost(localCost, costSource, costGlobal, costActorValue, a_blocker);
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

	int reflectModeSetting = 0;
	int reflectTriggers = 1;
	if (isArrow) {
		reflectModeSetting = isPlayer ? (ParrySettings::HasRequiredPerk(a_blocker, ParrySettings::playerArrowReflectPerk) ? ParrySettings::playerArrowReflectMode : 0) : cap.arrowReflectMode;
		reflectTriggers = isPlayer ? ParrySettings::playerArrowReflectTriggers : cap.arrowReflectTriggers;
	}
	else {
		reflectModeSetting = isPlayer ? (ParrySettings::HasRequiredPerk(a_blocker, ParrySettings::playerMagicReflectPerk) ? ParrySettings::playerMagicReflectMode : 0) : cap.magicReflectMode;
		reflectTriggers = isPlayer ? ParrySettings::playerMagicReflectTriggers : cap.magicReflectTriggers;
	}

	bool shouldReflect = ((reflectModeSetting == 1) || (reflectModeSetting == 2 && type == Sink::ParryType::Perfect)) &&
		MatchesReflectTrigger(a_blocker, reflectTriggers);
	int staggerMode = 0;
	int staggerSize = 1;
	if (isArrow) {
		staggerMode = isPlayer ? (ParrySettings::HasRequiredPerk(a_blocker, ParrySettings::playerArrowStaggerPerk) ? ParrySettings::playerArrowStagger : 0) : cap.arrowStagger;
		staggerSize = isPlayer ? ParrySettings::playerArrowStaggerSize : cap.arrowStaggerSize;
	}
	else {
		staggerMode = isPlayer ? (ParrySettings::HasRequiredPerk(a_blocker, ParrySettings::playerMagicStaggerPerk) ? ParrySettings::playerMagicStagger : 0) : cap.magicStagger;
		staggerSize = isPlayer ? ParrySettings::playerMagicStaggerSize : cap.magicStaggerSize;
	}

	bool shouldStagger = (staggerMode == 1) || (staggerMode == 2 && type == Sink::ParryType::Perfect);

	if (shouldStagger) {
		auto shooterReference = a_projectile->GetProjectileRuntimeData().shooter.get();
		if (auto* shooterActor = shooterReference ? shooterReference->As<RE::Actor>() : nullptr) {
			int parryVal = (type == Sink::ParryType::Perfect ? 2 : 1);
			shooterActor->SetGraphVariableInt("ShouldStaggerCMF", parryVal);
			shooterActor->SetGraphVariableFloat("ShouldStaggerMagnitudeCMF", ParrySettings::GetStaggerMagnitude(staggerSize, type == Sink::ParryType::Perfect));
		}
	}

	if (a_blocker == player || (shooter == player && ParrySettings::npcParrySlowTime)) {
		float slowTime = ParrySettings::ResolveConfigValue(ParrySettings::slowTimeMultiplier, ParrySettings::slowTimeMultiplierSource, ParrySettings::slowTimeMultiplierGlobal, ParrySettings::slowTimeMultiplierActorValue, a_blocker);
		int slowDuration = ParrySettings::ResolveConfigInt(ParrySettings::slowTimeDurationMS, ParrySettings::slowTimeDurationSource, ParrySettings::slowTimeDurationGlobal, ParrySettings::slowTimeDurationActorValue, a_blocker, 1);
		Sink::TriggerSlowTime(slowTime, slowDuration);
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
	ParrySettings::ApplyParryRestore(a_blocker, type == Sink::ParryType::Perfect);

	if (isPlayer && ParrySettings::playerParryCommitmentEnabled) {
		int reduction = (type == Sink::ParryType::Perfect) ?
			ParrySettings::ResolveConfigInt(ParrySettings::playerCommitmentReductionPerfectMS, ParrySettings::playerCommitmentReductionPerfectSource, ParrySettings::playerCommitmentReductionPerfectGlobal, ParrySettings::playerCommitmentReductionPerfectActorValue, a_blocker, 0) :
			ParrySettings::ResolveConfigInt(ParrySettings::playerCommitmentReductionNormalMS, ParrySettings::playerCommitmentReductionNormalSource, ParrySettings::playerCommitmentReductionNormalGlobal, ParrySettings::playerCommitmentReductionNormalActorValue, a_blocker, 0);
		Sink::ParryTimerManager::ReduceCommitment(a_blocker->GetFormID(), reduction);
	}

	return true;
}

inline bool shouldIgnoreHit(RE::Projectile* a_projectile, RE::hkpAllCdPointCollector* a_AllCdPointCollector)
{
	if (!a_projectile || !a_AllCdPointCollector) {
		return false;
	}

	for (auto& hit : a_AllCdPointCollector->hits) {
		if (!hit.rootCollidableA || !hit.rootCollidableB) {
			continue;
		}

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
				float configuredMagnitude = 0.0f;
				if (target->GetGraphVariableFloat("ShouldStaggerMagnitudeCMF", configuredMagnitude) && configuredMagnitude > 0.0f) {
					magnitude = configuredMagnitude;
				}

				logger::debug("Applying pending stagger to {} (Type: {})", target->GetName(), pendingStagger);
				ApplyStagger(target, magnitude, pendingStagger);

				// Reseta para 0 após aplicar
				target->SetGraphVariableInt("ShouldStaggerCMF", 0);
				target->SetGraphVariableFloat("ShouldStaggerMagnitudeCMF", 0.0f);
			}
		}
	}
	return false;
}

void Hook_OnProjectileCollision::OnArrowCollision(RE::Projectile* a_this, RE::hkpAllCdPointCollector* a_AllCdPointCollector)
{
	logger::debug("Arrow collided");
	if (a_this && shouldIgnoreHit(a_this, a_AllCdPointCollector)) {
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
	auto* attackerWeapon = GetEquippedWeapon(hit_causer);
	auto* defenderWeapon = GetEquippedWeapon(hit_target);

	float attackerDamage = 0.0f;
	if (attackerWeapon) {
		attackerDamage = attackerWeapon->GetAttackDamage();
	}

	float defenderDamage = 0.0f;
	if (defenderWeapon) {
		defenderDamage = defenderWeapon->GetAttackDamage();
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

	const RE::FormID winnerFormID = attackerWins ? hit_causer->GetFormID() : hit_target->GetFormID();
	// 5. Verifica quem deve receber a reação com base na UI
	bool affectAttacker = (ParrySettings::w2wReactionTarget == 1 || ParrySettings::w2wReactionTarget == 3);
	bool affectDefender = (ParrySettings::w2wReactionTarget == 2 || ParrySettings::w2wReactionTarget == 3);

	const auto hitCauserHandle = hit_causer->CreateRefHandle();
	const auto hitTargetHandle = hit_target->CreateRefHandle();

	// Aplica os Graph Events através de uma Task no SKSE para thread safety
	SKSE::GetTaskInterface()->AddTask([hitCauserHandle, hitTargetHandle, winnerFormID, affectAttacker, affectDefender]() {

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

		// Handles prevent a queued task from dereferencing actors unloaded after
		// the Precision callback returned.
		if (affectAttacker) {
			if (auto hitCauser = hitCauserHandle.get(); hitCauser) {
				if (auto* actor = hitCauser->As<RE::Actor>()) {
					applyReaction(actor, actor->GetFormID() == winnerFormID);
				}
			}
		}

		if (affectDefender) {
			if (auto hitTarget = hitTargetHandle.get(); hitTarget) {
				if (auto* actor = hitTarget->As<RE::Actor>()) {
					applyReaction(actor, actor->GetFormID() == winnerFormID);
				}
			}
		}
		});

	// Retorna bIgnoreHit = true para que o jogo ignore o dano físico do choque e trate apenas como a colisão das armas
	ret.bIgnoreHit = true;
	return ret;
}
