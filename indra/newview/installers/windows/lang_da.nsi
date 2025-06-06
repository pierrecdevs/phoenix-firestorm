; First is default
LoadLanguageFile "${NSISDIR}\Contrib\Language files\Danish.nlf"

; Language selection dialog
LangString InstallerLanguageTitle  ${LANG_DANISH} "Installationssprog"
LangString SelectInstallerLanguage  ${LANG_DANISH} "Vælg venligst sprog til installation"

; subtitle on license text caption
LangString LicenseSubTitleUpdate ${LANG_DANISH} " Opdater"
LangString LicenseSubTitleSetup ${LANG_DANISH} " Opsætning"

LangString MULTIUSER_TEXT_INSTALLMODE_TITLE ${LANG_DANISH} "Installation Mode"
LangString MULTIUSER_TEXT_INSTALLMODE_SUBTITLE ${LANG_DANISH} "Install for all users (requires Admin) or only for the current user?"
LangString MULTIUSER_INNERTEXT_INSTALLMODE_TOP ${LANG_DANISH} "When you run this installer with Admin privileges, you can choose whether to install in (e.g.) c:\Program Files or the current user's AppData\Local folder."
LangString MULTIUSER_INNERTEXT_INSTALLMODE_ALLUSERS ${LANG_DANISH} "Install for all users"
LangString MULTIUSER_INNERTEXT_INSTALLMODE_CURRENTUSER ${LANG_DANISH} "Install for current user only"

; installation directory text
LangString DirectoryChooseTitle ${LANG_DANISH} "Installationsmappe" 
LangString DirectoryChooseUpdate ${LANG_DANISH} "Vælg Second Life mappe til opdatering til version ${VERSION_LONG}.(XXX):"
LangString DirectoryChooseSetup ${LANG_DANISH} "Vælg mappe hvor Firestorm skal installeres:"

LangString MUI_TEXT_DIRECTORY_TITLE ${LANG_DANISH} "Installation Directory"
LangString MUI_TEXT_DIRECTORY_SUBTITLE ${LANG_DANISH} "Select the directory into which to install Firestorm:"

LangString MUI_TEXT_INSTALLING_TITLE ${LANG_DANISH} "Installing Firestorm..."
LangString MUI_TEXT_INSTALLING_SUBTITLE ${LANG_DANISH} "Installing the Firestorm viewer to $INSTDIR"

LangString MUI_TEXT_FINISH_TITLE ${LANG_DANISH} "Installing Second Life"
LangString MUI_TEXT_FINISH_SUBTITLE ${LANG_DANISH} "Installed the Firestorm viewer to $INSTDIR."

LangString MUI_TEXT_ABORT_TITLE ${LANG_DANISH} "Installation Aborted"
LangString MUI_TEXT_ABORT_SUBTITLE ${LANG_DANISH} "Not installing the Firestorm viewer to $INSTDIR."

; CheckStartupParams message box
LangString CheckStartupParamsMB ${LANG_DANISH} "Kunne ikke finde programmet '$INSTNAME'. Baggrundsopdatering fejlede."

; installation success dialog
LangString InstSuccesssQuestion ${LANG_DANISH} "Start Firestorm nu?"

; remove old NSIS version
LangString RemoveOldNSISVersion ${LANG_DANISH} "Checker ældre version..."

; check windows version
LangString CheckWindowsVersionDP ${LANG_DANISH} "Checker Windows version..."
LangString CheckWindowsVersionMB ${LANG_DANISH} 'Firestorm supporterer kun Windows Vista.$\n$\nForsøg på installation på Windows $R0 kan resultere i nedbrud og datatab.$\n$\n'
LangString CheckWindowsServPackMB ${LANG_DANISH} "It is recomended to run Firestorm on the latest service pack for your operating system.$\nThis will help with performance and stability of the program."
LangString UseLatestServPackDP ${LANG_DANISH} "Please use Windows Update to install the latest Service Pack."

; checkifadministrator function (install)
LangString CheckAdministratorInstDP ${LANG_DANISH} "Checker rettigheder til installation..."
LangString CheckAdministratorInstMB ${LANG_DANISH} 'Det ser ud til at du benytter en konto med begrænsninger.$\nDu skal have "administrator" rettigheder for at installere Firestorm.'

