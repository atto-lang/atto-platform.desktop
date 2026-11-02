import java.lang.foreign.Arena;
import java.lang.foreign.AddressLayout;
import java.lang.foreign.FunctionDescriptor;
import java.lang.foreign.Linker;
import java.lang.foreign.MemoryLayout;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.SymbolLookup;
import java.lang.foreign.ValueLayout;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.nio.file.Path;

// JDK 25 FFM: useful on HotSpot and GraalVM Native Image, but never a claim
// that AttoLang Java guest code can call an SDL window directly.
public final class DesktopInteropProbe {
    private static final Linker LINKER = Linker.nativeLinker();
    private static final ValueLayout.OfInt I32 = ValueLayout.JAVA_INT;
    private static final ValueLayout.OfLong I64 = ValueLayout.JAVA_LONG;
    private static final AddressLayout PTR = ValueLayout.ADDRESS;
    private static final MemoryLayout CONFIG = MemoryLayout.structLayout(
            I32, I32, I32, I32, I64, PTR);
    private static final MemoryLayout APP = MemoryLayout.structLayout(PTR, PTR, PTR, PTR, PTR);
    private static final MemoryLayout BACKEND = MemoryLayout.structLayout(PTR, PTR, PTR, PTR, PTR);
    private static int opens, starts, frames, polls, presents, stops, closes;

    private static int onStart(MemorySegment context) { starts++; return 0; }
    private static int onFrame(MemorySegment context, long number) {
        if (number != frames) return 4;
        frames++;
        return number == 1 ? 1 : 0;
    }
    private static void onStop(MemorySegment context) { stops++; }
    private static int open(MemorySegment context, MemorySegment config) {
        MemorySegment view = config.reinterpret(CONFIG.byteSize());
        if (view.get(I32, 0) != 1 || view.get(I32, 4) != 800 || view.get(I32, 8) != 500)
            return 3;
        opens++;
        return 0;
    }
    private static int poll(MemorySegment context, MemorySegment event, MemorySegment available) {
        polls++;
        available.reinterpret(I32.byteSize()).set(I32, 0, 0);
        return 0;
    }
    private static int present(MemorySegment context) { presents++; return 0; }
    private static void close(MemorySegment context) { closes++; }

    private static MemorySegment upcall(String method, MethodType type,
                                        FunctionDescriptor descriptor, Arena arena)
            throws ReflectiveOperationException {
        MethodHandle handle = MethodHandles.lookup().findStatic(DesktopInteropProbe.class, method, type);
        return LINKER.upcallStub(handle, descriptor, arena);
    }

    private static void check(boolean condition, String message) {
        if (!condition) throw new IllegalStateException(message);
    }

    public static void main(String[] args) throws Throwable {
        check(args.length == 1, "Usage: DesktopInteropProbe /absolute/path/to/libores_desktop");
        check(PTR.byteSize() == 8 && CONFIG.byteSize() == 32 &&
              APP.byteSize() == 40 && BACKEND.byteSize() == 40,
              "Unsupported host pointer width / C ABI layout");
        try (Arena arena = Arena.ofConfined()) {
            SymbolLookup library = SymbolLookup.libraryLookup(Path.of(args[0]).toAbsolutePath(), arena);
            MethodHandle version = LINKER.downcallHandle(
                    library.find("ores_desktop_abi_version").orElseThrow(),
                    FunctionDescriptor.of(I32));
            check((int) version.invokeExact() == 1, "Unsupported desktop ABI version");
            MethodHandle run = LINKER.downcallHandle(
                    library.find("ores_desktop_run").orElseThrow(),
                    FunctionDescriptor.of(I32, PTR, PTR, PTR));

            MemorySegment cfg = arena.allocate(CONFIG);
            cfg.set(I32, 0, 1); cfg.set(I32, 4, 800); cfg.set(I32, 8, 500);
            cfg.set(I32, 12, 8); cfg.set(I64, 16, 4);
            cfg.set(PTR, 24, arena.allocateFrom("AttoLang GraalVM desktop interop"));
            MemorySegment app = arena.allocate(APP);
            app.set(PTR, 8, upcall("onStart", MethodType.methodType(int.class, MemorySegment.class),
                    FunctionDescriptor.of(I32, PTR), arena));
            app.set(PTR, 24, upcall("onFrame", MethodType.methodType(int.class, MemorySegment.class, long.class),
                    FunctionDescriptor.of(I32, PTR, I64), arena));
            app.set(PTR, 32, upcall("onStop", MethodType.methodType(void.class, MemorySegment.class),
                    FunctionDescriptor.ofVoid(PTR), arena));
            MemorySegment backend = arena.allocate(BACKEND);
            backend.set(PTR, 8, upcall("open", MethodType.methodType(int.class, MemorySegment.class, MemorySegment.class),
                    FunctionDescriptor.of(I32, PTR, PTR), arena));
            backend.set(PTR, 16, upcall("poll", MethodType.methodType(int.class, MemorySegment.class,
                    MemorySegment.class, MemorySegment.class), FunctionDescriptor.of(I32, PTR, PTR, PTR), arena));
            backend.set(PTR, 24, upcall("present", MethodType.methodType(int.class, MemorySegment.class),
                    FunctionDescriptor.of(I32, PTR), arena));
            backend.set(PTR, 32, upcall("close", MethodType.methodType(void.class, MemorySegment.class),
                    FunctionDescriptor.ofVoid(PTR), arena));

            cfg.set(I32, 0, 99);
            check((int) run.invokeExact(cfg, app, backend) == 2 && opens == 0,
                    "Unsupported ABI version must fail closed");
            cfg.set(I32, 0, 1);
            check((int) run.invokeExact(cfg, app, backend) == 0, "Host run failed");
            check(opens == 1 && starts == 1 && frames == 2 && presents == 1 &&
                  stops == 1 && closes == 1 && polls == 2, "Lifecycle callback mismatch");
            System.out.println("GraalVM/Java FFM host ABI v1: PASS (headless, not an Atto guest test)");
        }
    }
}
