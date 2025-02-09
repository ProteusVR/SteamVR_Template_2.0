// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "AI/Navigation/NavigationAvoidanceTypes.h"
#include "AI/RVOAvoidanceInterface.h"
#include "AITypes.h"
#include "Navigation/PathFollowingComponent.h"
#include "AI/Navigation/NavigationTypes.h"
#include "AI/Navigation/NavigationSystem.h"
#include "Animation/AnimationAsset.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Interfaces/NetworkPredictionInterface.h"
#include "WorldCollision.h"
#include "VRCharacterMovementComponent.generated.h"

class FDebugDisplayInfo;
class ACharacter;
class UVRCharacterMovementComponent;
class AVRCharacter;

/** Shared pointer for easy memory management of FSavedMove_Character, for accumulating and replaying network moves. */
//typedef TSharedPtr<class FSavedMove_Character> FSavedMovePtr;


//=============================================================================
/**
 * VRCharacterMovementComponent handles movement logic for the associated Character owner.
 * It supports various movement modes including: walking, falling, swimming, flying, custom.
 *
 * Movement is affected primarily by current Velocity and Acceleration. Acceleration is updated each frame
 * based on the input vector accumulated thus far (see UPawnMovementComponent::GetPendingInputVector()).
 *
 * Networking is fully implemented, with server-client correction and prediction included.
 *
 * @see ACharacter, UPawnMovementComponent
 * @see https://docs.unrealengine.com/latest/INT/Gameplay/Framework/Pawn/Character/
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAIMoveCompletedSignature, FAIRequestID, RequestID, EPathFollowingResult::Type, Result);


UCLASS()
class VREXPANSIONPLUGIN_API UVRCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()
public:

	UPROPERTY(BlueprintReadOnly, Category = VRMovement)
	UVRRootComponent * VRRootCapsule;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRCharacterMovementComponent")
	bool bAllowWalkingThroughWalls;

	// Higher values will cause more slide but better step up
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRCharacterMovementComponent", meta = (ClampMin = "0.01", UIMin = "0", ClampMax = "1.0", UIMax = "1"))
	float WallRepulsionMultiplier;

	///////////////////////////
	// Navigation Functions
	///////////////////////////


	/** Blueprint notification that we've completed the current movement request */
	//UPROPERTY(BlueprintAssignable, meta = (DisplayName = "MoveCompleted"))
	//FAIMoveCompletedSignature ReceiveMoveCompleted;

#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION <= 12
	/** Called on completing current movement request */
	virtual void OnMoveCompleted(FAIRequestID RequestID, EPathFollowingResult::Type Result);
#else
	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result);
