using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;

// Host ABI v2 smoke only; these structures are NOT Atto guest ABI declarations.
// P/Invoke is source-generated and compatible with NativeAOT; callbacks stay alive
// through the synchronous run and never retain borrowed native pointers.
[StructLayout(LayoutKind.Sequential)]
unsafe struct Config
{
    public uint AbiVersion, Width, Height, MaxEventsPerFrame;
    public ulong FrameLimit;
    public byte* Title;
    public nuint TitleLength;
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
        if (frame != (ulong)(stats->Frames % 2)) return 4;
        stats->Frames++;
        return frame == 1 ? 1 : 0; // STOP on the second frame.
    }

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    private static void Stop(void* context) { ((Stats*)context)->Stops++; }

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    private static int Open(void* context, Config* config)
    {
        if (config->AbiVersion != 2 || config->Width != 800 || config->Height != 500 ||
            config->TitleLength == 0 || config->TitleLength > 127 ||
            config->Title[(int)config->TitleLength] != 0)
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
        if (IntPtr.Size != 8 || sizeof(Config) != 40 || sizeof(App) != 40 ||
            sizeof(Backend) != 40 || Native.Version() != 2)
            throw new InvalidOperationException("Unsupported desktop host ABI/layout");

        byte[] title = Encoding.UTF8.GetBytes("AttoLang .NET desktop interop");
        Stats stats = default;
        fixed (byte* titlePtr = title)
        {
            Config config = new() { AbiVersion = 2, Width = 800, Height = 500,
                MaxEventsPerFrame = 8, FrameLimit = 4, Title = titlePtr, TitleLength = (nuint)title.Length };
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

            byte[] exactTitle = new byte[127];
            Array.Fill(exactTitle, (byte)'X'); // No source terminator.
            byte[] malformedTitle = { 0xc0, 0xaf }; // Overlong invalid UTF-8.
            fixed (byte* exactPtr = exactTitle, badPtr = malformedTitle)
            {
                Config v2 = new() { AbiVersion = 2, Width = 800, Height = 500,
                    MaxEventsPerFrame = 8, FrameLimit = 4, Title = exactPtr,
                    TitleLength = (nuint)exactTitle.Length };
                v2.AbiVersion = 1;
                if (Native.Run(&v2, &app, &backend) != 2 || stats.Opens != 1)
                    throw new InvalidOperationException("v2 version mismatch was admitted");
                v2.AbiVersion = 2;
                v2.Title = badPtr;
                v2.TitleLength = (nuint)malformedTitle.Length;
                if (Native.Run(&v2, &app, &backend) != 2 || stats.Opens != 1)
                    throw new InvalidOperationException("malformed v2 title was admitted");
                v2.Title = exactPtr;
                v2.TitleLength = (nuint)exactTitle.Length;
                if (Native.Run(&v2, &app, &backend) != 0 ||
                    stats.Opens != 2 || stats.Starts != 2 || stats.Frames != 4 ||
                    stats.Presents != 2 || stats.Stops != 2 || stats.Closes != 2 ||
                    stats.Polls != 4)
                    throw new InvalidOperationException("v2 callback lifecycle mismatch");
            }
        }
        Console.WriteLine(".NET host ABI v2: PASS (headless, not an Atto guest test)");
        return 0;
    }
}
