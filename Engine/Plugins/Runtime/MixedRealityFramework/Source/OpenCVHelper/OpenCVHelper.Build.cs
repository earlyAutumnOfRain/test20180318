// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OpenCVHelper: ModuleRules
{
	public OpenCVHelper(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.AddRange(
			new string[] {
				"OpenCVHelper/Public"
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				"OpenCVHelper/Private"
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[] {
            }
        );

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
                "Projects",
                "OpenCV"
            }
        );
	}
}