#endif

	/**
	* Checks to see if the current location is not encroaching blocking geometry so the character can leave NavWalking.
	* Restores collision settings and adjusts character location to avoid getting stuck in geometry.
	* If it's not possible, MovementMode change will be delayed until character reach collision free spot.
	* @return True if movement mode was successfully changed
	*/
	virtual bool TryToLeaveNavWalking() override;
	
	virtual void PhysNavWalking(float deltaTime, int32 Iterations) override;
	void ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations) override;

	FORCEINLINE FVector GetActorFeetLocation() const { return VRRootCapsule ? (VRRootCapsule->GetVRLocation() - FVector(0, 0, UpdatedComponent->Bounds.BoxExtent.Z)) : UpdatedComponent ? (UpdatedComponent->GetComponentLocation() - FVector(0, 0, UpdatedComponent->Bounds.BoxExtent.Z)) : FNavigationSystem::InvalidLocation; }
	virtual FBasedPosition GetActorFeetLocationBased() const override
	{
		return FBasedPosition(NULL, GetActorFeetLocation());
	}

	/** Returns status of path following */
	UFUNCTION(BlueprintCallable, Category = "AI|Navigation")
	EPathFollowingStatus::Type GetMoveStatus() const;

	/** Returns true if the current PathFollowingComponent's path is partial (does not reach desired destination). */
	UFUNCTION(BlueprintCallable, Category = "AI|Navigation")
	bool HasPartialPath() const;

	///////////////////////////
	// End Navigation Functions
	///////////////////////////
	

	///////////////////////////
	// Replication Functions
	///////////////////////////
	void CallServerMoveVR(const class FSavedMove_VRCharacter* NewMove, const class FSavedMove_VRCharacter* OldMove);

	/** Replicated function sent by client to server - contains client movement and view info. */
	UFUNCTION(unreliable, server, WithValidation)
	virtual void ServerMoveVR(float TimeStamp, FVector_NetQuantize10 InAccel, FVector_NetQuantize100 ClientLoc, FVector_NetQuantize100 CapsuleLoc, FVector_NetQuantize100 rRequestedVelocity, FVector_NetQuantize100 LFDiff, uint8 CapsuleYaw, uint8 CompressedMoveFlags, uint8 ClientRoll, uint32 View, UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName, uint8 ClientMovementMode);
	virtual void ServerMoveVR_Implementation(float TimeStamp, FVector_NetQuantize10 InAccel, FVector_NetQuantize100 ClientLoc, FVector_NetQuantize100 CapsuleLoc, FVector_NetQuantize100 rRequestedVelocity, FVector_NetQuantize100 LFDiff, uint8 CapsuleYaw, uint8 CompressedMoveFlags, uint8 ClientRoll, uint32 View, UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName, uint8 ClientMovementMode);
	virtual bool ServerMoveVR_Validate(float TimeStamp, FVector_NetQuantize10 InAccel, FVector_NetQuantize100 ClientLoc, FVector_NetQuantize100 CapsuleLoc, FVector_NetQuantize100 rRequestedVelocity, FVector_NetQuantize100 LFDiff, uint8 CapsuleYaw, uint8 CompressedMoveFlags, uint8 ClientRoll, uint32 View, UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName, uint8 ClientMovementMode);

	/** Replicated function sent by client to server - contains client movement and view info for two moves. */
	UFUNCTION(unreliable, server, WithValidation)
	virtual void ServerMoveVRDual(float TimeStamp0, FVector_NetQuantize10 InAccel0, uint8 PendingFlags, uint32 View0, FVector_NetQuantize100 OldCapsuleLoc, FVector_NetQuantize100 rOldRequestedVelocity, FVector_NetQuantize100 OldLFDiff, uint8 OldCapsuleYaw, float TimeStamp, FVector_NetQuantize10 InAccel, FVector_NetQuantize100 ClientLoc, FVector_NetQuantize100 CapsuleLoc, FVector_NetQuantize100 rRequestedVelocity, FVector_NetQuantize100 LFDiff, uint8 CapsuleYaw, uint8 NewFlags, uint8 ClientRoll, uint32 View, UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName, uint8 ClientMovementMode);
	virtual void ServerMoveVRDual_Implementation(float TimeStamp0, FVector_NetQuantize10 InAccel0, uint8 PendingFlags, uint32 View0, FVector_NetQuantize100 OldCapsuleLoc, FVector_NetQuantize100 rOldRequestedVelocity, FVector_NetQuantize100 OldLFDiff, uint8 OldCapsuleYaw, float TimeStamp, FVector_NetQuantize10 InAccel, FVector_NetQuantize100 ClientLoc, FVector_NetQuantize100 CapsuleLoc, FVector_NetQuantize100 rRequestedVelocity, FVector_NetQuantize100 LFDiff, uint8 CapsuleYaw, uint8 NewFlags, uint8 ClientRoll, uint32 View, UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName, uint8 ClientMovementMode);
	virtual bool ServerMoveVRDual_Validate(float TimeStamp0, FVector_NetQuantize10 InAccel0, uint8 PendingFlags, uint32 View0, FVector_NetQuantize100 OldCapsuleLoc, FVector_NetQuantize100 rOldRequestedVelocity, FVector_NetQuantize100 OldLFDiff, uint8 OldCapsuleYaw, float TimeStamp, FVector_NetQuantize10 InAccel, FVector_NetQuantize100 ClientLoc, FVector_NetQuantize100 CapsuleLoc, FVector_NetQuantize100 rRequestedVelocity, FVector_NetQuantize100 LFDiff, uint8 CapsuleYaw, uint8 NewFlags, uint8 ClientRoll, uint32 View, UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName, uint8 ClientMovementMode);

	/** Replicated function sent by client to server - contains client movement and view info for two moves. First move is non root motion, second is root motion. */
	UFUNCTION(unreliable, server, WithValidation)
	virtual void ServerMoveVRDualHybridRootMotion(float TimeStamp0, FVector_NetQuantize10 InAccel0, uint8 PendingFlags, uint32 View0, FVector_NetQuantize100 OldCapsuleLoc,FVector_NetQuantize100 rOldRequestedVelocity, FVector_NetQuantize100 OldLFDiff, uint8 OldCapsuleYaw, float TimeStamp, FVector_NetQuantize10 InAccel, FVector_NetQuantize100 ClientLoc, FVector_NetQuantize100 CapsuleLoc, FVector_NetQuantize100 rRequestedVelocity, FVector_NetQuantize100 LFDiff, uint8 CapsuleYaw, uint8 NewFlags, uint8 ClientRoll, uint32 View, UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName, uint8 ClientMovementMode);
	virtual void ServerMoveVRDualHybridRootMotion_Implementation(float TimeStamp0, FVector_NetQuantize10 InAccel0, uint8 PendingFlags, uint32 View0, FVector_NetQuantize100 OldCapsuleLoc, FVector_NetQuantize100 rOldRequestedVelocity, FVector_NetQuantize100 OldLFDiff, uint8 OldCapsuleYaw, float TimeStamp, FVector_NetQuantize10 InAccel, FVector_NetQuantize100 ClientLoc, FVector_NetQuantize100 CapsuleLoc, FVector_NetQuantize100 rRequestedVelocity, FVector_NetQuantize100 LFDiff, uint8 CapsuleYaw, uint8 NewFlags, uint8 ClientRoll, uint32 View, UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName, uint8 ClientMovementMode);
	virtual bool ServerMoveVRDualHybridRootMotion_Validate(float TimeStamp0, FVector_NetQuantize10 InAccel0, uint8 PendingFlags, uint32 View0, FVector_NetQuantize100 OldCapsuleLoc, FVector_NetQuantize100 rOldRequestedVelocity, FVector_NetQuantize100 OldLFDiff, uint8 OldCapsuleYaw, float TimeStamp, FVector_NetQuantize10 InAccel, FVector_NetQuantize100 ClientLoc, FVector_NetQuantize100 CapsuleLoc, FVector_NetQuantize100 rRequestedVelocity, FVector_NetQuantize100 LFDiff, uint8 CapsuleYaw, uint8 NewFlags, uint8 ClientRoll, uint32 View, UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName, uint8 ClientMovementMode);


	FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	FNetworkPredictionData_Server* GetPredictionData_Server() const override;

	///////////////////////////
	// End Replication Functions
	///////////////////////////

	/**
	 * Default UObject constructor.
	 */
	UVRCharacterMovementComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	float ImmersionDepth() const override;
	void VisualizeMovement() const override;
	bool CanCrouch();

	/*
	bool HasRootMotion() const
	{
		return RootMotionParams.bHasRootMotion;
	}*/

	// Had to modify this, since every frame can start in penetration (capsule component moves into wall before movement tick)
	// It was throwing out the initial hit and not calling "step up", now I am only checking for penetration after adjustment but keeping the initial hit for step up.
	// this makes for FAR more responsive step ups.
	// I WILL need to override these for flying / swimming as well
	bool SafeMoveUpdatedComponent(const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult& OutHit, ETeleportType Teleport = ETeleportType::None);
	bool SafeMoveUpdatedComponent(const FVector& Delta, const FRotator& NewRotation, bool bSweep, FHitResult& OutHit, ETeleportType Teleport = ETeleportType::None);
	
	// This is here to force it to call the correct SafeMoveUpdatedComponent functions for floor movement
	void UVRCharacterMovementComponent::MoveAlongFloor(const FVector& InVelocity, float DeltaSeconds, FStepDownResult* OutStepDownResult) override;

	// Modify for correct location
	void ApplyRepulsionForce(float DeltaSeconds) override;

	// Update BaseOffset to be zero
	void UpdateBasedMovement(float DeltaSeconds) override;

	// Stop subtracting the capsules half height
	FVector GetImpartedMovementBaseVelocity() const override;

	// Cheating at the relative collision detection
	void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction);

	// Need to fill our capsule component variable here and override the default tick ordering
	void SetUpdatedComponent(USceneComponent* NewUpdatedComponent) override;

	// Correct an offset sweep test
	void ReplicateMoveToServer(float DeltaTime, const FVector& NewAcceleration) override;

	// Always called with the capsulecomponent location, no idea why it doesn't just get it inside it already
	// Had to force it within the function to use VRLocation instead.
	void FindFloor(const FVector& CapsuleLocation, FFindFloorResult& OutFloorResult, bool bZeroDelta, const FHitResult* DownwardSweepResult) const override;

	// Need to use actual capsule location for step up
	bool StepUp(const FVector& GravDir, const FVector& Delta, const FHitResult &InHit, FStepDownResult* OutStepDownResult);

	// Skip physics channels when looking for floor
	bool FloorSweepTest(
		FHitResult& OutHit,
		const FVector& Start,
		const FVector& End,
		ECollisionChannel TraceChannel,
		const struct FCollisionShape& CollisionShape,
		const struct FCollisionQueryParams& Params,
		const struct FCollisionResponseParams& ResponseParam
		) const override;

	// Multiple changes to support relative motion and ledge sweeps
	void PhysWalking(float deltaTime, int32 Iterations);

};


