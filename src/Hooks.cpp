#include "Hooks.h"
#include "Settings.h"


void ApplyStagger(RE::Actor* a_target, float a_magnitude, int a_parryValue) {
	if (!a_target) return;
	a_target->SetGraphVariableInt("GotParriedCMF", a_parryValue);
	a_target->SetGraphVariableFloat("staggerMagnitude", a_magnitude);
	a_target->NotifyAnimationGraph("staggerStart");
	
	/*SKSE::GetTaskInterface()->AddTask([a_target]() {
		if (a_target) {
			a_target->SetGraphVariableInt("GotParriedCMF", 0);
		}
		});*/
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
	aggressor->GetGraphVariableBool("UnblockableAttackCMF", isUnblockble);

	if (!victim || !aggressor || isUnblockble || !victim->IsBlocking()) {
		_ProcessHit(victim, hitData);
		return;
	}

	Sink::ParryType type = Sink::ParryTimerManager::GetParryType(victim->GetFormID());
	bool isPlayer = (victim == RE::PlayerCharacter::GetSingleton());

	if (type != Sink::ParryType::None && !isUnblockble) {
		if (!isPlayer && aggressor->IsPlayerRef() && !ParrySettings::npcParryPlayerEnabled) {
			_ProcessHit(victim, hitData);
			return;
		}
		int reflectMode = isPlayer ? ParrySettings::playerReflectMeleeMode : ParrySettings::npcReflectMeleeMode;
		int staggerMode = isPlayer ? ParrySettings::playerMeleeStagger : ParrySettings::npcMeleeStagger;
		bool shouldStagger = (staggerMode == 1) || (staggerMode == 2 && type == Sink::ParryType::Perfect);
		ParrySettings::CostType costType = isPlayer ? ParrySettings::playerReflectMeleeCostType : ParrySettings::npcReflectMeleeCostType;

		// 1. Verificar se deve refletir
		bool shouldReflect = (reflectMode == 1) || (reflectMode == 2 && type == Sink::ParryType::Perfect);

		float cost = hitData.totalDamage;

		if (HasEnoughResource(victim, costType, cost)) {
			auto player = RE::PlayerCharacter::GetSingleton();
			if (!Sink::g_IsSlowed) {
				if (victim == player || (aggressor == player && ParrySettings::npcParrySlowTime)) {
					Sink::ApplySlowTime(ParrySettings::slowTimeMultiplier);
					Sink::ResetTimeTask();
				}
			}
			// Paga o custo
			if (costType != ParrySettings::CostType::None) {
				RE::ActorValue av = (costType == ParrySettings::CostType::Stamina) ? RE::ActorValue::kStamina : RE::ActorValue::kMagicka;
				victim->AsActorValueOwner()->DamageActorValue(av, cost);
			}

			// Aplica Reflexão: Devolve o dano ao atacante
			if (shouldReflect) {
				aggressor->DoDamage(hitData.totalDamage, aggressor, false);
			}

			// Aplica Stagger
			if (shouldStagger) {
				int parryVal = (type == Sink::ParryType::Perfect ? 2 : 1);
				float magnitude = (type == Sink::ParryType::Perfect ? 1.0f : 0.5f);
				ApplyStagger(aggressor, magnitude, parryVal);
			}

			// Efeitos Visuais
			Sink::PlayParryEffects(victim, nullptr, Sink::ParrySource::Melee, type);
			logger::debug("Parry succeeded on melee hit by actor {}", victim->GetName());
			// Zera o dano que a vítima receberia
			hitData.totalDamage = 0;
			return; // Bloqueio completo
		}
	}
	// Chamar a função original
	_ProcessHit(victim, hitData);
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
	logger::info("Actor {} is blocking projectile {}", a_blocker->GetName(), a_projectile->GetName());
	Sink::ParryType type = Sink::ParryTimerManager::GetParryType(a_blocker->GetFormID());
	if (type == Sink::ParryType::None) return false;

	auto shooterHandle = a_projectile->GetProjectileRuntimeData().shooter;
	auto shooter = shooterHandle.get().get() ? shooterHandle.get().get()->As<RE::Actor>() : nullptr;

	if (shooter && shooter->IsPlayerRef() && !ParrySettings::npcParryPlayerEnabled) {
		return false;
	}

	bool isArrow = !a_projectile->GetProjectileRuntimeData().spell;

	int currentMode = isArrow ? (isPlayer ? ParrySettings::playerArrowMode : ParrySettings::npcArrowMode)
		: (isPlayer ? ParrySettings::playerMagicMode : ParrySettings::npcMagicMode);

	if (currentMode == 0) return false;

	// --- CÁLCULO DE CUSTO ---
	float cost = 0.0f;
	ParrySettings::CostType costType = ParrySettings::CostType::None;

	if (currentMode == 2) { // Modo com Custo
		costType = isArrow ? (isPlayer ? ParrySettings::playerArrowCostType : ParrySettings::npcArrowCostType)
			: (isPlayer ? ParrySettings::playerMagicCostType : ParrySettings::npcMagicCostType);

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

	// --- VERIFICAÇÃO DE RECURSO (NOVO) ---
	if (!HasEnoughResource(a_blocker, costType, cost)) {
		return false; // Não tem recurso -> o parry falha e ele leva o dano
	}

	// --- DETERMINAR REFLEXÃO ---
	int reflectModeSetting = isArrow ? (isPlayer ? ParrySettings::playerArrowReflectMode : ParrySettings::npcArrowReflectMode)
		: (isPlayer ? ParrySettings::playerMagicReflectMode : ParrySettings::npcMagicReflectMode);

	bool shouldReflect = (reflectModeSetting == 1) || (reflectModeSetting == 2 && type == Sink::ParryType::Perfect);
	int staggerMode = isArrow ? (isPlayer ? ParrySettings::playerArrowStagger : ParrySettings::npcArrowStagger)
		: (isPlayer ? ParrySettings::playerMagicStagger : ParrySettings::npcMagicStagger);

	bool shouldStagger = (staggerMode == 1) || (staggerMode == 2 && type == Sink::ParryType::Perfect);

	if (shouldStagger) {
		auto shooter = a_projectile->GetProjectileRuntimeData().shooter.get().get();
		if (shooter && shooter->As<RE::Actor>()) {
			int parryVal = (type == Sink::ParryType::Perfect ? 2 : 1);
			float magnitude = (type == Sink::ParryType::Perfect ? 1.0f : 0.5f);
			ApplyStagger(shooter->As<RE::Actor>(), magnitude, parryVal);
		}
	}

	

	if (!Sink::g_IsSlowed) {
		// a_blocker é quem está defendendo o projétil
		if (a_blocker == player || (shooter == player && ParrySettings::npcParrySlowTime)) {
			Sink::ApplySlowTime(ParrySettings::slowTimeMultiplier);
			Sink::ResetTimeTask();
		}
	}

	// 2. Tocar Efeitos
	Sink::PlayParryEffects(a_blocker, a_projectile, isArrow ? Sink::ParrySource::Arrow : Sink::ParrySource::Magic, type);
	// --- EXECUTAR ---
	// Aplica o dano no recurso
	if (cost > 0.0f) {
		RE::ActorValue resourceAV = (costType == ParrySettings::CostType::Stamina) ? RE::ActorValue::kStamina : RE::ActorValue::kMagicka;
		a_blocker->AsActorValueOwner()->DamageActorValue(resourceAV, cost);
	}

	if (shouldReflect) {
		ReflectProjectileBack(a_blocker, a_projectile, a_projectile_collidable);
	}
	else {
		a_projectile->SetDelete(true); // Apenas anula o projétil
	}

	return true;
}

inline bool shouldIgnoreHit(RE::Projectile* a_projectile, RE::hkpAllCdPointCollector* a_AllCdPointCollector)
{
	if (a_AllCdPointCollector) {
		for (auto& hit : a_AllCdPointCollector->hits) {
			auto refrA = RE::TESHavokUtilities::FindCollidableRef(*hit.rootCollidableA);
			auto refrB = RE::TESHavokUtilities::FindCollidableRef(*hit.rootCollidableB);
			if (refrA && refrA->formType == RE::FormType::ActorCharacter) {
				if (processProjectileBlock(refrA->As<RE::Actor>(), a_projectile, const_cast<RE::hkpCollidable*>(hit.rootCollidableB))) {
					return true;
				};
			}
			if (refrB && refrB->formType == RE::FormType::ActorCharacter) {
				if (processProjectileBlock(refrB->As<RE::Actor>(), a_projectile, const_cast<RE::hkpCollidable*>(hit.rootCollidableA))) {
					return true;
				};
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


