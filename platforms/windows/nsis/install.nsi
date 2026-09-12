; EasyPhotoBackupServer Installer Script

!define APP_NAME "EasyPhotoBackupServer"
!define EXEC_NAME "EasyPhotoBackupServer.exe"
!define TASK_NAME "EasyPhotoBackupServerTask"

!define PAIRING_APP "pairing_windows_cli.ps1"

!define RUN_REG_KEY "Software\Microsoft\Windows\CurrentVersion\Run"
!define RUN_WRAPPER "${APP_NAME}.bat"

!define UNINSTALL_REG_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}"

Name "${APP_NAME}"
OutFile "EasyPhotoBackup-Installer.exe"
InstallDir "$LOCALAPPDATA\${APP_NAME}"
RequestExecutionLevel user

Page directory
Page instfiles

UninstPage uninstConfirm
UninstPage instfiles

Section "Install"
  ; Check if a previous installation already exists
  IfFileExists "$INSTDIR\${EXEC_NAME}" 0 +4
    MessageBox MB_YESNO|MB_ICONQUESTION "A previous version of ${APP_NAME} is already installed. Do you want to uninstall and replace it?" IDYES proceed IDNO abortInst
    abortInst:
      Abort "Installation cancelled by user."
    proceed:

  CreateDirectory "$INSTDIR"
  SetOutPath "$INSTDIR"

  IfFileExists "$INSTDIR\${EXEC_NAME}" 0 +3
    nsExec::ExecToStack 'taskkill /f /im "${EXEC_NAME}"'
    Sleep 1000 ; short pause to ensure file handles are fully released

  ; copy the executable
  File /oname=${EXEC_NAME} "..\..\..\build\Release\ServerService.exe"
  File "..\..\..\ServerPairing\${PAIRING_APP}"

  WriteUninstaller "$INSTDIR\Uninstall.exe"

  ; Add application to "Installed Apps"
  WriteRegStr HKCU "${UNINSTALL_REG_KEY}" "DisplayName" "${APP_NAME}"
  WriteRegStr HKCU "${UNINSTALL_REG_KEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr HKCU "${UNINSTALL_REG_KEY}" "QuietUninstallString" '"$INSTDIR\Uninstall.exe" /S'
  WriteRegStr HKCU "${UNINSTALL_REG_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "${UNINSTALL_REG_KEY}" "DisplayIcon" "$INSTDIR\${EXEC_NAME}"
  WriteRegStr HKCU "${UNINSTALL_REG_KEY}" "Publisher" "${APP_NAME}"
  WriteRegDWORD HKCU "${UNINSTALL_REG_KEY}" "NoModify" 1
  WriteRegDWORD HKCU "${UNINSTALL_REG_KEY}" "NoRepair" 1

  ; schedule to be run on startup (use batch script to wrap the arguments)
  ClearErrors
  FileOpen $0 "$INSTDIR\${RUN_WRAPPER}" w
  IfErrors +4
  FileWrite $0 '@echo off$\r$\n'
  FileWrite $0 '"$INSTDIR\${EXEC_NAME}" --pairingApp "powershell.exe -ExecutionPolicy Bypass -File \"$INSTDIR\${PAIRING_APP}\""$\r$\n'
  FileClose $0
  WriteRegStr HKCU "${RUN_REG_KEY}" "${APP_NAME}" '"$INSTDIR\${RUN_WRAPPER}"'

  ; run the server
  nsExec::Exec 'powershell -WindowStyle Hidden -Command "Start-Process \"$INSTDIR\${RUN_WRAPPER}\" -WindowStyle Hidden"'
  ;nsExec::Exec 'powershell -WindowStyle Hidden -Command "Start-Process \"$INSTDIR\${EXEC_NAME}\" -ArgumentList \"--pairingApp `\"powershell.exe -ExecutionPolicy Bypass -File $INSTDIR\pairing_windows_cli.ps1`\"\" -WindowStyle Hidden"'
SectionEnd

Section "Uninstall"
  ; kill the running server
  nsExec::ExecToStack 'taskkill /f /im "${EXEC_NAME}"'

  ; remove the login task
  DeleteRegValue HKCU "${RUN_REG_KEY}" "${APP_NAME}"

  ; remove "Installed Apps" record
  DeleteRegKey HKCU "${UNINSTALL_REG_KEY}"

  ; delete the contents and the folder (including the uninstaller itself)
  Delete "$INSTDIR\*.*"
  RMDir /r "$INSTDIR"
SectionEnd
