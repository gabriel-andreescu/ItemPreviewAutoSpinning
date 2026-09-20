using BMK.Mutagen.Skyrim;
using Mutagen.Bethesda.Skyrim;

namespace ItemPreviewAutoSpinning.Generator;

internal static class Mcm
{
    public static void AddQuest(SkyrimMod mod)
    {
        McmQuest.Add(
            mod,
            new McmQuestOptions
            {
                EditorId = "ItemPreviewAutoSpinning_MCMQuest",
                DisplayName = "Item Preview Auto Spinning",
                ConfigScriptName = "ItemPreviewAutoSpinning_MCM",
                ModName = "ItemPreviewAutoSpinning",
            }
        );
    }
}
