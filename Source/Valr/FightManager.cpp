// Fill out your copyright notice in the Description page of Project Settings.


#include "FightManager.h"
#include "DrawDebugHelpers.h"
#include <cmath>
#include <EngineGlobals.h>
#include <Runtime/Engine/Classes/Engine/Engine.h>
#include "GameFramework/PlayerController.h"

#include "Kismet/KismetMathLibrary.h"

// Sets default values
AFightManager::AFightManager()
{
 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AFightManager::BeginPlay()
{
	Super::BeginPlay();
	if (!Player1 || !Player2 || !Camera)
	{
		UE_LOG(LogTemp, Warning, TEXT("FighterManager: The pointers aren't set up in the Blueprint!"));
		return;
	}
	GetWorld()->GetFirstPlayerController()->SetViewTarget(Camera);
	//AddTickPrerequisiteActor(Player1);
	//AddTickPrerequisiteActor(Player2);
	roundState = ROUND_STATE::ROUND_RESTARTING;
}

void AFightManager::processPlayer(AFighterPawn* &P1, AFighterPawn* &P2, FVector toP1, FVector toP2, bool &bOpponentIsHit)
{
	float PlayerDistance = (P1->GetActorLocation() - P2->GetActorLocation()).Size();
	if (P1->State == STATE::MOVING || P1->State == STATE::STEPPING)
	{
		P1->SetActorRotation(FMath::Lerp(P1->GetActorRotation(), toP2.Rotation(), ((float)Player1->turnSpeed / 100.f)));
		if (PlayerDistance < 50)
			if (P1->State == STATE::STEPPING)
				P1->SetActorLocation(P1->GetActorLocation() + toP1 * ((float)P1->steppingSpeed * P1->sideStepSpeed / 100.f));
			else P1->SetActorLocation(P1->GetActorLocation() + toP1 * P1->MovementSpeed);
	}
	else if (P1->State == STATE::ATTACKING)
	{
		if (P1->currentFrameOfAttack >= P1->Attacks[static_cast<uint8>(P1->attackType)].Parts[P1->currentPartsIndex].PSum)
		{
			if (P1->currentPartsIndex < P1->Attacks[static_cast<uint8>(P1->attackType)].Parts.Num() - 1)
				P1->currentPartsIndex++;
			else
			{
				P1->State = STATE::IDLE;
				P1->InputID = INPUT::IDLE;
				P1->currentFrameOfAttack = 0;
				P1->currentPartsIndex = 0;
				P1->attackType = ATTACK_TYPE::NONE;
				bOpponentIsHit = false;
				return;
			}
		}
		if (Angle(P1->GetActorRightVector(), toP2) > P1->Attacks[static_cast<uint8>(P1->attackType)].Parts[P1->currentPartsIndex].minAngle &&
			Angle(P1->GetActorRightVector(), toP2) < P1->Attacks[static_cast<uint8>(P1->attackType)].Parts[P1->currentPartsIndex].maxAngle &&
			(P2->GetActorLocation() - P1->GetActorLocation()).Size() > P1->Attacks[static_cast<uint8>(P1->attackType)].Parts[P1->currentPartsIndex].minDist &&
			(P2->GetActorLocation() - P1->GetActorLocation()).Size() < P1->Attacks[static_cast<uint8>(P1->attackType)].Parts[P1->currentPartsIndex].maxDist &&
			Angle(P1->GetActorForwardVector(), toP2) < 90)
		{
			if (P2->State == STATE::BLOCKING && Angle(P2->GetActorForwardVector(), toP1) < P2->BlockData.Angle &&
				(P1->GetActorLocation() - P2->GetActorLocation()).Size() > P2->BlockData.minDist &&
				(P1->GetActorLocation() - P2->GetActorLocation()).Size() < P2->BlockData.maxDist && !P1->Attacks[static_cast<uint8>(P1->attackType)].bUnblockableAttack)
			{
				uint8 specialCharge = P1->Attacks[static_cast<uint8>(P1->attackType)].Damage* P2->specialChargeMultiplier;
				if (P2->specialMeter <= 255-specialCharge) P2->specialMeter += specialCharge;
				P1->State = STATE::STUNNED;
				P1->currentFrameOfAttack = P2->BlockData.blockStunRate;
				P1->stunPush = P2->BlockData.blockPushPower;
				UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), P1->hitParticle, P1->GetActorLocation() + FVector(0, 0, 120) + toP2 * 2, toP2.Rotation());
			}
			else
			{
				if (!bOpponentIsHit)
				{
					if (P2->Health >= P1->Attacks[static_cast<uint8>(P1->attackType)].Damage) P2->Health -= P1->Attacks[static_cast<uint8>(P1->attackType)].Damage;
					else P2->Health = 0;
					P2->State = STATE::STUNNED;
					P2->currentFrameOfAttack = P1->Attacks[static_cast<uint8>(P1->attackType)].StunRate;
					P2->stunPush = P1->Attacks[static_cast<uint8>(P1->attackType)].StunPushPower;
					UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), P2->hitParticle, P2->GetActorLocation() + FVector(0, 0, 120) + toP1 * 2, toP1.Rotation());
					bOpponentIsHit = true;
					uint8 specialCharge = P1->Attacks[static_cast<uint8>(P1->attackType)].Damage* P2->specialChargeMultiplier;
					if (P2->specialMeter <= 255-specialCharge) P2->specialMeter += specialCharge;
				}
				P1->currentFrameOfAttack++;
				if (PlayerDistance > 50) 
					P1->SetActorLocation(P1->GetActorLocation() + (P1->GetActorForwardVector() * 
					((P1->Attacks[static_cast<uint8>(P1->attackType)].AttackTotalFrameCount-P1->currentFrameOfAttack) * P1->Attacks[static_cast<uint8>(P1->attackType)].attackPush)));
			}
		}
		else
		{
			P1->currentFrameOfAttack++;
			if (PlayerDistance > 50)
				P1->SetActorLocation(P1->GetActorLocation() + (P1->GetActorForwardVector() *
				((P1->Attacks[static_cast<uint8>(P1->attackType)].AttackTotalFrameCount - P1->currentFrameOfAttack) * P1->Attacks[static_cast<uint8>(P1->attackType)].attackPush)));
		}
		GEngine->AddOnScreenDebugMessage(-1, -1.f, FColor::Yellow, FString::Printf(TEXT("Attack Min Angle: %f"), P1->Attacks[static_cast<uint8>(P1->attackType)].Parts[P1->currentPartsIndex].minAngle));
		GEngine->AddOnScreenDebugMessage(-1, -1.f, FColor::Yellow, FString::Printf(TEXT("Attack Max Angle: %f"), P1->Attacks[static_cast<uint8>(P1->attackType)].Parts[P1->currentPartsIndex].maxAngle));
		GEngine->AddOnScreenDebugMessage(-1, -1.f, FColor::Yellow, FString::Printf(TEXT("Attack Min Distance: %f"), P1->Attacks[static_cast<uint8>(P1->attackType)].Parts[P1->currentPartsIndex].minDist));
		GEngine->AddOnScreenDebugMessage(-1, -1.f, FColor::Yellow, FString::Printf(TEXT("Attack Max Distance: %f"), P1->Attacks[static_cast<uint8>(P1->attackType)].Parts[P1->currentPartsIndex].maxDist));
	}
	else if (P1->State == STATE::STUNNED)
	{
		if (P1->currentFrameOfAttack > 0)
		{
			P1->currentFrameOfAttack--;
			P1->SetActorLocation(P1->GetActorLocation() + (toP1 * (P1->currentFrameOfAttack * ((float)P1->stunPush / 100.f))));
		}
		else
		{
			P1->State = STATE::IDLE;
			P1->InputID = INPUT::IDLE;
			P1->currentPartsIndex = 0;
			P1->stunPush = 1;
			P1->attackType = ATTACK_TYPE::NONE;
		}
	}
}

