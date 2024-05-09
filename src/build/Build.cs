using System;
using System.IO;

namespace SummerGame
{
    class Build
    {
        const BuildMode GameBuildMode = BuildMode.Debug;

        const string RootDirectory = "build";
        const string TempBuildDirectory = "build/temp";
        const string ObjectFilesDirectory = "build/temp/obj";
        static string OutputBuildDirectory;

        public static void Main(string[] args)
        {
            OutputBuildDirectory = $"build/SummerGame_{GameBuildMode}";
            using var timer = new Timer("Build finished in {time}s");

            var compiler = new Compiler();
            compiler.BuildMode = GameBuildMode;
            compiler.ObjectFilesOutputDirectory = ObjectFilesDirectory;
            compiler.OutputDirectory = TempBuildDirectory;

            var reflect = new Reflect();
            reflect.ScanPath = "reflect/build/scan.exe";
            reflect.ReflectIncludeDirectory = "reflect/src/runtime/";
            reflect.BuildDirectory = TempBuildDirectory;

            var version = "";//GetVersion();

            Console.WriteLine($"Building Summer Game ({GameBuildMode}, Version: {version})");

            CreateBuildDirectories(
                RootDirectory,
                TempBuildDirectory,
                ObjectFilesDirectory,
                OutputBuildDirectory
            );

            BuildImGui(compiler);
            BuildCore(compiler, reflect, version);
            BuildGameModule(compiler, reflect);

            File.Copy("lib/x64__SDL2.dll", Path.Combine(OutputBuildDirectory, "SDL2.dll"), true);

            var results = new[]
            {
                "ImGui.dll",
                "ImGui.pdb",
                "Game.dll",
                "Game.pdb",
                "SummerGame.exe",
                "SummerGame.pdb",
            };

            CopyResults(results);


            if (File.Exists("vc140.pdb"))
            {
                var dst = Path.Combine(OutputBuildDirectory, "vc140.pdb");
                File.Delete(dst);
                File.Move("vc140.pdb", dst);
            }

            return;
        }

        private static void CopyResults(string[] files)
        {
            foreach (var file in files)
            {
                var src = Path.Combine(TempBuildDirectory, file);
                var dst = Path.Combine(OutputBuildDirectory, file);
                if (File.Exists(src))
                {
                    File.Copy(src, dst, true);
                }
            }
        }

        private static void CreateBuildDirectories(string root, string tempBuildDirectory, string objectFilesDirectory, string outputBuildDirectory)
        {
            if (!Directory.Exists("build"))
            {
                Directory.CreateDirectory("build");
            }

            if (Directory.Exists(tempBuildDirectory))
            {
                Directory.Delete(tempBuildDirectory, true);
            }

            Directory.CreateDirectory(tempBuildDirectory);
            Directory.CreateDirectory(objectFilesDirectory);
            Directory.CreateDirectory(outputBuildDirectory);
        }

        private static string GetVersion()
        {
            return Utils.ExecuteCommand("git", "describe --tags --long --always").Unwrap().Stdout.TrimEnd().TrimStart();
        }

        private static void BuildGameModule(Compiler compiler, Reflect reflect)
        {
            var metaTask = new BuildTask();
            metaTask.Language = Language.Cpp;
            metaTask.OutputType = OutputType.DLL;
            metaTask.ResultName = "Metaprogram";
            metaTask.DllExportedProc = "Metaprogram";
            metaTask.IncludeDirectories.Add("ext/cimgui/");
            metaTask.IncludeDirectories.Add("reflect/src");
            metaTask.Sources.Add("src/metaprogram/Metaprogram.cpp");

            var libTask = new BuildTask();
            libTask.Language = Language.C;
            libTask.OutputType = OutputType.DLL;
            libTask.ResultName = "Game";
            libTask.DllExportedProc = "GameUpdateAndRender";

            libTask.IncludeDirectories.Add("ext/cimgui/");
            // TODO: remove.
            libTask.IncludeDirectories.Add("ext/");
            libTask.IncludeDirectories.Add("reflect/src/runtime/");

            libTask.Defines.Add("_CRT_SECURE_NO_WARNINGS");
            libTask.Defines.Add("PLATFORM_WINDOWS");
            libTask.Defines.Add("UNICODE");
            libTask.Defines.Add("_UNICODE");

            libTask.Sources.Add("src/GameEntry.c");

            compiler.Compile(metaTask).Unwrap();
            reflect.Run(libTask, metaTask).Unwrap();
            compiler.Compile(libTask).Unwrap();
        }

