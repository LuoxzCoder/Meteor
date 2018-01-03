// Copyright IceRiver. All Rights Reserved.

#include "InputCommamdComponent.h"
#include "Engine.h"
#include "Common/MeteorSingletonLibrary.h"

UInputCommamdComponent::UInputCommamdComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	KeyCount = (int)Meteor::INPUT_KEY::KEY_OTHER;

	KeyWaitTime = 15;

	CurrentFrameKeyState.SetNum(KeyCount);

	CurrentFrameInputEvent.SetNum(KeyCount);
}

void UInputCommamdComponent::BeginPlay()
{
	Super::BeginPlay();

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
	{
		Meteor::ActionStateDef def;
		def.name = "shi_yue_zhan";
		def.cmd.CreateInputCommand("A", "A");
		def.type = STATE_TYPE::TYPE_STANDING;
		def.moveType = MOVE_TYPE::MOVE_ATTACK;
		def.attack = 20;
		def.juggling = 15;
		def.poseNo = 295;
		def.priority = 10;
		ActionList.Add(def);
	}
	{
		Meteor::ActionStateDef def;
		def.name = "xuan_feng_zhan";
		def.cmd.CreateInputCommand("L-R-A", "L,R,A");
		def.type = STATE_TYPE::TYPE_STANDING;
		def.moveType = MOVE_TYPE::MOVE_ATTACK;
		def.attack = 20;
		def.juggling = 15;
		def.poseNo = 374;
		def.priority = 30;
		ActionList.Add(def);	
	}
	{
		Meteor::ActionStateDef def;
		def.name = "sui_an_lie";
		def.cmd.CreateInputCommand("L-A", "L,A");
		def.type = STATE_TYPE::TYPE_STANDING;
		def.moveType = MOVE_TYPE::MOVE_ATTACK;
		def.attack = 20;
		def.juggling = 15;
		def.poseNo = 374;
		def.priority = 20;
		ActionList.Add(def);
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
				Record.Timer = KeyWaitTime;
			}
		}
		else
		{
			if (CurrentFrameInputEvent[(int)key] == Meteor::INPUT_EVENT::INPUT_Pressed)
			{
				Record.CmdState++;
				Record.Timer = KeyWaitTime;
			}
			else
			{
				Record.Timer -= 1;
			}
		}

		if (Record.CmdState == Record.Action->cmd.Command.Num())
		{
			Record.Timer = 0;
			if (Record.Action->name != "shi_yue_zhan")
			{
				GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Blue, FString::Printf(TEXT("Attack: %s-%s-%d"), *Record.Action->cmd.Name.ToString(), *Record.Action->name.ToString(), Record.Action->poseNo));
			}
			// 如果满足条件则，直接跳出
			break;
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

