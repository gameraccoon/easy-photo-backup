; EasyPhotoBackupServer Installer Script

!define APP_NAME "EasyPhotoBackupServer"
!define SERVICE_EXEC_NAME "EasyPhotoBackupServerService.exe"
!define CLI_EXEC_NAME "EasyPhotoBackupServerCli.exe"
!define CLI_SHORTCUT_NAME "EasyPhotoBackup Cli"

; TODO: ask user for a directory path
!define TARGET_DIR "$DOCUMENTS\EasyPhotoBackup\Backups"

!define RUN_REG_KEY "Software\Microsoft\Windows\CurrentVersion\Run"
!define RUN_WRAPPER "${APP_NAME}.ps1"

!define UNINSTALL_REG_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}"

Name "${APP_NAME}"
OutFile "EasyPhotoBackup-Installer.exe"
InstallDir "$LOCALAPPDATA\${APP_NAME}"
RequestExecutionLevel user

Page directory
Page components
Page instfiles

UninstPage uninstConfirm
UninstPage instfiles

Section "Core Components" sec_core
  SectionIn RO

  ; Check if a previous installation already exists
  IfFileExists "$INSTDIR\${SERVICE_EXEC_NAME}" 0 +4
    MessageBox MB_YESNO|MB_ICONQUESTION "A previous version of ${APP_NAME} is already installed. Do you want to uninstall and replace it?" IDYES proceed IDNO abortInst
    abortInst:
      Abort "Installation cancelled by user."
    proceed:

  CreateDirectory "$INSTDIR"
  SetOutPath "$INSTDIR"

  IfFileExists "$INSTDIR\${SERVICE_EXEC_NAME}" 0 +3
    nsExec::ExecToStack 'taskkill /f /im "${SERVICE_EXEC_NAME}"'
    Sleep 1000 ; short pause to ensure file handles are fully released

  ; copy the executables
  File /oname=${SERVICE_EXEC_NAME} "..\..\..\build\Release\ServerService.exe"
  File /oname=${CLI_EXEC_NAME} "..\..\..\build\Release\ServerCli.exe"

  WriteUninstaller "$INSTDIR\Uninstall.exe"

  ; Add application to "Installed Apps"
  WriteRegStr HKCU "${UNINSTALL_REG_KEY}" "DisplayName" "${APP_NAME}"
  WriteRegStr HKCU "${UNINSTALL_REG_KEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr HKCU "${UNINSTALL_REG_KEY}" "QuietUninstallString" '"$INSTDIR\Uninstall.exe" /S'
  WriteRegStr HKCU "${UNINSTALL_REG_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "${UNINSTALL_REG_KEY}" "DisplayIcon" "$INSTDIR\${CLI_EXEC_NAME}"
  WriteRegStr HKCU "${UNINSTALL_REG_KEY}" "Publisher" "${APP_NAME}"
  WriteRegDWORD HKCU "${UNINSTALL_REG_KEY}" "NoModify" 1
  WriteRegDWORD HKCU "${UNINSTALL_REG_KEY}" "NoRepair" 1

  ; create a wrapper file to hide launch arguments
  FileOpen $0 "$INSTDIR\${RUN_WRAPPER}" w
  FileWrite $0 "Start-Process -FilePath '$INSTDIR\${SERVICE_EXEC_NAME}' -ArgumentList '--workingDir', '$INSTDIR', '--targetDir', '${TARGET_DIR}' -WindowStyle Hidden$\r$\n"
  FileClose $0

  ; schedule to be run on startup
  WriteRegStr HKCU "${RUN_REG_KEY}" "${APP_NAME}" 'powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "$INSTDIR\${RUN_WRAPPER}"'

  ; run the server right away
  nsExec::Exec 'powershell.exe -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "$INSTDIR\${RUN_WRAPPER}"'
SectionEnd

Section "Start Menu Shortcut" sec_shortcut
  Delete "$SMPROGRAMS\${APP_NAME}\${CLI_SHORTCUT_NAME}.lnk"
  RMDir "$SMPROGRAMS\${APP_NAME}"

  CreateDirectory "$SMPROGRAMS\${APP_NAME}"
  CreateShortcut "$SMPROGRAMS\${APP_NAME}\${CLI_SHORTCUT_NAME}.lnk" "$INSTDIR\${CLI_EXEC_NAME}"
SectionEnd

Section "Uninstall" sec_uninstall
  ; kill the running server
  nsExec::ExecToStack 'taskkill /f /im "${SERVICE_EXEC_NAME}"'

  ; remove the login task
  DeleteRegValue HKCU "${RUN_REG_KEY}" "${APP_NAME}"

  ; remove "Installed Apps" record
  DeleteRegKey HKCU "${UNINSTALL_REG_KEY}"

  ; delete the contents and the folder (including the uninstaller itself)
  Delete "$INSTDIR\*.*"
  RMDir /r "$INSTDIR"

  ; delete the Start Menu shortcuts
  Delete "$SMPROGRAMS\${APP_NAME}\${CLI_SHORTCUT_NAME}.lnk"
  RMDir "$SMPROGRAMS\${APP_NAME}"
SectionEnd