        private static void BuildCore(Compiler compiler, Reflect reflect, string version)
        {
            var metaTask = new BuildTask();
            metaTask.Language = Language.Cpp;
            metaTask.OutputType = OutputType.DLL;
            metaTask.ResultName = "D3D11RendererMetaprogram";
            metaTask.DllExportedProc = "Metaprogram";
            metaTask.Defines.Add("META_PASS");
            metaTask.IncludeDirectories.Add("reflect/src");
            metaTask.Sources.Add("src/renderer/D3D11_Metaprogram.cpp");

            var task = new BuildTask();
            task.Language = Language.Cpp;
            task.OutputType = OutputType.Executable;
            task.ResultName = "SummerGame";

            task.Defines.Add($"PLATFORM_WINDOW_HEADER_VALUE=\"\\\" Summer Game ({GameBuildMode}, {version})\\\"\"");
            task.Defines.Add("_CRT_SECURE_NO_WARNINGS");
            //task.Defines.Add("WIN32_LEAN_AND_MEAN");
            task.Defines.Add("PLATFORM_WINDOWS");
            task.Defines.Add("UNICODE");
            task.Defines.Add("_UNICODE");

            task.IncludeDirectories.Add("ext/cimgui");
            task.IncludeDirectories.Add("ext/sdl2/include");
            task.IncludeDirectories.Add("reflect/src/runtime");

            task.ExternalLibs.Add("lib/x64__SDL2.lib");
            task.ExternalLibs.Add("user32.lib");
            task.ExternalLibs.Add("winmm.lib");
            task.ExternalLibs.Add("shcore.lib");
            task.ExternalLibs.Add("advapi32.lib");

            // DirectX 11
            task.ExternalLibs.Add("dxgi.lib");
            task.ExternalLibs.Add("D3D11.lib");
            task.ExternalLibs.Add("D3DCompiler.lib");

            task.Sources.Add("src/renderer/D3D11_RendererEntry.cpp");

            compiler.Compile(metaTask).Unwrap();
            reflect.Run(task, metaTask).Unwrap();

            // Add these after reflection because they cause it to fail.
            task.Sources.Add("src/core/Win32Core.cpp");

            compiler.Compile(task).Unwrap();
        }


        private static void BuildImGui(Compiler compiler)
        {
            var imguiTask = new BuildTask();
            imguiTask.Language = Language.C;
            imguiTask.OutputType = OutputType.DLL;
            imguiTask.ResultName = "ImGui";
            imguiTask.DllExportedProc = "GetImGuiAPI";

            imguiTask.Defines.Add("IMGUI_IMPL_API=\"extern \\\"C\\\"\"");
            imguiTask.Defines.Add("CIMGUI_NO_EXPORT");
            imguiTask.Defines.Add("IMGUI_DISABLE_OBSOLETE_FUNCTIONS=1");
            imguiTask.Defines.Add("CIMGUI_USE_SDL2");
            imguiTask.Defines.Add("CIMGUI_USE_DX11");

            imguiTask.IncludeDirectories.Add("ext/cimgui/");
            imguiTask.IncludeDirectories.Add("ext/cimgui/generator/output");
            imguiTask.IncludeDirectories.Add("ext/cimgui/imgui/");
            imguiTask.IncludeDirectories.Add("ext/SDL2/include/");

            imguiTask.ExternalLibs.Add("lib/x64__SDL2.lib");

            imguiTask.Sources.Add("src/ImGui/ImGuiApi.c");
            imguiTask.Sources.Add("ext/cimgui/cimgui.cpp");
            imguiTask.Sources.Add("ext/cimgui/imgui/imgui.cpp");
            imguiTask.Sources.Add("ext/cimgui/imgui/imgui_draw.cpp");
            imguiTask.Sources.Add("ext/cimgui/imgui/imgui_demo.cpp");
            imguiTask.Sources.Add("ext/cimgui/imgui/imgui_widgets.cpp");
            imguiTask.Sources.Add("ext/cimgui/imgui/imgui_tables.cpp");
            imguiTask.Sources.Add("ext/cimgui/imgui/backends/imgui_impl_sdl2.cpp");
            imguiTask.Sources.Add("ext/cimgui/imgui/backends/imgui_impl_dx11.cpp");

            compiler.Compile(imguiTask);
        }
    }
}
