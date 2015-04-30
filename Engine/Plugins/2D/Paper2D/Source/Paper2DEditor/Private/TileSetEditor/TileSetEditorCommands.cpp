// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Paper2DEditorPrivatePCH.h"
#include "TileSetEditorCommands.h"

//////////////////////////////////////////////////////////////////////////
// FTileSetEditorCommands

#define LOCTEXT_NAMESPACE "TileSetEditor"

void FTileSetEditorCommands::RegisterCommands()
{
	// Show toggles
	UI_COMMAND(SetShowCollision, "Collision", "Toggles display of the collision geometry, if any exists.", EUserInterfaceActionType::ToggleButton, FInputChord(EKeys::C, EModifierKey::Alt));
	UI_COMMAND(SetShowGrid, "Grid", "Display the grid.", EUserInterfaceActionType::ToggleButton, FInputChord());
}

#undef LOCTEXT_NAMESPACE