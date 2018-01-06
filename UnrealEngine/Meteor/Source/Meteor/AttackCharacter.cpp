// Copyright IceRiver. All Rights Reserved.

#include "AttackCharacter.h"
#include "Meteor.h"
#include "Animation/AnimInstance.h"
#include "MyAnimMetaData.h"
#include "Engine.h"
#include "Meteor/Common/MeteorFuncLib.h"
#include "Meteor/Components/InputBufferComponent.h"
#include "Meteor/Components/InputCommandComponent.h"
#include "Meteor/Components/CombatSystemComponent.h"

AAttackCharacter::AAttackCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	MoveFwdSpeedFactor = 1.0f;

	MoveBwdSpeedFactor = 0.3f;

	MoveRightSpeedFactor = 0.3f;

	InputCommandCP = CreateDefaultSubobject<UInputCommandComponent>(TEXT("InputCommand"));
	AddTickPrerequisiteComponent(InputCommandCP);

	CombatSystemCP = CreateDefaultSubobject<UCombatSystemComponent>(TEXT("CombatSystem"));
	AddTickPrerequisiteComponent(CombatSystemCP);
	CombatSystemCP->SetInputCommandComponent(InputCommandCP);
}

void AAttackCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void AAttackCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AAttackCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	//Super::SetupPlayerInputComponent(PlayerInputComponent);

	check(PlayerInputComponent);

	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &AAttackCharacter::OnJump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &AAttackCharacter::StopJump);

	PlayerInputComponent->BindAxis("MoveForward", this, &AAttackCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AAttackCharacter::MoveRight);

	PlayerInputComponent->BindAxis("Turn", this, &AAttackCharacter::Turn);
	PlayerInputComponent->BindAxis("LookUp", this, &AAttackCharacter::LookUp);
}

void AAttackCharacter::MoveForward(float Value)
{
	if (Value > 0.0f)
		Value *= MoveFwdSpeedFactor;
	else
		Value *= MoveBwdSpeedFactor;

	AddMovementInput(GetActorForwardVector(), Value);
}

void AAttackCharacter::MoveRight(float Value)
{
	AddMovementInput(GetActorRightVector(), Value * MoveRightSpeedFactor);
}

void AAttackCharacter::Turn(float Value)
{
	if (CombatSystemCP->MoveType == MOVE_TYPE::MOVE_IDLE)
	{
		AddControllerYawInput(Value);
	}
}

void AAttackCharacter::LookUp(float Value)
{
	AddControllerPitchInput(Value);
}

void AAttackCharacter::OnJump()
{
	if (CombatSystemCP->MoveType == MOVE_TYPE::MOVE_IDLE)
	{
		ACharacter::Jump();
	}
}

void AAttackCharacter::StopJump()
{
	ACharacter::StopJumping();
}
