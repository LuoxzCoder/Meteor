// Copyright IceRiver. All Rights Reserved.

#include "AttackCharacter.h"
#include "Meteor.h"
#include "Animation/AnimInstance.h"
#include "MyAnimMetaData.h"
#include "Engine.h"
#include "Common/MeteorSingletonLibrary.h"

AAttackCharacter::AAttackCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	CurrentPoseIndex = -1;

	bAcceptInput = false;

	NextPoseTime = 0.2f;

	MoveFwdSpeedFactor = 1.0f;

	MoveBwdSpeedFactor = 0.3f;

	MoveRightSpeedFactor = 0.3f;

	AttackState = ATTACK_STATE::ATK_IDLE;

	NextPoseTimer = 0.0f;

	NextPoseMtg = nullptr;

	bIsSprinting = false;

	CurrentAnimFlag = EAnimFlag::Unknow;

	InputBufferCP = CreateDefaultSubobject<UInputBufferComponent>(TEXT("InputBuffer"));

	AddTickPrerequisiteComponent(InputBufferCP);

	MaxSprintWaitTime = 0.2f;

	SprintHoldTimer = 0.0f;
}

void AAttackCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	StreamMgr.LoadSynchronous(PoseInfoTable);
	StreamMgr.LoadSynchronous(Dao_PoseChangeTable);

	if (PoseInfoTable)
	{
		PoseInfoTable->GetAllRows(this->GetName(), Dao_AllPoses);
	}
}

void AAttackCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!InputBufferCP)
		return;

	InputBufferCP->UpdateInputBuffer();
	InputBufferCP->DebugInputBuffer();

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance == nullptr)
		return;

	// 跳跃和普通招式互斥，下面的判断过于简单，最好给每个攻击招式一个标签，标识其是地面攻击招式，或者空中攻击招式
	UAnimMontage* ActiveMontage = AnimInstance->GetCurrentActiveMontage();
	if (ActiveMontage)
	{
		bIsAttacking = true;
		bIsSprinting = true;
		CurrentAnimFlag = GetAnimFlag(ActiveMontage);
	}
	else
	{
		bIsAttacking = false;
		bIsSprinting = false;
		CurrentAnimFlag = EAnimFlag::Unknow;
	}

	switch (AttackState)
	{
	case AAttackCharacter::ATK_IDLE:
	{
		// 处理普通的攻击操作
		if (InputBufferCP->bAttackKeyDown && GetCharacterMovement()->MovementMode != EMovementMode::MOVE_Falling)
		{
			CurrentPoseIndex = GenerateAttackCommond(CurrentPoseIndex);

			bAcceptInput = false;
			NextPoseTime = 0.2f;
			NextPoseTimer = 0.0f;
			NextPoseMtg = nullptr;

			if (CurrentPoseIndex != -1)
			{
				SprintHoldTimer = 0.0f;
				UAnimMontage* poseMtg = GetPoseMontage(CurrentPoseIndex);
				if (poseMtg)
				{
					GetAnimMetaData(poseMtg);
					AnimInstance->Montage_Stop(0.0f);
					AnimInstance->Montage_Play(poseMtg);
					AttackState = ATK_PLAYING;
					GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Falling);
				}
			}
		}
		// 处理Sprint
		else if (GetCharacterMovement()->MovementMode != EMovementMode::MOVE_Falling &&
			bIsAttacking == false && bIsSprinting == false)
		{
			TArray<FString> AllInputCmds;
			AllInputCmds = InputBufferCP->GetInputCommonds();

			SPRINT_DIRECTION SprintDir = GetReadySprintDirection(AllInputCmds);
			if (SprintDir != SPRINT_DIRECTION::SPRINT_IDLE)
			{
				SprintHoldTimer += DeltaTime;
				if (SprintHoldTimer > MaxSprintWaitTime)
				{
					SprintHoldTimer = 0.0f;
					switch (SprintDir)
					{
					case AAttackCharacter::SPRINT_DIRECTION::SPRINT_FWD:
						AnimInstance->Montage_Play(SprintForwadMtg, 0.8f);
						break;
					case AAttackCharacter::SPRINT_DIRECTION::SPRINT_BWD:
						AnimInstance->Montage_Play(SprintBackwardMtg, 0.8f);
						break;
					case AAttackCharacter::SPRINT_DIRECTION::SPRINT_RIGHT:
						AnimInstance->Montage_Play(SprintRightMtg, 0.8f);
						break;
					case AAttackCharacter::SPRINT_DIRECTION::SPRINT_LEFT:
						AnimInstance->Montage_Play(SprintLeftMtg, 0.8f);
						break;
					default:
						break;
					}
				}
			}
		}
		break;
	}
	case AAttackCharacter::ATK_PLAYING:
	{
		if (ActiveMontage)
		{
			FName sectionName = AnimInstance->Montage_GetCurrentSection(ActiveMontage);

			// 设置playRate
			float* ratio = SectionRatioCache.Find(sectionName);
			if (ratio)
			{
				//UE_LOG(LogTemp, Warning, TEXT("%s %s: play rate %f"), *(ActiveMontage->GetName()), *(sectionName.ToString()), *ratio);
				AnimInstance->Montage_SetPlayRate(ActiveMontage, *ratio);
			}

			// 检测是否需要跳转
			if (sectionName.IsEqual(NextPoseOut) && InputBufferCP->bAttackKeyDown)
			{
				CurrentPoseIndex = GenerateAttackCommond(CurrentPoseIndex);
				if (CurrentPoseIndex != -1)
				{
					UAnimMontage* poseMtg = GetPoseMontage(CurrentPoseIndex);
					if (poseMtg)
					{
						GetAnimMetaData(poseMtg);

						// 执行实际的过渡，主要使用的功能是，当一个Montage过渡到下一个Montage时，会直接从当前位置过渡到下一个位置，不过会出现blend
						tmpBlend = poseMtg->BlendIn;
						poseMtg->BlendIn.SetBlendTime(NextPoseTime);
						AnimInstance->Montage_Play(poseMtg);
						AnimInstance->Montage_JumpToSection(NextPoseIn, poseMtg);
						AnimInstance->Montage_Pause(poseMtg);

						NextPoseMtg = poseMtg;
						NextPoseTimer = 0.0f;

						AttackState = ATK_NEXTPOSE;
					}
				}
			}
		}
		else// 如果当前动画已经播完，则进入Idle状态
		{
			AttackState = ATK_IDLE;
			CurrentPoseIndex = -1;
			GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Walking);
		}
		break;
	}
	case AAttackCharacter::ATK_NEXTPOSE:
	{
		NextPoseTimer += DeltaTime;

		if (NextPoseTimer > NextPoseTime)
		{
			AnimInstance->Montage_Resume(NextPoseMtg);
			AttackState = ATK_PLAYING;
			GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Falling);
		}
		break;
	}
	default:
		break;
	}
}


void AAttackCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	//Super::SetupPlayerInputComponent(PlayerInputComponent);

	check(PlayerInputComponent);

	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &AAttackCharacter::OnJump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &AAttackCharacter::StopJump);

	PlayerInputComponent->BindAction("Attack", IE_Pressed, this, &AAttackCharacter::OnAttack);
	PlayerInputComponent->BindAction("Attack", IE_Released, this, &AAttackCharacter::StopAttack);

	PlayerInputComponent->BindAxis("MoveForward", this, &AAttackCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AAttackCharacter::MoveRight);

	PlayerInputComponent->BindAxis("Turn", this, &AAttackCharacter::Turn);
	PlayerInputComponent->BindAxis("LookUp", this, &AAttackCharacter::LookUp);

	PlayerInputComponent->BindAction("Up", IE_Pressed, InputBufferCP, &UInputBufferComponent::OnUpKeyDown);
	PlayerInputComponent->BindAction("Up", IE_Released, InputBufferCP, &UInputBufferComponent::OnUpKeyRelease);

	PlayerInputComponent->BindAction("Down", IE_Pressed, InputBufferCP, &UInputBufferComponent::OnDownKeyDown);
	PlayerInputComponent->BindAction("Down", IE_Released, InputBufferCP, &UInputBufferComponent::OnDownKeyRelease);

	PlayerInputComponent->BindAction("Right", IE_Pressed, InputBufferCP, &UInputBufferComponent::OnRightKeyDown);
	PlayerInputComponent->BindAction("Right", IE_Released, InputBufferCP, &UInputBufferComponent::OnRightKeyRelease);

	PlayerInputComponent->BindAction("Left", IE_Pressed, InputBufferCP, &UInputBufferComponent::OnLeftKeyDown);
	PlayerInputComponent->BindAction("Left", IE_Released, InputBufferCP, &UInputBufferComponent::OnLeftKeyRelease);
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
	if (AttackState == ATK_IDLE)
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
	if (bIsAttacking == false && bIsSprinting == false)
	{
		ACharacter::Jump();
	}
}

void AAttackCharacter::StopJump()
{
	ACharacter::StopJumping();
}

void AAttackCharacter::OnAttack()
{
	if (InputBufferCP == nullptr)
		return;

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance)
	{
		UAnimMontage* activeMontage = AnimInstance->GetCurrentActiveMontage();
		if (activeMontage && bAcceptInput)
		{
			InputBufferCP->bAttackKeyDown = true;
			//InputBufferCP->PushAttackKey(UInputBufferComponent::ATTACK_KEY::KEY_A, GetWorld()->GetTimeSeconds());
		}
		else if (activeMontage == nullptr)
		{
			InputBufferCP->bAttackKeyDown = true;
			//InputBufferCP->PushAttackKey(UInputBufferComponent::ATTACK_KEY::KEY_A, GetWorld()->GetTimeSeconds());
		}
	}
}

void AAttackCharacter::StopAttack()
{
	
}

UAnimMontage* AAttackCharacter::GetPoseMontage(int32 poseIndex)
{
	FString poseStr = FString::FromInt(poseIndex);

	if (PoseInfoTable)
	{
		FPoseInputTable* PoseInfo = PoseInfoTable->FindRow<FPoseInputTable>(FName(*poseStr), "");
		if (PoseInfo)
		{
			UAnimMontage* poseMtg = PoseInfo->PoseMontage.Get();
			if (poseMtg == nullptr)
			{
				poseMtg = StreamMgr.LoadSynchronous(PoseInfo->PoseMontage);

				//poseMtg = PoseInfo->PoseMontage.Get();
				if (poseMtg)
				{
					GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Red, FString::Printf(TEXT("Load %s succeed!"), *(poseMtg->GetName())));
				}
				else
				{
					GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Red, FString::Printf(TEXT("Load  %s error!"), *poseStr));
				}
			}
			return poseMtg;
		}
	}
	return nullptr;
}

void AAttackCharacter::GetAnimMetaData(UAnimMontage* montage)
{
	if (montage)
	{
		SectionRatioCache.Empty(4);
		const TArray<UAnimMetaData*> MetaDatas = montage->GetMetaData();
		for (int i = 0; i < MetaDatas.Num(); ++i)
		{
			UAnimMetaData_SectionInfo* sectionInfo = Cast<UAnimMetaData_SectionInfo>(MetaDatas[i]);
			if (sectionInfo)
			{
				SectionRatioCache = sectionInfo->SectionSpeeds;
				NextPoseIn = sectionInfo->NextPoseIn;
				NextPoseOut = sectionInfo->NextPoseOut;
				break;
			}
		}
	}
}

