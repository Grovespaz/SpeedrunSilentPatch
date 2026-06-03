# SpeedrunSilentPatch

### If you're not a speedrunner, you probably don't want this patch. Go here instead:
https://github.com/CookiePLMonster/SilentPatch

(WIP) This is where you will find the GTA Community's version of [SilentPatch](https://cookieplmonster.github.io/mods/gta/)! 

If you're not familiar with [SilentPatch](https://cookieplmonster.github.io/mods/gta/), basically it fixes a bunch of issues with the 3D GTA Trilogy. This is great for casual players, but it fixes too much to be viable for speedrunning. Some fixes make SP straight-up faster than vanilla, meaning everyone would need to use it to be competitive.

The idea of SpeedrunSilentPatch (SSP) is to create a fork of [SilentPatch](https://cookieplmonster.github.io/mods/gta/) with three main objectives:

- Keep the games playable on modern hardware
- Improving speedrun QoL (eg fixing NG+ stuff)
- Removing the need to use several other third party tools to fix issues (dxwnd, dinput, etc)

Importantly this mod will **never** give a speed advantage. This means it is completely optional for veterans, while at the same time lowering the barrier to entry for those new to our community.

# SpeedrunSilentPatch Proposed Fixes (WIP)

## GTA3

- The mouse will no longer go beyond the game window dimensions, making it possible to play the game on multi-monitor setups without problems.
- DirectPlay dependency has been removed - this should improve compatibility with Windows 8 and newer.
- The "Cannot find enough available video memory" error showing on some computers has been resolved.
- Path to the User Files directory is now obtained using a dedicated API call rather than a legacy registry entry, future-proofing the games more.
- Mouse sensitivity is now properly saved - like in the 1.1 and Steam versions.
- Mission title and 'Mission Passed' texts now stay on screen for the same duration, regardless of screen resolution.
- Free resprays will not carry on a New Game now.
- Fixed ambulance and firetruck dispatch timers - they reset on New Game now.
- Timers reset on a New Game now.
- Lines read in CPlane::LoadPath and CTrain::ReadAndInterpretTrackFile are now null-terminated, fixing issues with plane/train paths under specific conditions in a modded game.
- If the settings file is absent, the game will now default to your desktop resolution instead of 640x480x16.
- All censorships from German and French versions of the game have been removed.

## GTAVC

- Fixed an issue where installing the game on A: or B: drive made the game ask for the CD.
- The mouse should not lock up randomly when exiting the menu on newer systems anymore.
- The mouse will no longer go beyond the game window dimensions, making it possible to play the game on multi-monitor setups without problems.
- DirectPlay dependency has been removed - this should improve compatibility with Windows 8 and newer.
- The game will not crash on startup if Data Execution Prevention is enabled for all applications anymore.
- The "Cannot find enough available video memory" error showing on some computers has been resolved.
- Path to the User Files directory is now obtained using a dedicated API call rather than a legacy registry entry, future-proofing the games more.
- The mouse vertical axis sensitivity now matches horizontal axis sensitivity.
- Mission title and 'Mission Passed' texts now stay on screen for the same duration, regardless of screen resolution.
- Free resprays will not carry on a New Game now.
- Fixed ambulance and firetruck dispatch timers - they reset on New Game now.
- Lines read in CPlane::LoadPath and CTrain::ReadAndInterpretTrackFile are now null-terminated, fixing issues with plane/yacht paths under specific conditions in a modded game.
- Mouse sensitivity is no longer reset on starting a New Game.
- Fixed the "Greetings from Vice City" outro splash displaying longer than intended - now displays for 2.5 seconds.
- If the settings file is absent, the game will now default to your desktop resolution instead of 640x480x16.
- All censorships from German and French versions of the game have been removed.

## GTASA

- The mouse should not lock up randomly when exiting the menu on newer systems anymore.
- DirectPlay dependency has been removed - this should improve compatibility with Windows 8 and newer.
- Path to the User Files directory is now obtained using a dedicated API call rather than a legacy registry entry, future-proofing the games more.
- Fixed a crash on car explosions - most likely to happen when playing with a multi-monitor setup.
- Fixed a streaming-related deadlock, which could occasionally result in the game being stuck on a black screen when entering or exiting interiors (this is the issue people used to fix by setting CPU affinity to one core).
- Fixed Skimmer not spawning on Windows 11 24H2.
- The mouse's vertical axis sensitivity now matches the horizontal axis sensitivity.
- Num5 is now bindable (like in the 1.01 patch).
- Dancing minigame timings have been improved, now they do not lose accuracy over time depending on the PC's uptime.
- "Keep weapons after wasted" and "keep weapons after busted" are now reset on the New Game.
- Steam and RGL versions have proper aspect ratios now.
- Steam/RGL versions will now default Steer with Mouse option to disabled, like in 1.0/1.01.
- Pickups, car generators, and stunt jumps spawned through the text IPL files now reinitialize on a New Game. Most notably, this fixes several pickups (like fire extinguishers) going missing after starting a new game.
- Mission title and 'Mission Passed' texts now stay on screen for the same duration, regardless of screen resolution.
- 16:9 resolutions are now selectable (like in the 1.01 patch).
- Free resprays will not carry on a New Game now.
- Fixed ambulance and firetruck dispatch timers - they reset on New Game now.
- Several stat counters now reset on New Game - so the player will not level up quicker after starting New Game from a save.
- The "To stop Carl..." message now resets properly on New Game.
- If the settings file is absent, the game will now default to your desktop resolution instead of 800x600x32.
- Censorships from Steam and RGL versions for German players have been removed.
- Remade the monitor selection dialog, adding several quality-of-life improvements - such as remembering the selected screen, modern styling, and an option to skip the dialog appearing on every game launch.
- The Steam/RGL version of the game will no longer reject 1.0/1.01 saves (still, a compatible SCM is needed for the save to work).
- EAX/NVIDIA splashes are now removed.


## Nice to haves (would require additional scripting)

- Forced DMCA audio removal
- WASD cheats removal (from chaos mod?)
- Some sort of anticheat/antitamper
- Fix default options (steer with mouse etc)
- Fix WASD cheats

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
