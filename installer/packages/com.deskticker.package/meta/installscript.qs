function Component()
{
}

Component.prototype.createOperations = function()
{
    component.createOperations();

    if (systemInfo.productType === "windows") {
        component.addOperation(
            "CreateShortcut",
            "@TargetDir@/DeskTicker.exe",
            "@StartMenuDir@/DeskTicker.lnk",
            "workingDirectory=@TargetDir@",
            "iconPath=@TargetDir@/DeskTicker.exe",
            "description=Launch DeskTicker");
        component.addOperation(
            "CreateShortcut",
            "@TargetDir@/DeskTicker.exe",
            "@DesktopDir@/DeskTicker.lnk",
            "workingDirectory=@TargetDir@",
            "iconPath=@TargetDir@/DeskTicker.exe",
            "description=Launch DeskTicker");
        component.addOperation(
            "Execute",
            "@TargetDir@/DeskTicker.exe",
            "--sync-startup",
            "UNDOEXECUTE",
            "cmd.exe",
            "/c",
            "reg.exe delete HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run /v DeskTicker /f >nul 2>&1 & exit /b 0");
    }
}