void AFightManager::testRoundStatus()
{
	if (roundTimer == 0)
	{
		if (Player1->Health > Player2->Health)
		{
			Player1Score++;
			roundCount++;
			Player2->State = STATE::KNOCKED_DOWN;
			Player2->Reset();
			timeToStart = 300;
		}
		else
		{
			Player2Score++;
			roundCount++;
			Player1->State = STATE::KNOCKED_DOWN;
			Player1->Reset();
			timeToStart = 300;
		}
		bPlayer1IsHit = false;
		bPlayer2IsHit = false;
		return;
	}

	if (Player1->Health == 0)
	{
		Player2Score++;
		roundCount++;
		Player1->State = STATE::KNOCKED_DOWN;
		Player1->Reset();
		timeToStart = 300;
		bPlayer1IsHit = false;
		bPlayer2IsHit = false;
	}
	else if (Player2->Health == 0)
	{
		Player1Score++;
		roundCount++;
		Player2->State = STATE::KNOCKED_DOWN;
		Player2->Reset();
		timeToStart = 300;
		bPlayer1IsHit = false;
		bPlayer2IsHit = false;
	}
}

void AFightManager::calculateTimers(FVector toP1, FVector toP2)
{
	if (timeToStart == 0)
	{
		roundTimer -= 1;
	}
	else if (timeToStart > 180)
	{
		if (Player1->State == STATE::KNOCKED_DOWN)
		{
			roundState = ROUND_STATE::ROUND_OVER_PLAYER2_WINS;
			DisableInput(GetWorld()->GetFirstPlayerController());
			Player2->State = STATE::IDLE;
			Player2->Reset();
		}
		else if (Player2->State == STATE::KNOCKED_DOWN)
		{
			roundState = ROUND_STATE::ROUND_OVER_PLAYER1_WINS;
			DisableInput(GetWorld()->GetFirstPlayerController());
			Player1->State = STATE::IDLE;
			Player1->Reset();
		}
		timeToStart--;
		return;
	}
	else if (timeToStart >= (60))
	{
		if (Player1->State == STATE::KNOCKED_DOWN)
		{
			Player1->State = STATE::GETTING_UP;
			Player1->SetActorRotation(toP2.Rotation());
			Player2->SetActorRotation(toP1.Rotation());
			FVector MiddleVector = (Player1->GetActorLocation() + Player2->GetActorLocation()) / 2;
			Player1->SetActorLocation(FVector(MiddleVector.X + toP1.X * 175, MiddleVector.Y + toP1.Y * 175, MiddleVector.Z));
			Player2->SetActorLocation(FVector(MiddleVector.X + toP2.X * 175, MiddleVector.Y + toP2.Y * 175, MiddleVector.Z));
		}
		else if (Player2->State == STATE::KNOCKED_DOWN)
		{
			Player2->State = STATE::GETTING_UP;
			Player1->SetActorRotation(toP2.Rotation());
			Player2->SetActorRotation(toP1.Rotation());
			FVector MiddleVector = (Player1->GetActorLocation() + Player2->GetActorLocation()) / 2;
			Player1->SetActorLocation(FVector(MiddleVector.X + toP1.X * 175, MiddleVector.Y + toP1.Y * 175, MiddleVector.Z));
			Player2->SetActorLocation(FVector(MiddleVector.X + toP2.X * 175, MiddleVector.Y + toP2.Y * 175, MiddleVector.Z));
		}
		Camera->SetActorLocation(FVector((Player1->GetActorLocation().X + Player2->GetActorLocation().X) * 0.5f, (Player1->GetActorLocation().Y + Player2->GetActorLocation().Y) * 0.5f, Player1->GetActorLocation().Z + Camera->Height));
		Camera->SpringArm->TargetArmLength = FMath::Lerp(Camera->SpringArm->TargetArmLength, 350.f, 0.05f);
		FVector MiddleVector = Player2->GetActorLocation() - Player1->GetActorLocation();
		FVector PerpendicularVector = { MiddleVector.Y,-MiddleVector.X,MiddleVector.Z };
		PerpendicularVector.Normalize();
		//Camera->SetActorRotation(FRotator(PerpendicularVector.Rotation().Roll, PerpendicularVector.Rotation().Yaw, PerpendicularVector.Rotation().Pitch));
		DisableInput(GetWorld()->GetFirstPlayerController());
		roundState = ROUND_STATE::ROUND_RESTARTING;
		timeToStart--;
		return;
	}
	else if (timeToStart <= (60) && timeToStart > 1)
	{
		Camera->SpringArm->TargetArmLength = FMath::Lerp(Camera->SpringArm->TargetArmLength, 350.f, 0.05f);
		FVector MiddleVector = Player2->GetActorLocation() - Player1->GetActorLocation();
		FVector PerpendicularVector = { MiddleVector.Y,-MiddleVector.X,MiddleVector.Z };
		PerpendicularVector.Normalize();
		//Camera->SetActorRotation(FRotator(PerpendicularVector.Rotation().Roll, PerpendicularVector.Rotation().Yaw, PerpendicularVector.Rotation().Pitch));
		roundState = ROUND_STATE::ROUND_ABOUT_TO_START;
		timeToStart--;
		return;
	}
	else if (timeToStart == 1)
	{
		roundTimer = 3600;
		if (Player1->State == STATE::GETTING_UP)
		{
			Player1->State = STATE::IDLE;
		}
		if (Player2->State == STATE::GETTING_UP)
		{
			Player2->State = STATE::IDLE;
		}
		EnableInput(GetWorld()->GetFirstPlayerController());
		timeToStart = 0;
		roundState = ROUND_STATE::ROUND_ONGOING;
	}
}

