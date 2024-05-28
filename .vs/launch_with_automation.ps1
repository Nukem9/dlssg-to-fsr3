param ($Action, $VSProcessId, $ChildProcessId)

# See end of file

Add-Type -Language Csharp -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.ComTypes;

namespace VSAutomationHelper
{
    public class Main
    {
        [DllImport("ole32.dll")]
        private static extern int CreateBindCtx(uint reserved, out IBindCtx ppbc);

        public static object FindDTEInstanceByProcessId(int processId)
        {
            IBindCtx bindContext = null;
            IRunningObjectTable objectTable = null;
            IEnumMoniker enumMonikers = null;
     
            try
            {
                Marshal.ThrowExceptionForHR(CreateBindCtx(0, out bindContext));
                bindContext.GetRunningObjectTable(out objectTable);
                objectTable.EnumRunning(out enumMonikers);

                IMoniker[] moniker = new IMoniker[1];
                IntPtr numberFetched = IntPtr.Zero;

                while (enumMonikers.Next(1, moniker, numberFetched) == 0)
                {
                    var runningObjectMoniker = moniker[0];

                    if (runningObjectMoniker == null)
                        continue;

                    try
                    {
                        string name = null;
                        runningObjectMoniker.GetDisplayName(bindContext, null, out name);

                        if (name.StartsWith("!VisualStudio.DTE.")) // Consistent from 2010~2022
                        {
                            int idIndex = name.IndexOf(':');

                            if (idIndex != -1 && int.Parse(name.Substring(idIndex + 1)) == processId)
                            {
                                object runningObject = null;
                                Marshal.ThrowExceptionForHR(objectTable.GetObject(runningObjectMoniker, out runningObject));

                                return runningObject;
                            }
                        }
                    }
                    catch (UnauthorizedAccessException)
                    {
                        // Inaccessible due to permissions
                    }
                }
            }
            finally
            {
                if (enumMonikers != null)
                    Marshal.ReleaseComObject(enumMonikers);
     
                if (objectTable != null)
                    Marshal.ReleaseComObject(objectTable);
     
                if (bindContext != null)
                    Marshal.ReleaseComObject(bindContext);
            }

            return null;
        }
    }
}
"@

#
# .\script.ps1 Launch     Callback from Visual Studio to launch the game
# .\script.ps1 Attach     Callback from the game to attach Visual Studio's debugger
#
# GAME_DEBUGGER_CMDLINE   Env var holding a command line expression that launches the game
# GAME_DEBUGGER_REQUEST   Env var holding a command line expression that runs when the game is ready for debugger attach
# GAME_DEBUGGER_PROC      Env var holding kernel32.dll's CreateProcessA import function string to prevent AV false positives
#
if ($Action -eq 'Launch') {
    # Visual Studio doesn't necessarily launch the debugee. Keep chasing the parent PID until we find it.
    $parentVSProcessId = $PID

    while ($true) {
        $wmiProcess = Get-WmiObject Win32_Process -Filter ProcessId=$parentVSProcessId

        if ($wmiProcess.ProcessName.ToLower() -eq "devenv.exe") {
            break;
        }

        $parentVSProcessId = $wmiProcess.ParentProcessId
    }

    # Start process with special environment vars set
    $localPSPath = (Get-Process -Id $PID | Get-Item).FullName
    $localScriptPath = $MyInvocation.MyCommand.Path

    $env:GAME_DEBUGGER_REQUEST = '"' + $localPSPath + '" -ExecutionPolicy Bypass -File "' + $localScriptPath + '" Attach ' + $parentVSProcessId + ' '
    $env:GAME_DEBUGGER_PROC = 'kernel32!CreateProcessA'

    & $env:GAME_DEBUGGER_CMDLINE
}
elseif ($Action -eq 'Attach') {
    # Callback from game DLL side. Tell Visual Studio to attach to its process.
    $automationDTE = [VSAutomationHelper.Main]::FindDTEInstanceByProcessId($VSProcessId)

    foreach ($process in $automationDTE.Debugger.LocalProcesses) {
        if ($process.ProcessID -eq $ChildProcessId) {
            $process.Attach()
            break;
        }
    }
}