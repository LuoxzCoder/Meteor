// Copyright IceRiver. All Rights Reserved.

#include "InputCommandComponent.h"
#include "Meteor/Common/MeteorFuncLib.h"
#include "GameFramework/PlayerInput.h"
#include "Engine.h"

using namespace Meteor;

UInputCommandComponent::UInputCommandComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	KeyCount = (int)Meteor::INPUT_KEY::KEY_OTHER;

	InputHaltTime = 15;

	NonAttackCommandHaltTime = 10;

	StateNo = -1;

	LastStateNo = -1;

	bHasControl = true;

	CurrentStateType = STATE_TYPE::TYPE_STANDING;

	CurrentFrameKeyState.SetNum(KeyCount);

	CurrentFrameInputEvent.SetNum(KeyCount);

	bCanAttack = false;

	bBeginChain = false;

	IdleStateNextPoses = {165, 168, 171, 174};

	bGuardKeyDown = false;

	bJumpKeyDown = false;

	GuardPoseIndex = 18;
}

void UInputCommandComponent::BeginPlay()
{
	Super::BeginPlay();

	GetKeyMapping();
	InitActionList();
	RecreateNextPoses(-1);
	CreateStateRecord(-1);
}

void UInputCommandComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TickInputState();
	PoseprocessInputBuffer();

	bCanAttack = false;
	TickStateRecord();
}

void UInputCommandComponent::InitActionList()
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
				if (def.cmd.CreateInputCommand(*state->InputCommand, state->InputCommand))
				{
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
}

void UInputCommandComponent::GetKeyMapping()
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

void UInputCommandComponent::TickInputState()
{
	for (int i = 0; i < CurrentFrameKeyState.Num(); ++i)
	{
		CurrentFrameKeyState[i] = false;
	}

	for (int i = 0; i < CurrentFrameInputEvent.Num(); ++i)
	{
		CurrentFrameInputEvent[i] = Meteor::INPUT_EVENT::INPUT_Idle;
	}

	if (bHasControl)
	{
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
}

void UInputCommandComponent::PoseprocessInputBuffer()
{
	// 处理U D 同时按下的情况
	if (CurrentFrameKeyState[(int)Meteor::INPUT_KEY::KEY_UP] && CurrentFrameKeyState[(int)Meteor::INPUT_KEY::KEY_DOWN])
	{
		CurrentFrameKeyState[(int)Meteor::INPUT_KEY::KEY_UP] = false;
		CurrentFrameKeyState[(int)Meteor::INPUT_KEY::KEY_DOWN] = false;
		
		if (CurrentFrameInputEvent[(int)Meteor::INPUT_KEY::KEY_UP] == Meteor::INPUT_EVENT::INPUT_Pressed)
		{
			CurrentFrameKeyState[(int)Meteor::INPUT_KEY::KEY_UP] = true;
		}
		if (CurrentFrameInputEvent[(int)Meteor::INPUT_KEY::KEY_DOWN] == Meteor::INPUT_EVENT::INPUT_Pressed)
		{
			CurrentFrameKeyState[(int)Meteor::INPUT_KEY::KEY_DOWN] = true;
		}
		if (CurrentFrameInputEvent[(int)Meteor::INPUT_KEY::KEY_UP] == Meteor::INPUT_EVENT::INPUT_Pressed && 
			CurrentFrameInputEvent[(int)Meteor::INPUT_KEY::KEY_DOWN] == Meteor::INPUT_EVENT::INPUT_Pressed)
		{
			CurrentFrameKeyState[(int)Meteor::INPUT_KEY::KEY_UP] = false;
			CurrentFrameKeyState[(int)Meteor::INPUT_KEY::KEY_DOWN] = false;
		}
	}
	
	// 处理 L R 同时按下的情况
	if (CurrentFrameKeyState[(int)Meteor::INPUT_KEY::KEY_LEFT] && CurrentFrameKeyState[(int)Meteor::INPUT_KEY::KEY_RIGHT])
	{
		CurrentFrameKeyState[(int)Meteor::INPUT_KEY::KEY_LEFT] = false;
		CurrentFrameKeyState[(int)Meteor::INPUT_KEY::KEY_RIGHT] = false;

		if (CurrentFrameInputEvent[(int)Meteor::INPUT_KEY::KEY_LEFT] == Meteor::INPUT_EVENT::INPUT_Pressed)
		{
			CurrentFrameKeyState[(int)Meteor::INPUT_KEY::KEY_LEFT] = true;
		}
		if (CurrentFrameInputEvent[(int)Meteor::INPUT_KEY::KEY_RIGHT] == Meteor::INPUT_EVENT::INPUT_Pressed)
		{
			CurrentFrameKeyState[(int)Meteor::INPUT_KEY::KEY_RIGHT] = true;
		}
		if (CurrentFrameInputEvent[(int)Meteor::INPUT_KEY::KEY_LEFT] == Meteor::INPUT_EVENT::INPUT_Pressed && 
			CurrentFrameInputEvent[(int)Meteor::INPUT_KEY::KEY_RIGHT] == Meteor::INPUT_EVENT::INPUT_Pressed)
		{
			CurrentFrameKeyState[(int)Meteor::INPUT_KEY::KEY_LEFT] = false;
			CurrentFrameKeyState[(int)Meteor::INPUT_KEY::KEY_LEFT] = false;
		}
	}
}

void UInputCommandComponent::TickStateRecord()
{
	// 因为CurrentStateRecord中优先级最高的在最末尾，所以倒着来索引
	for (int i = CurrentStateRecord.Num() - 1; i >= 0;--i)
	{
		ActionStateRecord& Record = CurrentStateRecord[i];

		if (Record.Timer == 0)
		{
			Record.CmdState = 0;
			Record.DelayTimer = NonAttackCommandHaltTime;
		}
		if (Record.CmdState < Record.Action->cmd.Command.Num())
		{
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
		}
		if (Record.CmdState == Record.Action->cmd.Command.Num())
		{
			if (Record.Action->cmd.bIsDelayCommand)
			{
				Record.DelayTimer -= 1;
			}
			if (CanAttack(Record))
			{
				Record.Timer = 0;
				LastStateNo = StateNo;
				StateNo = Record.Action->poseNo;

				bCanAttack = true;
				GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Blue, FString::Printf(TEXT("%s-%s-%d"), *Record.Action->name.ToString(), *Record.Action->cmd.Name.ToString(), Record.Action->poseNo));
				
				RecreateNextPoses(StateNo);
				break;
			}
			else
			{
				Record.Timer -= 1;
			}
		}
	}
}

