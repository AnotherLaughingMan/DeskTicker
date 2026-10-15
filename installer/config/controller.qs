function Controller()
{
    this.overwriteConfirmed = false;
}

Controller.prototype.IntroductionPageCallback = function()
{
    var page = gui.currentPageWidget();
    if (page !== null) {
        page.title = "Welcome to DeskTicker";
        page.MessageLabel.setText(
            "<p>DeskTicker is a lightweight desktop clock that displays the current time and date in a compact, movable widget.</p>"
            + "<p>After installation, launch it from Start. Right-click the widget to adjust its colors, transparency, text size, and always-on-top behavior. You can also choose to start DeskTicker with Windows.</p>");
        page.MessageLabel.wordWrap = true;
    }
}

Controller.prototype.TargetDirectoryPageCallback = function()
{
    if (this.overwriteConfirmed)
        return;

    var targetDirectory = installer.fromNativeSeparators(installer.value("TargetDir"));
    var componentsFile = targetDirectory + "/components.xml";
    var maintenanceTool = targetDirectory + "/maintenancetool.exe";
    if (!installer.fileExists(componentsFile) || !installer.fileExists(maintenanceTool))
        return;

    var packageCheck = installer.execute("findstr.exe",
        ["/I", "/L", "/C:com.deskticker.package", installer.toNativeSeparators(componentsFile)]);
    if (packageCheck.length < 2 || Number(packageCheck[1]) !== 0)
        return;

    var answer = QMessageBox.question(
        "deskticker.overwrite",
        "Existing DeskTicker installation",
        "An existing DeskTicker installation was found. Replace its installed files? "
            + "Your DeskTicker preferences will be preserved.",
        QMessageBox.Yes | QMessageBox.No,
        QMessageBox.No);
    if (answer !== QMessageBox.Yes)
        return;

    var runningProcesses = installer.execute("tasklist.exe",
        ["/FI", "IMAGENAME eq DeskTicker.exe", "/FO", "CSV", "/NH"]);
    if (runningProcesses.length < 2 || Number(runningProcesses[1]) !== 0) {
        QMessageBox.critical("deskticker.overwrite.process-check",
            "Could not check DeskTicker", "Close DeskTicker before replacing the installation.");
        gui.reject();
        return;
    }
    if (runningProcesses[0].toLowerCase().indexOf('"deskticker.exe"') !== -1) {
        QMessageBox.warning("deskticker.overwrite.running",
            "DeskTicker is running", "Close DeskTicker and start the installer again.");
        gui.reject();
        return;
    }

    var purgeResult = installer.execute(maintenanceTool, ["purge", "--confirm-command"]);
    if (purgeResult.length < 2 || Number(purgeResult[1]) !== 0
        || installer.fileExists(componentsFile)) {
        QMessageBox.critical("deskticker.overwrite.failed",
            "Could not replace DeskTicker",
            "The installer stopped before installing the new version. Reopen it and retry; "
                + "the existing installation may need repair if removal was interrupted.");
        gui.reject();
        return;
    }

    this.overwriteConfirmed = true;
}
