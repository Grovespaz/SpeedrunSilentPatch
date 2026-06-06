#pragma once

#define SILENTPATCHVC_FEATURE_DEFAULT 0

#define ENABLE_SUPPORT_DELAYED_PATCHING 1

/*** OG SP fixes ***/
// Fixed an issue where installing the game on `A:` or `B:` drive made the game ask for the CD.
#define ENABLE_FIX_AB_DRIVE_CD_CHECK 1
// The mouse should not lock up randomly when exiting the menu on newer systems anymore.
#define ENABLE_FIX_MOUSE_MENU_LOCKUP 1
// The mouse will no longer go beyond the game window dimensions, making it possible to play the game on multi-monitor setups without problems.
#define ENABLE_FIX_MOUSE_WINDOW_CONFINEMENT 1
// Use ClipCursor instead of per-frame mouse recentering, avoiding synthetic mouse deltas with non-exclusive DirectInput.
#define ENABLE_FIX_MOUSE_WINDOW_CONFINEMENT_CLIPCURSOR 1
// DirectPlay dependency has been removed - this should improve compatibility with Windows 8 and newer.
#define ENABLE_FIX_NO_DIRECTPLAY 1
// The game will not crash on startup if Data Execution Prevention is enabled for all applications anymore.
#define ENABLE_FIX_DEP_STARTUP_CRASH 1
// The "<samp>Cannot find enough available video memory</samp>" error showing on some computers has been resolved.
#define ENABLE_FIX_FAKE_VRAM_POLL 1
// Path to the User Files directory is now obtained using a dedicated API call rather than a legacy registry entry, future-proofing the games more.
#define ENABLE_FIX_USER_FILES_PATH 1
// The mouse vertical axis sensitivity now matches horizontal axis sensitivity.
#define ENABLE_FIX_MOUSE_VERTICAL_SENSITIVITY 1
// Mission title and 'Mission Passed' texts now stay on screen for the same duration, regardless of screen resolution.
#define ENABLE_FIX_MISSION_TEXT_DURATION 1
// Free resprays will not carry on a New Game now.
#define ENABLE_FIX_FREE_RESPRAYS_NEW_GAME 1
// Fixed ambulance and firetruck dispatch timers - they reset on New Game now.
#define ENABLE_FIX_EMERGENCY_DISPATCH_TIMERS_NEW_GAME 1
// Lines read in `CPlane::LoadPath` and `CTrain::ReadAndInterpretTrackFile` are now null-terminated, fixing issues with plane/yacht paths under specific conditions in a modded game.
#define ENABLE_FIX_NULL_TERMINATED_PATH_LINES 1
// Mouse sensitivity is no longer reset on starting a New Game.
#define ENABLE_FIX_MOUSE_SENSITIVITY_NEW_GAME 1
// Fixed the "Greetings from Vice City" outro splash displaying longer than intended - now displays for 2.5 seconds.
#define ENABLE_FIX_OUTRO_SPLASH_DURATION 1
// If the settings file is absent, the game will now default to your desktop resolution instead of 640x480x16.
#define ENABLE_ENHANCEMENT_DEFAULT_DESKTOP_RESOLUTION 1
// All censorships from German and French versions of the game have been removed.
#define ENABLE_ENHANCEMENT_NO_CENSORSHIP 1

/*** SpeedrunSilentPatch exclusive fixes ***/
// Fixed an issue where VC JP asked for a CD when running on systems without a CD-ROM drive.
#define ENABLE_FIX_JP_NO_CDROM_DRIVE_CHECK 1