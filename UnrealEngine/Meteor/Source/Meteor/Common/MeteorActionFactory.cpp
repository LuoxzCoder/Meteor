
#include "MeteorActionFactory.h"
#include "AttackCharacter.h"


FMeteorActionStateFactory::FMeteorActionStateFactory()
{
	IdlePool = FMeteorActionPool<FIdleState>(4);
	AttackPool = FMeteorActionPool<FAttackState>(4);
	GuardPool = FMeteorActionPool<FGuardState>(2);
}

FIdleState* FMeteorActionStateFactory::CreateIdleState(AAttackCharacter* Owner)
{
	return IdlePool.CreateAction(Owner);
}

FAttackState* FMeteorActionStateFactory::CreateAttackState(AAttackCharacter* Owner)
{
	return AttackPool.CreateAction(Owner);
}

FGuardState* FMeteorActionStateFactory::CreateGuardState(AAttackCharacter* Owner)
{
	return GuardPool.CreateAction(Owner);
}
