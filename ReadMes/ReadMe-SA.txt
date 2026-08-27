SpeedrunSilentPatch 1.1 for GTA SA
Beta build 8, based on SP Build 33
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
	- The mouse should not lock up randomly when exiting the menu on newer systems anymore.
	- DirectPlay dependency has been removed - this should improve compatibility with Windows 8 and newer.
	- Path to the User Files directory is now obtained using a dedicated API call rather than a legacy registry entry, future-proofing the games more.
	- Fixed a crash on car explosions - most likely to happen when playing with a multi-monitor setup.
	- Fixed a streaming-related deadlock, which could occasionally result in the game being stuck on a black screen when entering or exiting interiors
	  (this is the issue people used to fix by setting CPU affinity to one core).
	- Fixed Skimmer not spawning on Windows 11 24H2.
	- The mouse cursor is now locked to the game window instead of getting re-centered constantly, so rapid mouse
	  movements can no longer cause the cursor to leave the window in multi-monitor setups.
	- The mouse's vertical axis sensitivity now matches the horizontal axis sensitivity.
	- Num5 is now bindable (like in the 1.01 patch).
	- Dancing minigame timings have been improved, now they do not lose accuracy over time depending on the PC's uptime.
	- "Keep weapons after wasted" and "keep weapons after busted" are now reset on the New Game.
	- Pickups, car generators, and stunt jumps spawned through the text IPL files now reinitialize on a New Game.
	  Most notably, this fixes several pickups (like fire extinguishers) going missing after starting a new game.
	- Mission title and 'Mission Passed' texts now stay on screen for the same duration, regardless of screen resolution.
	- 16:9 resolutions are now selectable (like in the 1.01 patch).
	- Free resprays will not carry on a New Game now.
	- Fixed ambulance and firetruck dispatch timers - they reset on New Game now.
	- Several stat counters now reset on New Game - so the player will not level up quicker after starting New Game from a save.
	- The "To stop Carl..." message now resets properly on New Game.
	- If the settings file is absent, the game will now default to your desktop resolution instead of 800x600x32.
	- Remade the monitor selection dialog, adding several quality-of-life improvements -
	  such as remembering the selected screen, modern styling, and an option to skip the dialog appearing on every game launch.
	- EAX/NVIDIA splashes are now removed.
	- A 1.0 no-DVD-only bug where recruiting gang members would stop working after activating a replay has been fixed.

	SpeedrunSilentPatch-only fixes:
	- WASD cheat prevention is available. By default, cheats only work while Shift is held.
	- Windowed mode & Borderless is available, inspired by III.VC.SA.WindowedMode.
	- Copyrighted ambience music in the casinos, Pleasure Domes, Stadium, Lowrider challenge, beach party and the strip club
	  can be replaced with default San Andreas ambience to help prevent DMCA strikes on video and streaming platforms.
	  Disabled by default, toggleable with setting in .ini file.
	- CPU affinity setting through ini. This is no longer needed since the leaderboard mods have decided to allow the root-cause fix
	  (streaming deadlock described above).

INSTALLATION

	Easy as pie. Extract the archive contents to your GTA SA directory, overwriting vorbisFile.dll
	(so SpeedrunSilentPatchSA.asi, vorbisFile.dll, and vorbisHooked.dll end up in the main game
	directory) and that's all. Patches will take effect automatically.
	Make sure you check the INI file!


CONFIGURATION

	SpeedrunSilentPatchSA.ini is included as an example configuration file. Keep it next to
	SpeedrunSilentPatchSA.asi in the main game directory.

	Open SpeedrunSilentPatchSA.ini in a text editor and edit the values under [SilentPatch].
	For numeric options, use 1 to enable an option and 0 to disable it unless the setting
	description says otherwise.

	Available settings:
	* WindowedMode - runs the game in a desktop window or borderless instead of fullscreen.
	  The following values are supported:
		`Off` = Full screen (like vanilla)
		`Framed` = Bordered window
		`Borderless` = Borderless
	  See below for more advanced usages of windowed mode.
	* AlwaysOnTop - Keeps the game on top of all other windows when running in windowed mode.
	* EnableCheats - controls cheat input.
		`Normal` = Vanilla behavior
		`Off` = Cheats are completely disabled
		`Shift` = Cheats can only be entered while holding shift. Releasing shift clears the input buffer.
		`CapsLock` = Cheats can only be entered while Caps Lock is on.
		`ScrollLock` = Cheats can only be entered while Scroll Lock is on.
	* CpuAffinityMask - Allows you to set SA's process affinity, pinning it to one core, for example.
	  This is no longer needed as SSP includes a fix for the deadlock that used to hang if you didn't use this. Usually set to `1` but takes any mask you want. `0` (default) disables.
	  Don't touch if you have no clue what this does.
	* SkipIntroSplashes - skips the EAX/NVIDIA intro splashes.
	* ReplaceDMCAMusic - replaces copyrighted ambience music (including "beat" minigames) with neutral ambience.
	

WINDOWED MODE

	SSP stores the last exact window position in `Documents\GTA San Andreas User Files\SpeedrunSilentPatchSA.WindowedMode.ini`.
	If you want to precisely adjust the position you want the game to be on startup, you can edit the `Left` and `Top` values in that file.

	If you run in Borderless but want to change the position while the game is running, pressing Alt+F will toggle it into Bordered (with a frame) so you can drag the window around.
	When the window is where you want it yo be, press Alt+F again to toggle back into Borderless.

SUPPORTED GAME VERSIONS

	* GTA SA 1.0 only!

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

	Windowed mode based on III.VC.SA.WindowedMode (https://github.com/ThirteenAG/III.VC.SA.WindowedMode), with contributions from:
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
	gamerzworld
	iFarbod
	Inadequate
	LonesomeRider
	mirh
	Reyks
	Tomasak