EAnimFlag AAttackCharacter::GetAnimFlag(UAnimMontage* montage)
{
	if (montage)
	{
		const TArray<UAnimMetaData*> MetaDatas = montage->GetMetaData();
		for (int i = 0; i < MetaDatas.Num(); ++i)
		{
			UAnimMetaData_Flag* flag = Cast<UAnimMetaData_Flag>(MetaDatas[i]);
			if (flag)
			{
				return flag->AnimFlag;
			}
		}
	}
	return EAnimFlag::Unknow;
}

// inputCmds 中排在前面的cmd优先可能被触发
int32 AAttackCharacter::GetNextPose(int32 poseIdx, const TArray<FString>& inputCmds, bool bInAir)
{
	if (PoseInfoTable == nullptr)
	{
		StreamMgr.LoadSynchronous(PoseInfoTable);
		GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Cyan, FString::Printf(TEXT("Reload  %s!"), *PoseInfoTable->GetName()));
	}
	if (Dao_PoseChangeTable == nullptr)
	{
		StreamMgr.LoadSynchronous(Dao_PoseChangeTable);
		GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Cyan, FString::Printf(TEXT("Reload  %s!"), *Dao_PoseChangeTable->GetName()));
	}
	if (PoseInfoTable && Dao_PoseChangeTable && inputCmds.Num() > 0)
	{
		FString poseStr = FString::FromInt(poseIdx);
		FPoseChangeTable* poseChange = Dao_PoseChangeTable->FindRow<FPoseChangeTable>(FName(*poseStr), "");
		if (poseChange)
		{
			const TArray<int32>& NextPoses = poseChange->NextPoses;

			for (int32 c = 0; c < inputCmds.Num(); ++c)
			{
				FName inputCmd = FName(*inputCmds[c]);
				for (int32 i = 0; i < NextPoses.Num(); ++i)
				{
					FString tmpPoseStr = FString::FromInt(NextPoses[i]);
					const FPoseInputTable* PoseInfo = PoseInfoTable->FindRow<FPoseInputTable>(FName(*tmpPoseStr), "");
					if (PoseInfo)
					{
						FName poseCmd = FName(*PoseInfo->PoseInputKey);
						if (poseCmd.Compare(inputCmd) == 0)
						{
							return NextPoses[i];
						}
					}
				}
			}
		}
	}
	return -1;
}

AAttackCharacter::SPRINT_DIRECTION AAttackCharacter::GetReadySprintDirection(const TArray<FString>& InputCmds)
{
	static FName Sprint_Fwd_Cmd = "U-U";
	static FName Sprint_Bwd_Cmd = "D-D";
	static FName Sprint_Left_Cmd = "L-L";
	static FName Sprint_Right_Cmd = "R-R";

	for (int32 i = 0; i < InputCmds.Num(); ++i)
	{
		const FString& cmd = InputCmds[i];
		if (cmd.Len() == 3)
		{
			if (Sprint_Fwd_Cmd == FName(*cmd))
				return SPRINT_DIRECTION::SPRINT_FWD;
			else if (Sprint_Bwd_Cmd == FName(*cmd))
				return SPRINT_DIRECTION::SPRINT_BWD;
			else if (Sprint_Left_Cmd == FName(*cmd))
				return SPRINT_DIRECTION::SPRINT_LEFT;
			else if (Sprint_Right_Cmd == FName(*cmd))
				return SPRINT_DIRECTION::SPRINT_RIGHT;
		}
	}
	return SPRINT_DIRECTION::SPRINT_IDLE;
}

int32 AAttackCharacter::GenerateAttackCommond(int32 OldAttackCmd)
{
	int32 NewAttackCmd;
	TArray<FString> AllCmdKeys;
	AllCmdKeys = InputBufferCP->GetInputCommonds();
	for (int32 i = 0; i < AllCmdKeys.Num(); ++i)
	{
		AllCmdKeys[i].Append("-A");
	}
	AllCmdKeys.Add("A");
	NewAttackCmd = GetNextPose(OldAttackCmd, AllCmdKeys);
	InputBufferCP->ConsumeInputKey();
	return NewAttackCmd;
}
