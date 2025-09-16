using System.IO;
using UnrealBuildTool;

namespace UnrealBuildTool.Rules
{
	public class EmmsUIEditor : ModuleRules
	{
		public EmmsUIEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

			PublicDependencyModuleNames.AddRange(new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"AngelscriptCode",
				"Slate",
				"SlateCore",
				"UMG",
				"EmmsUI",
				"UnrealEd",
				"WorkspaceMenuStructure",
				"ScriptableEditorWidgets",
				"StructUtilsEditor",
				"PropertyEditor",
				"UMGEditor"
			});
		}		
	}
}