class VREXPANSIONPLUGIN_API FSavedMove_VRCharacter : public FSavedMove_Character
{

public:

	FVector VRCapsuleLocation;
	FVector LFDiff;
	FRotator VRCapsuleRotation;
	FVector RequestedVelocity;

	bool bHasRequestedMove;

	void Clear();
	virtual void SetInitialPosition(ACharacter* C);

	FSavedMove_VRCharacter() : FSavedMove_Character()
	{
		VRCapsuleLocation = FVector::ZeroVector;
		LFDiff = FVector::ZeroVector;
		VRCapsuleRotation = FRotator::ZeroRotator;
		RequestedVelocity = FVector::ZeroVector;
		bHasRequestedMove = false;
	}
};

// Need this for capsule location replication
class VREXPANSIONPLUGIN_API FNetworkPredictionData_Client_VRCharacter : public FNetworkPredictionData_Client_Character
{
public:
	FNetworkPredictionData_Client_VRCharacter(const UCharacterMovementComponent& ClientMovement)
		: FNetworkPredictionData_Client_Character(ClientMovement)
	{

	}

	FSavedMovePtr AllocateNewMove()
	{
		return FSavedMovePtr(new FSavedMove_VRCharacter());
	}
};


// Need this for capsule location replication?????
class VREXPANSIONPLUGIN_API FNetworkPredictionData_Server_VRCharacter : public FNetworkPredictionData_Server_Character
{
public:
	FNetworkPredictionData_Server_VRCharacter(const UCharacterMovementComponent& ClientMovement)
		: FNetworkPredictionData_Server_Character(ClientMovement)
	{

	}

	FSavedMovePtr AllocateNewMove()
	{
		return FSavedMovePtr(new FSavedMove_VRCharacter());
	}
};