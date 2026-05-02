#include "AEConfig.h"
#include "PiPL.r"

resource 'PiPL' (16000) {
  {
    Kind { AEEffect },
    Name { "Rimlight Toolkit" },
    Category { "Rimlight Toolkit" },
    CodeWin64X86 { "EffectMain" },
    AE_PiPL_Version { 2, 0 },
    AE_Effect_Spec_Version { PF_PLUG_IN_VERSION, PF_PLUG_IN_SUBVERS },
    AE_Effect_Version { 0x00010000 },
    AE_Effect_Info_Flags { 0 },
    AE_Effect_Global_OutFlags { PF_OutFlag_DEEP_COLOR_AWARE | PF_OutFlag_USE_OUTPUT_EXTENT },
    AE_Effect_Global_OutFlags_2 { PF_OutFlag2_SUPPORTS_THREADED_RENDERING },
    AE_Effect_Match_Name { "DGruwier Rimlight Toolkit" },
    AE_Reserved_Info { 0 }
  }
};
