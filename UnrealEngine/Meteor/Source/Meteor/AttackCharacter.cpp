
#include "AttackCharacter.h"
#include "Meteor.h"
#include "Animation/AnimInstance.h"
#include "FPoseInputTable.h"
#include "MyAnimMetaData.h"

AAttackCharacter::AAttackCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	CurrentPoseIndex = 295;

	bAcceptInput = false;

	NextPoseTime = 0.2f;

	AttackState = ATTACK_STATE::ATK_IDLE;

	bAttackKeyDown = false;

	NextPoseTimer = 0.0f;

	NextPoseMtg = nullptr;
}


void AAttackCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (PoseInfoTable)
	{
		FPoseInputTable* PoseInfo = nullptr;

		PoseInfo = PoseInfoTable->FindRow<FPoseInputTable>(TEXT("295"), "");
		StreamMgr.LoadSynchronous(PoseInfo->PoseMontage);

		PoseInfo = PoseInfoTable->FindRow<FPoseInputTable>(TEXT("296"), "");
		StreamMgr.LoadSynchronous(PoseInfo->PoseMontage);

		PoseInfo = PoseInfoTable->FindRow<FPoseInputTable>(TEXT("297"), "");
		StreamMgr.LoadSynchronous(PoseInfo->PoseMontage);
	}

	// 进行循环测试
	TmpPoseTranslationTable.Add(295, 296);
	TmpPoseTranslationTable.Add(296, 297);
}

void AAttackCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance == nullptr)
		return;

	switch (AttackState)
	{
	case AAttackCharacter::ATK_IDLE:
	{
		if (bAttackKeyDown)
		{
			ConsumeInputKey();

			// reset
			CurrentPoseIndex = 295;
			bAcceptInput = false;
			NextPoseTime = 0.2f;
			NextPoseTimer = 0.0f;
			NextPoseMtg = nullptr;

			UAnimMontage* poseMtg = GetPoseMontage(CurrentPoseIndex);
			if (poseMtg)
			{
				GetAnimMetaData(poseMtg);
				AnimInstance->Montage_Play(poseMtg);
				AttackState = ATK_PLAYING;
			}
		}
		break;
	}
	case AAttackCharacter::ATK_PLAYING:
	{
		UAnimMontage* ActiveMontage = AnimInstance->GetCurrentActiveMontage();
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
			if (sectionName.IsEqual(NextPoseOut) && bAttackKeyDown)
			{
				ConsumeInputKey();

				int32* nextPoseIndex = TmpPoseTranslationTable.Find(CurrentPoseIndex);
				if (nextPoseIndex)
				{
					CurrentPoseIndex = *nextPoseIndex;
					UAnimMontage* poseMtg = GetPoseMontage(CurrentPoseIndex);
					if (poseMtg)
					{
						GetAnimMetaData(poseMtg);

						// 使用代理会出错，可能是因为指针造成的
						/*tmpBlend = poseMtg->BlendIn;
						poseMtg->BlendIn.SetBlendTime(NextPoseTime);
						AnimInstance->Montage_Play(poseMtg, 0.0f); // 这个用法也是错误的
						AnimInstance->Montage_JumpToSection(NextPoseIn, poseMtg);

						auto lamda = [&]()->void {
							GWorld->GetTimerManager().ClearTimer(NextPoseTransitionTimer);
							AttackState = ATK_PLAYING;
						};

						auto dlg = FTimerDelegate::CreateLambda(lamda);
						GWorld->GetTimerManager().SetTimer(NextPoseTransitionTimer, dlg, NextPoseTime, false);

						AttackState = ATK_NEXTPOSE;*/

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

	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	PlayerInputComponent->BindAction("Attack", IE_Pressed, this, &AAttackCharacter::OnAttackV2);
	PlayerInputComponent->BindAction("Attack", IE_Released, this, &AAttackCharacter::StopAttack);
}


void AAttackCharacter::OnAttackV2()
{
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance)
	{
		UAnimMontage* activeMontage = AnimInstance->GetCurrentActiveMontage();
		if (activeMontage && bAcceptInput)
		{
			bAttackKeyDown = true;
		}
		else if (activeMontage == nullptr)
		{
			bAttackKeyDown = true;
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
			if (poseMtg)
			{
				return poseMtg;
				UE_LOG(LogTemp, Warning, TEXT("Get %s Succeed!"), *(poseMtg->GetName()));
			}
		}
	}
	return nullptr;
}

void AAttackCharacter::GetAnimMetaData(UAnimMontage* montage)
{
	if (montage)
	{
		SectionRatioCache.Empty(4);
		SectionRatioHasSet.Empty(4);
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

bool AAttackCharacter::ConsumeInputKey()
{
	bool lastInput = bAttackKeyDown;
	bAttackKeyDown = false;
	return lastInput;
}
