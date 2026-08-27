SpeedrunSilentPatch 1.1 for GTA III
Beta build 8, based on SP build 9
Last update - 26.08.2026


DESCRIPTION

	If you're not a speedrunner, you probably don't want this patch. Go here instead:
	https://github.com/CookiePLMonster/SilentPatch

	SpeedrunSilentPatch is a conservative version of SilentPatch that only includes some basic
	quality-of-life fixes that (crucially) don't allow you to get a faster speedrun time.

	That way, using this patch does not give speedrunners an unfair advantage and using it is
	completely optional.
	
INCLUDED FIXES

	Original SilentPatch fixes included in SpeedrunSilentPatch:
	- The mouse will no longer go beyond the game window dimensions, making it possible to play the game
	  on multi-monitor setups without problems.
	- DirectPlay dependency has been removed - this should improve compatibility with Windows 8 and newer.
	- The "Cannot find enough available video memory" error showing on some computers has been resolved.
	- Path to the User Files directory is now obtained using a dedicated API call rather than a
 	  legacy registry entry, future-proofing the games more.
	- Mouse sensitivity is now properly saved - like in the 1.1 and Steam versions.
	- Mission title and 'Mission Passed' texts now stay on screen for the same duration, regardless of
	  screen resolution.
	- Free resprays will not carry on a New Game now.
	- Fixed ambulance and firetruck dispatch timers - they reset on New Game now.
	- Timers reset on a New Game now.
	- Lines read in CPlane::LoadPath and CTrain::ReadAndInterpretTrackFile are now null-terminated, fixing
	  issues with plane/train paths under specific conditions in a modded game.
	- If the settings file is absent, the game will now default to your desktop resolution instead of 640x480x16.
	- All censorships from German and French versions of the game have been removed.

	SpeedrunSilentPatch-only fixes:
	- Windowed mode is available, inspired by III.VC.SA.WindowedMode.
	- Repair Alt+F4 functionality.
	


INSTALLATION

	Easy as pie. Extract the archive contents to your GTA III directory (so SpeedrunSilentPatchIII.asi
	ends up	in the main game directory) and that's all. Patches will take effect automatically.
	Make sure you check the INI file!


CONFIGURATION

	SpeedrunSilentPatchIII.ini is included as an example configuration file. Keep it next to
	SpeedrunSilentPatchIII.asi in the main game directory.

	Open SpeedrunSilentPatchIII.ini in a text editor and edit the values under [SilentPatch].
	Use 1 to enable an option and 0 to disable it.

	Available settings:
	* WindowedMode - runs the game in a desktop window or borderless instead of fullscreen.
	  The following values are supported:
		`Off` = Full screen (like vanilla)
		`Framed` = Bordered window
		`Borderless` = Borderless
	  See below for more advanced usages of windowed mode.
	* AlwaysOnTop - Keeps the game on top of all other windows when running in windowed mode.

WINDOWED MODE

	SSP stores the last exact window position in `Documents\GTA3 User Files\SpeedrunSilentPatchIII.WindowedMode.ini`.
	If you want to precisely adjust the position you want the game to be on startup, you can edit the `Left` and `Top` values in that file.

	If you run in Borderless but want to change the position while the game is running, pressing Alt+F will toggle it into Bordered (with a frame) so you can drag the window around.
	When the window is where you want it yo be, press Alt+F again to toggle back into Borderless.

SUPPORTED GAME VERSIONS

	* GTA III 1.0 (all versions)
	* GTA III 1.1 (all versions, including Steam and Rockstar Games Launcher versions)


CREDITS

	Original SilentPatch includes code contributions from:

	Silent (obviously)
	aap
	B1ack_Wh1te
	DK22Pac
	Fire_Head
	Nick007J
	NTAuthority
	Sergeanur
	spaceeinstein
	Wesser

	SpeedrunSilentPatch adapted from SilentPatch by:
	Grovespaz (code)
	EnglishBen (fix curation)

	Windowed mode loosely based on III.VC.SA.WindowedMode (https://github.com/ThirteenAG/III.VC.SA.WindowedMode), with contributions from:
	maxorator
	ThirteenAG
	not6
	Miran


SILENT'S PATREON & GITHUB SPONSORS SUPPORTERS

	BuckoA51
	Calinou
	ddm
	deSSy2724
	Iman Shahani
	Jack
	kiwidog
	Lagahan
	m0b
	mirrorsedger
	MrNicolaZenk
	mrtaufner
	nta
	Phoenyx12
	retrozone.co
	SirJarko
	switchblade
	TJGM
	Vetle Ledaal


SILENT'S SPECIAL THANKS

	People who helped identify issues, tested the patch or were generally supportive:

	Ash_735
	Blackbird88
	mirh
	ThirteenAG
