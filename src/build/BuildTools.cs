using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.Text;
using System.IO;
using System;

namespace SummerGame
{
    public enum BuildMode
    {
        Debug,
        Release
    }

    public enum Language
    {
        C,
        Cpp
    }

    public enum OutputType
    {
        Executable,
        DLL
    }

    public static class ConsoleTags
    {
        public static string Running = "[Running]";
        public static string Failed = "[Failed!]";
        public static string Done = "[Done...]";
    }

    public class BuildTask
    {
        public Language Language;
        public OutputType OutputType;
        public List<string> Sources = new();
        public List<string> Defines = new();
        public List<string> IncludeDirectories = new();
        public List<string> ExternalLibs = new();
        public string ResultName;
        public string DllExportedProc;
    }

    public class ExecutionResult
    {
        public int ExitCode;
        public string StandardOutput;
        public string StandardError;

        public (string Stdout, string Stderr) Unwrap()
        {
            if (ExitCode != 0)
            {
                Console.WriteLine(StandardOutput);
                Console.Error.WriteLine(StandardError);
                throw new Exception("Task Failed!");
            }

            return (StandardOutput, StandardError);
        }
    }

    public class Reflect
    {
        public string ScanPath;
        public string ReflectIncludeDirectory;
        public string BuildDirectory;

        public ExecutionResult Run(BuildTask targetTask, BuildTask metaprogramTask)
        {
            Console.Write($"{ConsoleTags.Running} Running Reflect on {targetTask.ResultName}");
            var args = $"-m {Path.Combine(BuildDirectory, metaprogramTask.ResultName) + ".dll"} -p {metaprogramTask.DllExportedProc} -a -x c -DMETA_PASS -std={(targetTask.Language == Language.C ? "c11" : "c++11")} -I{ReflectIncludeDirectory} {GetIncludeDirsArgs(targetTask.IncludeDirectories.ToArray())} {GetDefinesArgs(targetTask.Defines.ToArray())} {GetSourcesArgs(targetTask.Sources.ToArray())}";
            Console.WriteLine($"Reflect args {args}");
            var result = Utils.ExecuteCommand(ScanPath, args);

            if (result.ExitCode != 0)
            {
                Console.WriteLine($"\r{ConsoleTags.Failed} Running Reflect on {targetTask.ResultName}");
            }
            else
            {
                Console.WriteLine($"\r{ConsoleTags.Done} Running Reflect on {targetTask.ResultName}");
            }

            return result;
        }

        private static string GetDefinesArgs(string[] defines)
        {
            defines ??= Array.Empty<string>();

            var builder = new StringBuilder();

            foreach (var define in defines)
            {
                builder.Append($"-D{define} ");
            }

            return builder.ToString().TrimEnd();
        }

        private static string GetIncludeDirsArgs(string[] dirs)
        {
            dirs ??= Array.Empty<string>();

            var builder = new StringBuilder();

            foreach (var dir in dirs)
            {
                builder.Append($"-I{dir} ");
            }

            return builder.ToString().TrimEnd();
        }

        private static string GetSourcesArgs(string[] sources)
        {
            sources ??= Array.Empty<string>();

            var builder = new StringBuilder();

            foreach (var source in sources)
            {
                builder.Append($"{source} ");
            }

            return builder.ToString().TrimEnd();
        }
    }

    public class Compiler
    {
        public BuildMode BuildMode;
        public string FlagKey = "/";
        public string ObjectFilesOutputDirectory = string.Empty;
        public string OutputDirectory = string.Empty;

