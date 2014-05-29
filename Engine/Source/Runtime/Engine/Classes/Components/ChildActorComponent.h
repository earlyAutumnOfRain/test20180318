// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "ChildActorComponent.generated.h"

/** A component that spawns an Actor when registered, and destroys it when unregistered.*/
UCLASS(ClassGroup=Utility, hidecategories=(Object,LOD,Physics,Lighting,TextureStreaming,Activation,"Components|Activation",Collision), meta=(BlueprintSpawnableComponent), MinimalAPI)
class UChildActorComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()

	/** The class of Actor to spawn */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=ChildActorComponent)
	TSubclassOf<AActor>	ChildActorClass;

	/** The actor that we spawned and own */
	UPROPERTY(BlueprintReadOnly, Category = ChildActorComponent, TextExportTransient)
	AActor*	ChildActor;

	/** We try to keep the child actor's name as best we can, so we store it off here when destroying */
	FName ChildActorName;

	// Begin ActorComponent interface.
	virtual void OnComponentCreated() OVERRIDE;
	virtual void OnComponentDestroyed() OVERRIDE;
	virtual TSharedPtr<FComponentInstanceDataBase> GetComponentInstanceData() const OVERRIDE;
	virtual FName GetComponentInstanceDataType() const OVERRIDE;
	virtual void ApplyComponentInstanceData(TSharedPtr<FComponentInstanceDataBase> ComponentInstanceData) OVERRIDE;

	// End ActorComponent interface.

	/** Create the child actor */
	void CreateChildActor();

	/** Kill any currently present child actor */
	void DestroyChildActor();
};



