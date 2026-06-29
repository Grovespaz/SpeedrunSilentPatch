#pragma once

#define SILENTPATCHSA_FEATURE_DEFAULT 0

#define ENABLE_SUPPORT_DELAYED_PATCHING 1

// The mouse should not lock up randomly when exiting the menu on newer systems anymore.
#define ENABLE_FIX_MOUSE_MENU_LOCKUP 1
// The mouse will no longer go beyond the game window dimensions, making it possible to play the game on multi-monitor setups without problems.
#define ENABLE_FIX_MOUSE_WINDOW_CONFINEMENT 1
// DirectPlay dependency has been removed - this should improve compatibility with Windows 8 and newer.
#define ENABLE_FIX_NO_DIRECTPLAY 1
// Fixed a crash on car explosions - most likely to happen when playing with a multi-monitor setup.
#define ENABLE_FIX_CAR_EXPLOSION_CRASH 1
// Fixed a streaming-related deadlock, which could occasionally result in the game being stuck on a black screen when entering or exiting interiors (this is the issue people used to fix by setting CPU affinity to one core).
// This is disabled in favor of the CPU affinity fix, which is more in the spirit of SSP.
#define ENABLE_FIX_CDSTREAM_DEADLOCK 0
// The game process can now be forced to a configured CPU affinity, defaulting to one core.
#define ENABLE_FIX_CPU_AFFINITY 1
// Fixed Skimmer not spawning on Windows 11 24H2.
#define ENABLE_FIX_SKIMMER_WINDOWS_11_24H2 1
// The mouse's vertical axis sensitivity now matches the horizontal axis sensitivity.
#define ENABLE_FIX_MOUSE_VERTICAL_SENSITIVITY 1
// "Keep weapons after wasted" and "keep weapons after busted" are now reset on the New Game.
#define ENABLE_FIX_KEEP_WEAPONS_NEW_GAME 1
// Pickups, car generators, and stunt jumps spawned through the text IPL files now reinitialize on a New Game. Most notably, this fixes several pickups (like fire extinguishers) going missing after starting a new game.
#define ENABLE_FIX_IPL_SPAWNS_NEW_GAME 1
// Mission title and 'Mission Passed' texts now stay on screen for the same duration, regardless of screen resolution.
#define ENABLE_FIX_MISSION_TEXT_DURATION 1
// Free resprays will not carry on a New Game now.
#define ENABLE_FIX_FREE_RESPRAYS_NEW_GAME 1
// Fixed ambulance and firetruck dispatch timers - they reset on New Game now.
#define ENABLE_FIX_EMERGENCY_DISPATCH_TIMERS_NEW_GAME 1
// Several stat counters now reset on New Game - so the player will not level up quicker after starting New Game from a save.
#define ENABLE_FIX_STAT_COUNTERS_NEW_GAME 1
// The "To stop Carl..." message now resets properly on New Game.
#define ENABLE_FIX_STOP_CARL_MESSAGE_NEW_GAME 1
// Cheats can now be disabled or gated behind Shift/CapsLock/ScrollLock to avoid accidental WASD cheat collisions.
#define ENABLE_FIX_ACCIDENTAL_CHEATS 1
// If the settings file is absent, the game will now default to your desktop resolution instead of 800x600x32.
#define ENABLE_ENHANCEMENT_DEFAULT_DESKTOP_RESOLUTION 1
// Remade the monitor selection dialog, adding several quality-of-life improvements - such as remembering the selected screen, modern styling, and an option to skip the dialog appearing on every game launch.
#define ENABLE_ENHANCEMENT_MONITOR_SELECTION_DIALOG 1
// EAX/NVIDIA splashes are now removed.
#define ENABLE_ENHANCEMENT_SKIP_INTRO_SPLASHES 1
// Allows the game to run in a normal desktop window instead of exclusive fullscreen.
#define ENABLE_ENHANCEMENT_WINDOWED_MODE 1
// Replaces DMCA-sensitive ambience tracks with neutral ambience.
#define ENABLE_ENHANCEMENT_REPLACE_DMCA_AMBIENCE 1

/*** 1.0 -> 1.01 patches: ***/
// Fixed a crash when entering advanced display options on a dual monitor machine after: starting the game on the primary monitor in maximum resolution, exiting, starting again in maximum resolution on the secondary monitor. The secondary monitor's maximum resolution had to be greater than the maximum resolution of the primary monitor (like in the 1.01 patch).
#define ENABLE_FIX_ADVANCED_DISPLAY_DUAL_MONITOR_CRASH 1
// Fixed a crash when entering Advanced Display Settings with 32MB VRAM (like in the 1.01 patch).
#define ENABLE_FIX_ADVANCED_DISPLAY_32MB_VRAM_CRASH 1
// <kbd>Num5</kbd> is now bindable (like in the 1.01 patch).
#define ENABLE_FIX_NUM5_BINDABLE 1
// `FILE_FLAG_NO_BUFFERING` flag has been removed from IMG reading functions - speeding up streaming.
#define ENABLE_FIX_IMG_NO_BUFFERING 1
// 16:9 resolutions are now selectable (like in the 1.01 patch).
#define ENABLE_FIX_16_9_RESOLUTIONS 1
// Blown-up vehicles are now correctly colored and no longer shine (like in the 1.01 and Steam versions).
#define ENABLE_FIX_BLOWN_UP_VEHICLE_RENDERING 1
// Dancing minigame timings have been improved, now they do not lose accuracy over time depending on the PC's uptime.
#define ENABLE_FIX_DANCING_TIMINGS 1

/*** Supersedes an 1.01 fix: ***/
// Path to the User Files directory is now obtained using a dedicated API call rather than a legacy registry entry, future-proofing the games more.
#define ENABLE_FIX_USER_FILES_PATH 1
// User radio files should no longer crash while Frame Limiter is disabled. Changelog match is not confirmed; this enables SilentPatch's User Tracks fix.
#define ENABLE_FIX_USER_TRACKS_CRASH 1

/*** Only for broken Hoodlum executables: ***/
// A 1.0 no-DVD-only bug where recruiting gang members would stop working after activating a replay has been fixed (contributed by **Wesser**).
#define ENABLE_FIX_HOODLUM_RECRUITING_REPLAY 1

/*** Likely 1.0 -> 1.01 patches: ***/
// Dirty cars are now able to get clean (like in the 1.01 patch).
#define ENABLE_FIX_DIRTY_CARS 1


/*
Still unmatched / not ported:

Rain / Thunderstorm audio-system crash.
Reverb not present at certain save points.
*/