float AFightManager::Angle(FVector a, FVector b)
{
	return FMath::RadiansToDegrees(std::acos(FVector::DotProduct(a, b)));
}

// Called every frame
void AFightManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FVector toPlayer1 = Player1->GetActorLocation() - Player2->GetActorLocation();
	FVector toPlayer2 = Player2->GetActorLocation() - Player1->GetActorLocation();
	toPlayer1.Normalize();
	toPlayer2.Normalize();

	GEngine->AddOnScreenDebugMessage(-1, -1.f, FColor::Orange, FString::Printf(TEXT("Player 2 Angle: %f  -  Player 2 to Player 1 Distance: %f"), Angle(Player2->GetActorRightVector(), toPlayer1), (Player1->GetActorLocation() - Player2->GetActorLocation()).Size()));
	GEngine->AddOnScreenDebugMessage(-1, -1.f, FColor::Blue, FString::Printf(TEXT("Player 2 State: %i"), Player2->State));
	GEngine->AddOnScreenDebugMessage(-1, -1.f, FColor::Blue, FString::Printf(TEXT("Player 2 InputID: %i"), Player2->InputID));
	GEngine->AddOnScreenDebugMessage(-1, -1.f, FColor::Orange, FString::Printf(TEXT("Player 2 CurrentFrameOfAttack: %i"), Player2->currentFrameOfAttack));
	GEngine->AddOnScreenDebugMessage(-1, -1.f, FColor::Orange, FString::Printf(TEXT("Player 2 CurrentPartsIndex: %i"), Player2->currentPartsIndex));
	GEngine->AddOnScreenDebugMessage(-1, -1.f, FColor::Blue, FString::Printf(TEXT("Player 2 AttackType: %i"), Player2->attackType));

	GEngine->AddOnScreenDebugMessage(-1, -1.f, FColor::White, FString::Printf(TEXT("FrameTime: %f----------------------FrameRate: %f"), DeltaTime, 1.f / DeltaTime));

	GEngine->AddOnScreenDebugMessage(-1, -1.f, FColor::Green, FString::Printf(TEXT("Player 1 Angle: %f  -  Player 1 to Player 2 Distance: %f"), Angle(Player1->GetActorRightVector(), toPlayer2), (Player2->GetActorLocation() - Player1->GetActorLocation()).Size()));
	GEngine->AddOnScreenDebugMessage(-1, -1.f, FColor::Red, FString::Printf(TEXT("Player 1 State: %i"), Player1->State));
	GEngine->AddOnScreenDebugMessage(-1, -1.f, FColor::Red, FString::Printf(TEXT("Player 1 InputID: %i"), Player1->InputID));
	GEngine->AddOnScreenDebugMessage(-1, -1.f, FColor::Green, FString::Printf(TEXT("Player 1 CurrentFrameOfAttack: %i"), Player1->currentFrameOfAttack));
	GEngine->AddOnScreenDebugMessage(-1, -1.f, FColor::Green, FString::Printf(TEXT("Player 1 CurrentPartsIndex: %i"), Player1->currentPartsIndex));
	GEngine->AddOnScreenDebugMessage(-1, -1.f, FColor::Red, FString::Printf(TEXT("Player 1 AttackType: %i"), Player1->attackType));

	calculateTimers(toPlayer1,toPlayer2);

	Camera->SetActorLocation(FVector((Player1->GetActorLocation().X + Player2->GetActorLocation().X) * 0.5f, (Player1->GetActorLocation().Y + Player2->GetActorLocation().Y) * 0.5f, Player1->GetActorLocation().Z + Camera->Height));
	//Camera->SpringArm->TargetArmLength = FVector::Distance(Player1->GetActorLocation(), Player2->GetActorLocation());
	if (Camera->SpringArm->TargetArmLength < Camera->closestDistance) Camera->SpringArm->TargetArmLength = Camera->closestDistance;
	
	/*if (Player2->State != STATE::STUNNED)
	{
		if (t == 0)
		{
			n = FMath::RandRange(0, 9);
			t = 100;
		}
		else
		{
			t--;
			switch (n)
			{
				case 0: Player2->AxisW(1); Player2->AxisA(0); Player2->AxisS(0); Player2->AxisD(0); break;
				case 1: Player2->AxisW(0); Player2->AxisA(1); Player2->AxisS(0); Player2->AxisD(0); break;
				case 2: Player2->AxisW(0); Player2->AxisA(0); Player2->AxisS(1); Player2->AxisD(0); break;
				case 3: Player2->AxisW(0); Player2->AxisA(0); Player2->AxisS(0); Player2->AxisD(1); break;
				case 4: Player2->PressedLight(); break;
				case 5: Player2->PressedMedium(); break;
				case 6: Player2->PressedHeavy(); break;
				case 7: Player2->PressedSpecial(); break;
				case 8: Player2->PressedBlock(); break;
				case 9: Player2->ReleasedBlock(); break;
			}
		}
	}*/

	FVector MiddleVector = Player2->GetActorLocation() - Player1->GetActorLocation();
	FVector PerpendicularVector = { MiddleVector.Y,-MiddleVector.X,MiddleVector.Z };
	PerpendicularVector.Normalize();
	Camera->SetActorRotation(FRotator(PerpendicularVector.Rotation().Roll, PerpendicularVector.Rotation().Yaw, PerpendicularVector.Rotation().Pitch));

	processPlayer(Player1, Player2, toPlayer1, toPlayer2, bPlayer2IsHit);
	processPlayer(Player2, Player1, toPlayer2, toPlayer1, bPlayer1IsHit);
	testRoundStatus();

	/*DrawDebugLine
	(
		GetWorld(),
		Player1->GetActorLocation(),
		Player2->GetActorLocation(),
		FColor::Red,
		false,
		1,
		1,
		1);

	DrawDebugLine
	(
		GetWorld(),
		Player1->GetActorLocation(),
		Player1->GetActorLocation() + (Player1->GetActorRightVector() * 100),
		FColor::Red,
		false,
		1,
		1,
		1);*/
}

