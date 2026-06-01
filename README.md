# SpeedrunSilentPatch

If you're not a speedrunner, you probably don't want this patch. Go here instead:
https://github.com/CookiePLMonster/SilentPatch

SpeedrunSilentPatch is a conservative, trimmed-down version of SilentPatch that only includes some basic quality-of-life fixes that (crucially) don't allow you to get a faster speedrun time.

That way, using this patch does not give speedrunners an unfair advantage and using it is completely optional.

## Featured fixes

### GTA3

* The mouse will no longer go beyond the game window dimensions, making it possible to play the game on multi-monitor setups without problems.
* DirectPlay dependency has been removed - this should improve compatibility with Windows 8 and newer.
* The "Cannot find enough available video memory" error showing on some computers has been resolved.
* Path to the User Files directory is now obtained using a dedicated API call rather than a legacy registry entry, future-proofing the games more.
* Mouse sensitivity is now properly saved - like in the 1.1 and Steam versions.
* Mission title and 'Mission Passed' texts now stay on screen for the same duration, regardless of screen resolution.
* Free resprays will not carry on a New Game now.
* Fixed ambulance and firetruck dispatch timers - they reset on New Game now.
* Timers reset on a New Game now.
* Lines read in CPlane::LoadPath and CTrain::ReadAndInterpretTrackFile are now null-terminated, fixing issues with plane/train paths under specific conditions in a modded game.
* If the settings file is absent, the game will now default to your desktop resolution instead of 640x480x16.
* All censorships from German and French versions of the game have been removed.

### GTAVC

* Fixed an issue where installing the game on A: or B: drive made the game ask for the CD.
* The mouse should not lock up randomly when exiting the menu on newer systems anymore.
* The mouse will no longer go beyond the game window dimensions, making it possible to play the game on multi-monitor setups without problems.
* DirectPlay dependency has been removed - this should improve compatibility with Windows 8 and newer.
* The game will not crash on startup if Data Execution Prevention is enabled for all applications anymore.
* The "Cannot find enough available video memory" error showing on some computers has been resolved.
* Path to the User Files directory is now obtained using a dedicated API call rather than a legacy registry entry, future-proofing the games more.
* The mouse vertical axis sensitivity now matches horizontal axis sensitivity.
* Mission title and 'Mission Passed' texts now stay on screen for the same duration, regardless of screen resolution.
* Free resprays will not carry on a New Game now.
* Fixed ambulance and firetruck dispatch timers - they reset on New Game now.
* Lines read in CPlane::LoadPath and CTrain::ReadAndInterpretTrackFile are now null-terminated, fixing issues with plane/yacht paths under specific conditions in a modded game.
* Mouse sensitivity is no longer reset on starting a New Game.
* Fixed the "Greetings from Vice City" outro splash displaying longer than intended - now displays for 2.5 seconds.
* If the settings file is absent, the game will now default to your desktop resolution instead of 640x480x16.
* All censorships from German and French versions of the game have been removed.

## Thanks / Credits
* Silent, for creating SilentPatch, the project this project is based on
* EnglishBen, for the curation of which fixes to enable for speedrunners
* Grovespaz, for making the code changes required

# Original SilentPatch ReadMe:

<p align="center">
  <img src="https://i.imgur.com/sCDzq12.png" alt="Logo">
</p>

SilentPatch for the 3D-era Grand Theft Auto games is the first and flagship release of the "SilentPatch family", providing numerous fixes for this beloved franchise.
SilentPatch addresses a wide range of issues, from critical fixes for crashes and other blockers to various major and minor improvements identified by
the passionate community in these games over decades. SilentPatch does not alter the core gameplay experience, making it an optimal choice
for both first-time players and the old guard returning for yet another playthrough.

## Featured fixes

* [Fixes in GTA III](CHANGELOG-III.md)
* [Fixes in GTA Vice City](CHANGELOG-VC.md)
* [Fixes in GTA San Andreas](CHANGELOG-SA.md)

## Compilation requirements

* Visual Studio 2017 or newer with `C++ Windows XP Support for VS 2017 (v141) tools` installed. Newer toolsets will work too, but the projects will require retargeting.
* [vcpkg](https://vcpkg.io/) installed separately or as a Visual Studio component. Necessary for SP for San Andreas to include `libflac`.
* RenderWare Graphics SDK. Each game requires their corresponding RenderWare version and an environment variable pointing at the `RW3.x\Graphics\rwsdk` directory:
  * GTA III: RW 3.3, D3D8, `RWG33SDK` variable.
  * GTA Vice City: RW 3.4, D3D8, `RWG34SDK` variable.
  * GTA San Andreas: RW 3.6, D3D9, `RWG36SDK` variable.

## Contribution guidelines

* Contributions with bug fixes are welcome, but you must be able to explain why you believe they fix a bug, rather than alter a design decision that doesn't suit you.
  I reserve the right to reject submissions that cannot be unambiguously classified as fixes.
* Contributions for GTA III and Vice City must use patterns and support all game versions. For GTA San Andreas, contributions must support version 1.0, but preferably
  also the new binaries (newsteam/RGL).
* This repository is not intended for game support. Issues such as "I installed mods and the game now crashes" will be closed.

## Credits

SilentPatch includes code contributions from:
* aap
* B1ack_Wh1te
* DK22Pac
* Fire_Head
* iFarbod
* Kaizo M
* Nick007J
* NTAuthority
* Sergeanur
* spaceeinstein
* Wesser
