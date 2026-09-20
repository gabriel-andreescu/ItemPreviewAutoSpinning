using Mutagen.Bethesda;
using Mutagen.Bethesda.Plugins.Allocators;
using Mutagen.Bethesda.Skyrim;

namespace ItemPreviewAutoSpinning.Generator;

internal static class Program
{
    private static void Main(string[] args)
    {
        var output = Path.GetFullPath(args[0]);
        var mod = new SkyrimMod("ItemPreviewAutoSpinning.esp", SkyrimRelease.SkyrimSE)
        {
            IsSmallMaster = true,
        };
        using var allocator = new TextFileFormKeyAllocator(mod, args[1])
        {
            CommitOnDispose = false,
        };
        mod.SetAllocator(allocator);

        Mcm.AddQuest(mod);
        Directory.CreateDirectory(Path.Combine(output, "mcm"));
        mod.WriteToBinary(Path.Combine(output, "mcm", "ItemPreviewAutoSpinning.esp"));

        allocator.Commit();
    }
}
