#pragma once

#define DDRAW_FEATURE_DEFAULT 0

#define ENABLE_FIX_NO_DIRECTPLAY 1
#define ENABLE_FIX_DEP_STARTUP_CRASH 1
#define ENABLE_FIX_FAKE_VRAM_POLL 1
#define ENABLE_FIX_USER_FILES_PATH 1
#define ENABLE_FIX_IMG_NO_BUFFERING 1
#define ENABLE_FIX_UNNAMED_CDSTREAM_SEMAPHORE 1
// The game process can be forced to a configured CPU affinity, disabled by default in the INI.
#define ENABLE_FIX_CPU_AFFINITY 1
// Stabilize the leftover SecuROM helper and allow the no-CD executable to run without optical drives.
#define ENABLE_FIX_VC_JP_NO_CD_BOOTSTRAP 1
#define ENABLE_ENHANCEMENT_DEFAULT_DESKTOP_RESOLUTION 1
#define ENABLE_ENHANCEMENT_NO_CENSORSHIP 1
#define ENABLE_ENHANCEMENT_WINDOWED_MODE 1
#define ENABLE_ENHANCEMENT_SKIP_INTRO_SPLASHES 1