// Called to bind functionality to input
void AFightManager::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis("Player1_W", Player1, &AFighterPawn::AxisW);
	PlayerInputComponent->BindAxis("Player1_A", Player1, &AFighterPawn::AxisA);
	PlayerInputComponent->BindAxis("Player1_S", Player1, &AFighterPawn::AxisS);
	PlayerInputComponent->BindAxis("Player1_D", Player1, &AFighterPawn::AxisD);
	PlayerInputComponent->BindAction("Player1_Light", IE_Pressed, Player1, &AFighterPawn::PressedLight);
	PlayerInputComponent->BindAction("Player1_Medium", IE_Pressed, Player1, &AFighterPawn::PressedMedium);
	PlayerInputComponent->BindAction("Player1_Heavy", IE_Pressed, Player1, &AFighterPawn::PressedHeavy);
	PlayerInputComponent->BindAction("Player1_Special", IE_Pressed, Player1, &AFighterPawn::PressedSpecial);
	PlayerInputComponent->BindAxis("Player1_Block", Player1, &AFighterPawn::AxisBlock);

	PlayerInputComponent->BindAxis("Player2_W", Player2, &AFighterPawn::AxisS);
	PlayerInputComponent->BindAxis("Player2_A", Player2, &AFighterPawn::AxisD);
	PlayerInputComponent->BindAxis("Player2_S", Player2, &AFighterPawn::AxisW);
	PlayerInputComponent->BindAxis("Player2_D", Player2, &AFighterPawn::AxisA);
	PlayerInputComponent->BindAction("Player2_Light", IE_Pressed, Player2, &AFighterPawn::PressedLight);
	PlayerInputComponent->BindAction("Player2_Medium", IE_Pressed, Player2, &AFighterPawn::PressedMedium);
	PlayerInputComponent->BindAction("Player2_Heavy", IE_Pressed, Player2, &AFighterPawn::PressedHeavy);
	PlayerInputComponent->BindAction("Player2_Special", IE_Pressed, Player2, &AFighterPawn::PressedSpecial);
	PlayerInputComponent->BindAxis("Player2_Block", Player2, &AFighterPawn::AxisBlock);

}

