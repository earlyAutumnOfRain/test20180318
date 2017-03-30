// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "InsertEdgeLoop.h"
#include "IMeshEditorModeEditingContract.h"
#include "IMeshEditorModeUIContract.h"
#include "UICommandInfo.h"
#include "EditableMesh.h"
#include "MeshElement.h"
#include "MultiBoxBuilder.h"
#include "UICommandList.h"


#define LOCTEXT_NAMESPACE "MeshEditorMode"


void UInsertEdgeLoopCommand::RegisterUICommand( FBindingContext* BindingContext )
{
	UI_COMMAND_EXT( BindingContext, /* Out */ UICommandInfo, "InsertEdgeLoop", "Insert Edge Loop Mode", "Set the primary action to insert edge loops.", EUserInterfaceActionType::RadioButton, FInputChord() );
}


void UInsertEdgeLoopCommand::ApplyDuringDrag( IMeshEditorModeEditingContract& MeshEditorMode, UViewportInteractor* ViewportInteractor, bool& bOutShouldDeselectAllFirst, TArray<FMeshElement>& OutMeshElementsToSelect )
{
	// Insert edge loop
	static TMap< UEditableMesh*, TArray< FMeshElement > > SelectedMeshesAndEdges;
	MeshEditorMode.GetSelectedMeshesAndEdges( /* Out */ SelectedMeshesAndEdges );

	if( SelectedMeshesAndEdges.Num() > 0 )
	{
		// Deselect the edges first, since they'll be deleted or split up while inserting the edge loop,
		// and we want them to be re-selected after undo
		MeshEditorMode.DeselectMeshElements( SelectedMeshesAndEdges );


		for( auto& MeshAndEdges : SelectedMeshesAndEdges )
		{
			UEditableMesh* EditableMesh = MeshAndEdges.Key;
			verify( !EditableMesh->AnyChangesToUndo() );

			const TArray<FMeshElement>& EdgeElements = MeshAndEdges.Value;

			// Figure out where to add the loop along the edge
			static TArray<float> Splits;
			MeshEditorMode.FindEdgeSplitUnderInteractor( MeshEditorMode.GetActiveActionInteractor(), EditableMesh, EdgeElements, /* Out */ Splits );

			// Insert the edge loop
			if( Splits.Num() > 0 )
			{
				// @todo mesheditor edgeloop: Test code
				if( false )
				{
					if( Splits[ 0 ] > 0.25f )
					{
						Splits.Insert( FMath::Max( Splits[ 0 ] - 0.2f, 0.0f ), 0 );
					}
					if( Splits.Last() < 0.75f )
					{
						Splits.Add( FMath::Min( Splits.Last() + 0.2f, 1.0f ) );
					}
				}

				for( const FMeshElement& EdgeMeshElement : EdgeElements )
				{
					const FEdgeID EdgeID( EdgeMeshElement.ElementAddress.ElementID );

					static TArray<FEdgeID> NewEdgeIDs;
					NewEdgeIDs.Reset();

					EditableMesh->InsertEdgeLoop( EdgeID, Splits, /* Out */ NewEdgeIDs );

					// Select all of the new edges that were created by inserting the loop
					if( NewEdgeIDs.Num() > 0 )
					{
						bOutShouldDeselectAllFirst = true;	// Don't keep the edge selected

						for( const FEdgeID NewEdgeID : NewEdgeIDs )
						{
							FMeshElement MeshElementToSelect;
							{
								MeshElementToSelect.Component = EdgeMeshElement.Component;
								MeshElementToSelect.ElementAddress.SubMeshAddress = EdgeMeshElement.ElementAddress.SubMeshAddress;
								MeshElementToSelect.ElementAddress.ElementType = EEditableMeshElementType::Edge;
								MeshElementToSelect.ElementAddress.ElementID = NewEdgeID;
							}

							// Queue selection of this new element.  We don't want it to be part of the current action.
							OutMeshElementsToSelect.Add( MeshElementToSelect );
						}
					}
				}

				MeshEditorMode.TrackUndo( EditableMesh, EditableMesh->MakeUndo() );
			}
		}
	}
}


void UInsertEdgeLoopCommand::AddToVRRadialMenuActionsMenu( IMeshEditorModeUIContract& MeshEditorMode, FMenuBuilder& MenuBuilder, TSharedPtr<FUICommandList> CommandList, const FName TEMPHACK_StyleSetName, class UVREditorMode* VRMode )
{
	if( MeshEditorMode.GetMeshElementSelectionMode() == EEditableMeshElementType::Edge )
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("VRInsertEdgeLoop", "Insert Loop"),
			FText(),
			FSlateIcon(TEMPHACK_StyleSetName, "MeshEditorMode.EdgeInsert"),	// @todo mesheditor extensibility: TEMPHACK for style; Need PolygonModelingStyle, probably.  Or we're just cool with exporting MeshEditorModeStyle, since we're all the same plugin after all.
			MakeUIAction( MeshEditorMode ),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
			);
	}
}


#undef LOCTEXT_NAMESPACE