void UInputCommandComponent::CreateStateRecord(int currStateId)
{
	CurrentStateRecord.Empty();
	for (int i = 0; i < ActionList.Num(); ++i)
	{
		const ActionStateDef& Action = ActionList[i];
		{
			ActionStateRecord record;
			record.Action = &ActionList[i];
			record.CmdState = 0;
			record.Timer = 0;
			record.DelayTimer = NonAttackCommandHaltTime;
			CurrentStateRecord.Add(record);
		}
	}
	// 需要按照优先级排序，最高优先级的Record首先进行, 具体的优先级算法可以更新复杂
	CurrentStateRecord.Sort();
}

void UInputCommandComponent::RecreateNextPoses(int currStateId)
{
	CurrentNextPoses.Empty();
	FPoseChangeTable* PoseChangeInfo = GetPoseChangeInfo(currStateId);
	if (PoseChangeInfo == nullptr)
	{
		PoseChangeInfo = GetPoseChangeInfo(-1);
	}
	if (PoseChangeInfo)
	{
		CurrentNextPoses = PoseChangeInfo->NextPoses;

		if (currStateId == -1)
		{
			for (int i = 0; i < IdleStateNextPoses.Num(); ++i)
			{
				CurrentNextPoses.AddUnique(IdleStateNextPoses[i]);
			}
		}
	}
}

FPoseStateInfo* UInputCommandComponent::GetPoseStateInfo(int stateNo)
{
	if (Dao_PoseStateInfo)
	{
		FString tmp = FString::FromInt(stateNo);
		FPoseStateInfo* PoseState = Dao_PoseStateInfo->FindRow<FPoseStateInfo>(*tmp, Dao_PoseStateInfo->GetName());
		return PoseState;
	}
	return nullptr;
}

FPoseChangeTable* UInputCommandComponent::GetPoseChangeInfo(int stateNo)
{
	if (Dao_PoseChangeInfo)
	{
		FString tmp = FString::FromInt(stateNo);
		FPoseChangeTable* NextPoses = Dao_PoseChangeInfo->FindRow<FPoseChangeTable>(*tmp, Dao_PoseChangeInfo->GetName());
		if (NextPoses && NextPoses->PoseID == stateNo)
		{
			return NextPoses;
		}
	}
	return nullptr;
}

bool UInputCommandComponent::CanAttack(ActionStateRecord& Record)
{
	if (Record.Action->type == CurrentStateType &&
		CurrentStateType != STATE_TYPE::TYPE_LYE_DOWN && 
		bBeginChain && 
		CurrentNextPoses.Find(Record.Action->poseNo) != INDEX_NONE)
	{
		if (Record.Action->cmd.bIsDelayCommand)
		{
			if (Record.DelayTimer == 0)
			{
				return true;
			}
			else
			{
				return false;
			}
		}
		else
		{
			return true;
		}
	}
	return false;
}

void UInputCommandComponent::OnGuard()
{
	bGuardKeyDown = true;
}

void UInputCommandComponent::StopGuard()
{
	bGuardKeyDown = false;
}
