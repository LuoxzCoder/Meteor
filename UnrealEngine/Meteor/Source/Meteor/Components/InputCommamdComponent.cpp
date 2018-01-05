// Copyright IceRiver. All Rights Reserved.

#include "InputCommamdComponent.h"
#include "Engine.h"
#include "Common/MeteorFuncLib.h"
#include "AttackCharacter.h"

UInputCommamdComponent::UInputCommamdComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	KeyCount = (int)Meteor::INPUT_KEY::KEY_OTHER;

	InputHaltTime = 15;

	StateNo = -1;

	LastStateNo = -1;

	bHasControl = true;

	CurrentStateType = STATE_TYPE::TYPE_STANDING;

	CurrentFrameKeyState.SetNum(KeyCount);

	CurrentFrameInputEvent.SetNum(KeyCount);
}

void UInputCommamdComponent::BeginPlay()
{
	Super::BeginPlay();

	UMeteorFuncLib::GetMeteorSingleton()->StreamMgr.LoadSynchronous(Dao_PoseStateInfo);

	GetKeyMapping();
	InitActionList();
	RecreateStateRecord(-1);
}

void UInputCommamdComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TickInputState();

	TickStateRecord();
}

void UInputCommamdComponent::InitActionList()
{
	if (Dao_PoseStateInfo)
	{
		TArray<FPoseStateInfo*> StateInfos;
		Dao_PoseStateInfo->GetAllRows("", StateInfos);

		for (int i = 0; i < StateInfos.Num(); ++i)
		{
			FPoseStateInfo* state = StateInfos[i];
			if (state)
			{
				Meteor::ActionStateDef def;
				def.name = *state->PoseName;
				def.cmd.CreateInputCommand(*state->InputCommand, state->InputCommand);
				if (state->bInAir)
					def.type = STATE_TYPE::TYPE_AIR;
				else
					def.type = STATE_TYPE::TYPE_STANDING;
				def.moveType = MOVE_TYPE::MOVE_ATTACK;
				def.attack = 20;
				def.juggling = 15;
				def.poseNo = state->PoseID;
				def.priority = state->Priority;

				ActionList.Add(def);
			}
		}
	}
}

void UInputCommamdComponent::GetKeyMapping()
{
	const APlayerController* PlayerControler = GetWorld()->GetFirstPlayerController();
	if (PlayerControler)
	{
		for (int i = 0; i < KeyCount; ++i)
		{
			FName mappingName = GetKeyMappingName((Meteor::INPUT_KEY)i);
			const TArray<FInputActionKeyMapping>& InputActionMappings = PlayerControler->PlayerInput->GetKeysForAction(mappingName);
			if (InputActionMappings.Num() > 0)
			{
				const FInputActionKeyMapping& Mapping = InputActionMappings[0];
				KeyMapping.Add((Meteor::INPUT_KEY)i, Mapping.Key);
			}
		}
	}
}

void UInputCommamdComponent::TickInputState()
{
	for (int i = 0; i < CurrentFrameKeyState.Num(); ++i)
	{
		CurrentFrameKeyState[i] = false;
	}

	const APlayerController* PlayerControler = GetWorld()->GetFirstPlayerController();
	for (int i = 0; i < KeyCount; ++i)
	{
		FKey* key = KeyMapping.Find((Meteor::INPUT_KEY)i);
		if (key)
		{
			if (PlayerControler->IsInputKeyDown(*key))
			{
				CurrentFrameKeyState[i] = true;
			}
			if (PlayerControler->WasInputKeyJustPressed(*key))
			{
				CurrentFrameInputEvent[i] = Meteor::INPUT_EVENT::INPUT_Pressed;
			}
			else if (PlayerControler->WasInputKeyJustReleased(*key))
			{
				CurrentFrameInputEvent[i] = Meteor::INPUT_EVENT::INPUT_Released;
			}
			else
			{
				CurrentFrameInputEvent[i] = Meteor::INPUT_EVENT::INPUT_Idle;
			}
		}
	}
}

void UInputCommamdComponent::TickStateRecord()
{
	// 因为CurrentStateRecord中优先级最高的在最末尾，所以倒着来索引
	for (int i = CurrentStateRecord.Num() - 1; i >= 0;--i)
	{
		ActionStateRecord& Record = CurrentStateRecord[i];

		if (Record.Timer == 0)
		{
			Record.CmdState = 0;
		}

		INPUT_KEY key = Record.Action->cmd.Command[Record.CmdState].Key;
		if (Record.CmdState == 0)
		{
			if (CurrentFrameKeyState[(int)key] || 
				CurrentFrameInputEvent[(int)key] == Meteor::INPUT_EVENT::INPUT_Pressed)
			{
				Record.CmdState++;
				Record.Timer = InputHaltTime;
			}
		}
		else
		{
			if (CurrentFrameInputEvent[(int)key] == Meteor::INPUT_EVENT::INPUT_Pressed)
			{
				Record.CmdState++;
				Record.Timer = InputHaltTime;
			}
			else
			{
				Record.Timer -= 1;
			}
		}
		if (Record.CmdState == Record.Action->cmd.Command.Num())
		{
			if (CanAttack(Record.Action))
			{
				Record.Timer = 0;
				LastStateNo = StateNo;
				StateNo = Record.Action->poseNo;

				if (Record.Action->priority > 0)
				{
					GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Blue, FString::Printf(TEXT("Attack: %s-%s-%d"), *Record.Action->cmd.Name.ToString(), *Record.Action->name.ToString(), Record.Action->poseNo));
				}
				break;
			}
		}
	}
}

void UInputCommamdComponent::RecreateStateRecord(int currStateId)
{
	// for test
	for (int i = 0; i < ActionList.Num(); ++i)
	{
		ActionStateRecord record;
		record.Action = &ActionList[i];
		record.CmdState = 0;
		record.Timer = 0;
		CurrentStateRecord.Add(record);
	}

	// 需要按照优先级排序，最高优先级的Record首先进行
	// 具体的优先级算法可以更新复杂
	CurrentStateRecord.Sort();
}

FPoseStateInfo* UInputCommamdComponent::GetPoseStateInfo(int stateNo)
{
	if (Dao_PoseStateInfo == nullptr)
	{
		UMeteorFuncLib::GetMeteorSingleton()->StreamMgr.LoadSynchronous(Dao_PoseStateInfo);
	}
	FString tmp = FString::FromInt(stateNo);
	FPoseStateInfo* PoseState = Dao_PoseStateInfo->FindRow<FPoseStateInfo>(*tmp, "");
	return PoseState;
}

bool UInputCommamdComponent::CanAttack(ActionStateDef* Action)
{
	if (Action->type == CurrentStateType && CurrentStateType != STATE_TYPE::TYPE_LYE_DOWN)
	{
		return true;
	}
	else
	{
		return false;
	}
}

