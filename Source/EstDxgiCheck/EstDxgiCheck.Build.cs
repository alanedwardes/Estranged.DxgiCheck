using UnrealBuildTool;

public class EstDxgiCheck : ModuleRules
{
    public EstDxgiCheck(ReadOnlyTargetRules Target) : base(Target)
    {
        IWYUSupport = IWYUSupport.Full;
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PrivateDependencyModuleNames.AddRange(new string[] { "Core", "Projects", "HTTP" });

        if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
        {
            PublicSystemLibraries.Add("DXGI.lib");
            PublicSystemLibraries.Add("d3d11.lib");
            PublicSystemLibraries.Add("d3d12.lib");
        }
    }
}