; checkifadministrator function (uninstall)
LangString CheckAdministratorUnInstDP ${LANG_DANISH} "Checker rettigheder til at afinstallere..."
LangString CheckAdministratorUnInstMB ${LANG_DANISH} 'Det ser ud til at du benytter en konto med begrænsninger.$\nDu skal have "administrator" rettigheder for at afinstallere Firestorm.'

; checkifalreadycurrent
LangString CheckIfCurrentMB ${LANG_DANISH} "Det ser ud til at Firestorm ${VERSION_LONG} allerede er installeret.$\n$\nØnsker du at installere igen?"

; checkcpuflags
LangString MissingSSE2 ${LANG_DANISH} "This machine may not have a CPU with SSE2 support, which is required to run Firestorm ${VERSION_LONG}. Do you want to continue?"

; Extended cpu checks (AVX2)
LangString MissingAVX2 ${LANG_DANISH} "Your CPU does not support AVX2 instructions. Please download the legacy CPU version from %DLURL%-legacy-cpus/"
LangString AVX2Available ${LANG_DANISH} "Your CPU supports AVX2 instructions. You can download the AVX2 optimized version for better performance from %DLURL%/. Do you want to download it now?"
LangString AVX2OverrideConfirmation ${LANG_DANISH} "If you believe that your PC can support AVX2 optimization, you may install anyway. Do you want to proceed?"
LangString AVX2OverrideNote ${LANG_DANISH} "By overriding the installer, you are about to install a version that might crash immediately after launching. If this happens, please immediately install the standard CPU version."

; closesecondlife function (install)
LangString CloseSecondLifeInstDP ${LANG_DANISH} "Venter på at Second Life skal lukke ned..."
LangString CloseSecondLifeInstMB ${LANG_DANISH} "Firestorm kan ikke installeres mens programmet kører.$\n$\nAfslut programmet for at fortsætte.$\nVælg ANNULÉR for at afbryde installation."
LangString CloseSecondLifeInstRM ${LANG_DANISH} "Firestorm failed to remove some files from a previous install."

; closesecondlife function (uninstall)
LangString CloseSecondLifeUnInstDP ${LANG_DANISH} "Venter på at Firestorm skal lukke ned..."
LangString CloseSecondLifeUnInstMB ${LANG_DANISH} "Firestorm kan ikke afinstalleres mens programmet kører.$\n$\nAfslut programmet for at fortsætte.$\nVælg ANNULÉR for at afbryde installation."

; CheckNetworkConnection
LangString CheckNetworkConnectionDP ${LANG_DANISH} "Checker netværksforbindelse..."

; error during installation
LangString ErrorSecondLifeInstallRetry ${LANG_DANISH} "Firestorm installer encountered issues during installation. Some files may not have been copied correctly."
LangString ErrorSecondLifeInstallSupport ${LANG_DANISH} "Please reinstall viewer from https://www.firestormviewer.org/downloads/ and contact https://www.firestormviewer.org/support/ if issue persists after reinstall."

; ask to remove user's data files
LangString RemoveDataFilesMB ${LANG_DANISH} "Do you want to REMOVE all other files related to Firestorm as well?$\n$\nIt is recomended to keep the settings and cache files if you have other versions of Second Life installed or are uninstalling to upgrade to a newer version."

; delete program files
LangString DeleteProgramFilesMB ${LANG_DANISH} "Der er stadig filer i Firestorm program mappen.$\n$\nDette er sandsynligvis filer du har oprettet eller flyttet til :$\n$INSTDIR$\n$\nØnsker du at fjerne disse filer?"

; <FS:Ansariel> Ask to create protocol handler registry entries
LangString CreateUrlRegistryEntries ${LANG_DANISH} "Do you want to register Firestorm as default handler for virtual world protocols?$\n$\nIf you have other versions of Firestorm installed, this will overwrite the existing registry keys."

; <FS:Ansariel> Optional start menu entry
LangString CreateStartMenuEntry ${LANG_DANISH} "Create an entry in the start menu?"

; <FS:Ansariel> Application name suffix for OpenSim variant
LangString ForOpenSimSuffix ${LANG_DANISH} "for OpenSimulator"

; uninstall text
LangString UninstallTextMsg ${LANG_DANISH} "Dette vil afinstallere Firestorm ${VERSION_LONG} fra dit system."

; ask to remove registry keys that still might be needed by other viewers that are installed
LangString DeleteRegistryKeysMB ${LANG_DANISH} "Do you want to remove application registry keys?$\n$\nIt is recomended to keep registry keys if you have other versions of Firestorm installed."