        public ExecutionResult Compile(BuildTask task)
        {
            Console.Write($"{ConsoleTags.Running} Building {task.OutputType} {task.ResultName}");
            var optimizationFlags = GetOptimizationFlags();
            var compilerFlags = $"/MP /W3 /Gm- /GR- /fp:fast /EHsc /nologo /diagnostics:classic /WX {optimizationFlags} /std:{(task.Language == Language.C ? "c17" : "c++20")} /Zc:preprocessor /arch:AVX /Fo{ObjectFilesOutputDirectory}/";
            //var optimizationFlags = "/Zi /Od /MTd";
            var exportFlag = string.IsNullOrEmpty(task.DllExportedProc) ? string.Empty : $"/EXPORT:{task.DllExportedProc}";
            var linkerFlags = $"/INCREMENTAL:NO /OPT:REF /MACHINE:X64 {GetLinkerLibsArgs(task.ExternalLibs.ToArray())} {GetOutTypeFlag(task.OutputType)} {exportFlag} /OUT:{OutputDirectory}/{task.ResultName}.{GetResultExtension(task.OutputType)} /PDB:{OutputDirectory}/{task.ResultName}.pdb";
            var args = $"{compilerFlags} {GetDefinesArgs(task.Defines.ToArray())} {GetIncludeDirsArgs(task.IncludeDirectories.ToArray())} {GetSourcesArgs(task.Sources.ToArray())} /link {linkerFlags}";

            var result = Utils.ExecuteCommand("cl", args.Replace("/", FlagKey));

            if (result.ExitCode != 0)
            {
                Console.WriteLine($"\r{ConsoleTags.Failed} Building {task.OutputType} {task.ResultName}");
            }
            else
            {
                Console.WriteLine($"\r{ConsoleTags.Done} Building {task.OutputType} {task.ResultName}");
            }

            return result;
        }

        private string GetOptimizationFlags()
        {
            if (BuildMode == BuildMode.Debug)
            {
                return "/Zi /Od /MTd";
            }
            else if (BuildMode == BuildMode.Release)
            {
                return "/O2 /GL /MT /Zi";
            }

            throw new InvalidEnumArgumentException();
        }

        private string GetResultExtension(OutputType type)
        {
            return type switch
            {
                OutputType.Executable => "exe",
                OutputType.DLL => "dll",
                _ => throw new InvalidEnumArgumentException(),
            };
        }

        private string GetOutTypeFlag(OutputType type)
        {
            return type switch
            {
                OutputType.Executable => "",
                OutputType.DLL => "/DLL",
                _ => throw new InvalidEnumArgumentException(),
            };
        }

        private static string GetDefinesArgs(string[] defines)
        {
            defines ??= Array.Empty<string>();

            var builder = new StringBuilder();

            foreach (var define in defines)
            {
                builder.Append($"/D{define} ");
            }

            return builder.ToString().TrimEnd();
        }

        private static string GetIncludeDirsArgs(string[] dirs)
        {
            dirs ??= Array.Empty<string>();

            var builder = new StringBuilder();

            foreach (var dir in dirs)
            {
                builder.Append($"/I{dir} ");
            }

            return builder.ToString().TrimEnd();
        }

        private static string GetSourcesArgs(string[] sources)
        {
            sources ??= Array.Empty<string>();

            var builder = new StringBuilder();

            foreach (var source in sources)
            {
                builder.Append($"{source} ");
            }

            return builder.ToString().TrimEnd();
        }

        private static string GetLinkerLibsArgs(string[] sources)
        {
            sources ??= Array.Empty<string>();

            var builder = new StringBuilder();

            foreach (var source in sources)
            {
                builder.Append($"{source} ");
            }

            return builder.ToString().TrimEnd();
        }
    }

    public static class Utils
    {
        public static ExecutionResult ExecuteCommand(string command, string args)
        {
            Process p = new Process();
            p.StartInfo.UseShellExecute = false;
            //p.StartInfo.RedirectStandardOutput = true;
            //p.StartInfo.RedirectStandardError = true;
            p.StartInfo.FileName = command;
            p.StartInfo.Arguments = args;

            p.Start();
            p.WaitForExit();

            //var stdout = p.StandardOutput.ReadToEnd();
            //var stderr = p.StandardError.ReadToEnd();

            //p.WaitForExit();

            return new()
            {
                ExitCode = p.ExitCode,
                StandardOutput = string.Empty,
                StandardError = string.Empty
            };
        }
    }

    public class Timer : IDisposable
    {
        private Stopwatch stopwatch;
        private string template;

        public Timer(string template)
        {
            this.template = template;
            stopwatch = new Stopwatch();
            stopwatch.Start();
        }

        public void Dispose()
        {
            stopwatch.Stop();
            var ts = stopwatch.Elapsed;
            Console.WriteLine(template.Replace("{time}", $"{ts.Seconds}.{ts.Milliseconds}"));
        }
    }
}
