using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;

// Host ABI v1 smoke only; these structures are NOT Atto guest ABI declarations.
// P/Invoke is source-generated and compatible with NativeAOT; callbacks stay alive
// through the synchronous run and never retain borrowed native pointers.
[StructLayout(LayoutKind.Sequential)]
unsafe struct Config
{
    public uint AbiVersion, Width, Height, MaxEventsPerFrame;
    public ulong FrameLimit;
    public byte* Title;
}

[StructLayout(LayoutKind.Sequential)]
unsafe struct App
{
    public void* Context;
    public delegate* unmanaged[Cdecl]<void*, int> OnStart;
    public delegate* unmanaged[Cdecl]<void*, void*, int> OnEvent;
    public delegate* unmanaged[Cdecl]<void*, ulong, int> OnFrame;
    public delegate* unmanaged[Cdecl]<void*, void> OnStop;
}

[StructLayout(LayoutKind.Sequential)]
unsafe struct Backend
{
    public void* Context;
    public delegate* unmanaged[Cdecl]<void*, Config*, int> Open;
    public delegate* unmanaged[Cdecl]<void*, void*, int*, int> Poll;
    public delegate* unmanaged[Cdecl]<void*, int> Present;
    public delegate* unmanaged[Cdecl]<void*, void> Close;
}

[StructLayout(LayoutKind.Sequential)]
struct Stats
{
    public int Opens, Starts, Frames, Presents, Stops, Closes, Polls;
}

internal static unsafe partial class Native
{
    [LibraryImport("ores_desktop", EntryPoint = "ores_desktop_abi_version")]
    [UnmanagedCallConv(CallConvs = new[] { typeof(CallConvCdecl) })]
    internal static partial uint Version();

    [LibraryImport("ores_desktop", EntryPoint = "ores_desktop_run")]
    [UnmanagedCallConv(CallConvs = new[] { typeof(CallConvCdecl) })]
    internal static partial int Run(Config* config, App* app, Backend* backend);
}

internal static unsafe class Program
{
    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    private static int Start(void* context) { ((Stats*)context)->Starts++; return 0; }

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    private static int Frame(void* context, ulong frame)
    {
        Stats* stats = (Stats*)context;
        if (frame != (ulong)stats->Frames) return 4;
        stats->Frames++;
        return frame == 1 ? 1 : 0; // STOP on the second frame.
    }

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    private static void Stop(void* context) { ((Stats*)context)->Stops++; }

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    private static int Open(void* context, Config* config)
    {
        if (config->AbiVersion != 1 || config->Width != 800 || config->Height != 500)
            return 3;
        ((Stats*)context)->Opens++;
        return 0;
    }

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    private static int Poll(void* context, void* eventData, int* hasEvent)
    {
        ((Stats*)context)->Polls++;
        *hasEvent = 0;
        return 0;
    }

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    private static int Present(void* context) { ((Stats*)context)->Presents++; return 0; }

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    private static void Close(void* context) { ((Stats*)context)->Closes++; }

    private static int Main()
    {
        if (IntPtr.Size != 8 || sizeof(Config) != 32 || sizeof(App) != 40 ||
            sizeof(Backend) != 40 || Native.Version() != 1)
            throw new InvalidOperationException("Unsupported desktop host ABI/layout");

        byte[] title = Encoding.UTF8.GetBytes("AttoLang .NET desktop interop\0");
        Stats stats = default;
        fixed (byte* titlePtr = title)
        {
            Config config = new() { AbiVersion = 1, Width = 800, Height = 500,
                MaxEventsPerFrame = 8, FrameLimit = 4, Title = titlePtr };
            App app = new() { Context = &stats, OnStart = &Start, OnFrame = &Frame, OnStop = &Stop };
            Backend backend = new() { Context = &stats, Open = &Open, Poll = &Poll,
                Present = &Present, Close = &Close };
            Config invalid = config;
            invalid.AbiVersion = 99;
            if (Native.Run(&invalid, &app, &backend) != 2 || stats.Opens != 0)
                throw new InvalidOperationException("ABI version must fail closed");
            if (Native.Run(&config, &app, &backend) != 0 ||
                stats.Opens != 1 || stats.Starts != 1 || stats.Frames != 2 ||
                stats.Presents != 1 || stats.Stops != 1 || stats.Closes != 1 || stats.Polls != 2)
                throw new InvalidOperationException("Host callback lifecycle mismatch");
        }
        Console.WriteLine(".NET host ABI v1: PASS (headless, not an Atto guest test)");
        return 0;
    }
}
