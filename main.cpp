#include <mod/amlmod.h>
#include <mod/logger.h>
#include <mod/config.h>

// SAUtils
#include "isautils.h"
ISAUtils* sautils = NULL;

// Macros!
#define sizeofA(__aVar)     ( (int)(sizeof(__aVar)/sizeof(__aVar[0])) )
#define TO_RAD(__an)        ( (float)(__an) * (float)M_PI / 180.0f )
#define TO_DEG(__an)        ( (float)(__an) * 180.0f / M_PI)

// Enums
enum eRadarShape : uint8_t
{
    SHAPE_CIRCLE = 0,
    SHAPE_RECT,

    MAX_SHAPES
};
struct GtaVec2d
{
    float x, y;
};

// Settings
#define SETID_OUTLINE   ((void*)0)
#define SETID_SHAPE     ((void*)1)
#define SETID_RECTX     ((void*)2)
#define SETID_RECTY     ((void*)3)
const char* aYesNo[2] = 
{
    "FEM_OFF",
    "FEM_ON",
};
const char* aShapes[MAX_SHAPES] = 
{
    "Circle",
    "Rectangle",
};

// Vars
uintptr_t pGame;
void *hGame;
Config* cfg = NULL;

eRadarShape nRadarShape = SHAPE_CIRCLE;
uintptr_t pMaskBackTo, pMaskContinueBackTo, pLRPBackTo1, pLRPBackTo2, pLRPBackTo3, pLRPBackTo4, pLRPBackTo5;

ConfigEntry *cfgRadarRectX, *cfgRadarRectY, *cfgRadarOutline, *cfgRadarShape;
GtaVec2d RadarRect = {1.0f, 1.0f};

// Game vars
void *maskVertices;
bool *bDrawRadarMap;
float *NearScreenZ;

// Game funcs
void (*SetMaskVertices)(int, GtaVec2d*, float);
void (*RenderIndexedPrimitive)(int, void*, int, uint16_t*, int);
void (*TransformRadarPointToScreenSpace)(GtaVec2d&, GtaVec2d&);

// Mod desc
MYMOD(net.rusjj.radarshaper, Radar ShapeR, 1.1, RusJJ)
BEGIN_DEPLIST()
    ADD_DEPENDENCY_VER(net.rusjj.aml, 1.1)
END_DEPLIST()

// Funcs
inline void ClampToRectCenter(float width, float height, GtaVec2d& vec)
{
    if(fabsf(vec.x) > width || fabsf(vec.y) > height)
    {
        float scale = fabsf(vec.x / width), scaley = fabsf(vec.y / height);
        if(scaley > scale) scale = scaley;
        
        vec.x /= scale;
        vec.y /= scale;
    }
}
inline void ToggleRadarOutline(bool enable)
{
  #ifdef AML32
    if(enable)
    {
        aml->Write(pGame + 0x437F14, "\x65\xF5\x0C\xEB", 4);
        aml->Write(pGame + 0x437F44, "\x65\xF5\xF4\xEA", 4);
        aml->Write(pGame + 0x437F74, "\x65\xF5\xDC\xEA", 4);
        aml->Write(pGame + 0x437FA0, "\x65\xF5\xC6\xEA", 4);
    }
    else
    {
        aml->Write(pGame + 0x437F14, "\xAF\xF3\x00\x80", 4);
        aml->Write(pGame + 0x437F44, "\xAF\xF3\x00\x80", 4);
        aml->Write(pGame + 0x437F74, "\xAF\xF3\x00\x80", 4);
        aml->Write(pGame + 0x437FA0, "\xAF\xF3\x00\x80", 4);
    }
  #else
    if(enable)
    {
        aml->Write(pGame + 0x51D494, "\x2B\x40\xF4\x97", 4);
        aml->Write(pGame + 0x51D4C8, "\x1E\x40\xF4\x97", 4);
        aml->Write(pGame + 0x51D4FC, "\x11\x40\xF4\x97", 4);
        aml->Write(pGame + 0x51D52C, "\x05\x40\xF4\x97", 4);
    }
    else
    {
        aml->Write(pGame + 0x51D494, "\x1F\x20\x03\xD5", 4);
        aml->Write(pGame + 0x51D4C8, "\x1F\x20\x03\xD5", 4);
        aml->Write(pGame + 0x51D4FC, "\x1F\x20\x03\xD5", 4);
        aml->Write(pGame + 0x51D52C, "\x1F\x20\x03\xD5", 4);
    }
  #endif
}
inline float Dist2D(float x, float y)
{
    return sqrtf(x * x + y * y);
}

// LimitRadarPoint
inline float LRPCircle(GtaVec2d& vec)
{
    float x = vec.x, y = vec.y, dist = Dist2D(x, y);
    if(!*bDrawRadarMap && dist > 1.0f)
    {
        float scale = 1.0f / dist;
        vec.x = x * scale;
        vec.y = y * scale;
    }
    return dist;
}
inline float LRPRect(GtaVec2d& vec)
{
    float x = vec.x, y = vec.y, dist = Dist2D(x, y);
    if(!*bDrawRadarMap /*&& dist > 1.0f*/)
    {
        ClampToRectCenter(RadarRect.x, RadarRect.y, vec);
    }
    return dist;
}

// DrawRadarMask
inline void RadarMaskRect()
{
    static int rectIdx = 0;
    static GtaVec2d rectMask[4][6];
    static GtaVec2d cachedRadarRect = {0, 0};
    static uint16_t maskIndices[] = {
        0, 1, 2,
        3, 4, 5,
        6, 7, 8,
        9, 10,11,
        12,13,14,
        15,16,17,
    };

    GtaVec2d in;
    if(cachedRadarRect.x != RadarRect.x || cachedRadarRect.y != RadarRect.y)
    {
        cachedRadarRect.x = RadarRect.x;
        cachedRadarRect.y = RadarRect.y;
        rectIdx = 0;

        if(RadarRect.x < 1) // A(1,1) B(1,-1) C(X,-1) D(X,1)
        {
            // left
            in = {           -1,            1 }; TransformRadarPointToScreenSpace(rectMask[rectIdx][0], in);
            in = {           -1,           -1 }; TransformRadarPointToScreenSpace(rectMask[rectIdx][1], in);
            in = { -RadarRect.x,           -1 }; TransformRadarPointToScreenSpace(rectMask[rectIdx][2], in);
            in = {           -1,            1 }; TransformRadarPointToScreenSpace(rectMask[rectIdx][3], in);
            in = { -RadarRect.x,            1 }; TransformRadarPointToScreenSpace(rectMask[rectIdx][4], in);
            in = { -RadarRect.x,           -1 }; TransformRadarPointToScreenSpace(rectMask[rectIdx][5], in);
            ++rectIdx;

            // right
            in = {            1,            1 }; TransformRadarPointToScreenSpace(rectMask[rectIdx][0], in);
            in = {            1,           -1 }; TransformRadarPointToScreenSpace(rectMask[rectIdx][1], in);
            in = {  RadarRect.x,           -1 }; TransformRadarPointToScreenSpace(rectMask[rectIdx][2], in);
            in = {            1,            1 }; TransformRadarPointToScreenSpace(rectMask[rectIdx][3], in);
            in = {  RadarRect.x,            1 }; TransformRadarPointToScreenSpace(rectMask[rectIdx][4], in);
            in = {  RadarRect.x,           -1 }; TransformRadarPointToScreenSpace(rectMask[rectIdx][5], in);
            ++rectIdx;
        }

        if(RadarRect.y < 1)
        {
            // top
            in = {           -1,            1 }; TransformRadarPointToScreenSpace(rectMask[rectIdx][0], in);
            in = {           -1,  RadarRect.y }; TransformRadarPointToScreenSpace(rectMask[rectIdx][1], in);
            in = {            1,  RadarRect.y }; TransformRadarPointToScreenSpace(rectMask[rectIdx][2], in);
            in = {           -1,            1 }; TransformRadarPointToScreenSpace(rectMask[rectIdx][3], in);
            in = {            1,            1 }; TransformRadarPointToScreenSpace(rectMask[rectIdx][4], in);
            in = {            1,  RadarRect.y }; TransformRadarPointToScreenSpace(rectMask[rectIdx][5], in);
            ++rectIdx;

            // bottom
            in = {           -1,           -1 }; TransformRadarPointToScreenSpace(rectMask[rectIdx][0], in);
            in = {            1,           -1 }; TransformRadarPointToScreenSpace(rectMask[rectIdx][1], in);
            in = {            1, -RadarRect.y }; TransformRadarPointToScreenSpace(rectMask[rectIdx][2], in);
            in = {           -1,           -1 }; TransformRadarPointToScreenSpace(rectMask[rectIdx][3], in);
            in = {           -1, -RadarRect.y }; TransformRadarPointToScreenSpace(rectMask[rectIdx][4], in);
            in = {            1, -RadarRect.y }; TransformRadarPointToScreenSpace(rectMask[rectIdx][5], in);
            ++rectIdx;
        }
    }

    for(int rect = 0; rect < rectIdx; ++rect)
    {
        SetMaskVertices(6, rectMask[rect], *NearScreenZ);
        RenderIndexedPrimitive(3, maskVertices, 6, maskIndices, 18);
    }
}

// Switches
extern "C" float LRPSwitch(GtaVec2d& vec)
{
    switch(nRadarShape)
    {
        default:            return LRPCircle(vec);
        case SHAPE_RECT:    return LRPRect(vec);
    }
}
extern "C" uintptr_t RadarMaskSwitch()
{
    switch(nRadarShape)
    {
        default:            return pMaskContinueBackTo;
        case SHAPE_RECT:    RadarMaskRect(); break;
    }
    return pMaskBackTo;
}

// UltraDumbPatches :(
#ifdef AML32
__attribute__((optnone)) __attribute__((naked)) void MaskPatch(void)
{
    asm volatile("ADD.W R0, R8, #8\nSTR R0, [SP,#0x18]\nMOVS R1, #0\nVMOV.F32 S20, #6.0"); // org
    asm volatile("PUSH {R0-R1}\nBL RadarMaskSwitch\nMOV R12, R0\nPOP {R0-R1}");
    asm volatile("BX R12");
}
__attribute__((optnone)) __attribute__((naked)) void LRPPatch01(void)
{
    asm volatile("PUSH {R0-R11}\nSUB SP, SP, #8");
    asm volatile("VMOV R0, S0\nVMOV R1, S2\nSTR R0, [SP, #0]\nSTR R1, [SP, #4]");
    asm volatile("MOV R0, SP\nBL LRPSwitch");
    asm volatile("LDR R0, [SP, #0]\nLDR R1, [SP, #4]\nVMOV S0, R0\nVMOV S2, R1");
    asm volatile(
        "MOV R12, %0\n"
    :: "r" (pLRPBackTo1));
    asm volatile("ADD SP, SP, #8\nPOP {R0-R11}");
    asm volatile("BX R12");
}
__attribute__((optnone)) __attribute__((naked)) void LRPPatch02(void)
{
    asm volatile("PUSH {R0-R11}\nSUB SP, SP, #8");
    asm volatile("VMOV R0, S0\nVMOV R1, S2\nSTR R0, [SP, #0]\nSTR R1, [SP, #4]");
    asm volatile("MOV R0, SP\nBL LRPSwitch");
    asm volatile("LDR R0, [SP, #0]\nLDR R1, [SP, #4]\nVMOV S0, R0\nVMOV S2, R1");
    asm volatile(
        "MOV R12, %0\n"
    :: "r" (pLRPBackTo2));
    asm volatile("ADD SP, SP, #8\nPOP {R0-R11}");
    asm volatile("BX R12");
}
__attribute__((optnone)) __attribute__((naked)) void LRPPatch03(void)
{
    asm volatile("PUSH {R0-R11}\nSUB SP, SP, #8");
    asm volatile("VMOV R1, S2\nVMOV R0, S4\nSTR R0, [SP, #0]\nSTR R1, [SP, #4]");
    asm volatile("MOV R0, SP\nBL LRPSwitch");
    asm volatile("VMOV S8, R0\nLDR R0, [SP, #0]\nLDR R1, [SP, #4]\nVMOV S2, R1\nVMOV S4, R0");
    asm volatile(
        "MOV R12, %0\n"
    :: "r" (pLRPBackTo3));
    asm volatile("ADD SP, SP, #8\nPOP {R0-R11}\nVMOV.F32 S0, #1.0");
    asm volatile("BX R12");
}
__attribute__((optnone)) __attribute__((naked)) void LRPPatch04(void)
{
    asm volatile("PUSH {R0-R11}\nSUB SP, SP, #8");
    asm volatile("VMOV R1, S26\nVMOV R0, S24\nSTR R0, [SP, #0]\nSTR R1, [SP, #4]");
    asm volatile("MOV R0, SP\nBL LRPSwitch");
    asm volatile("VMOV S0, R0\nLDR R0, [SP, #0]\nLDR R1, [SP, #4]\nVMOV S26, R1\nVMOV S24, R0");
    asm volatile(
        "MOV R12, %0\n"
    :: "r" (pLRPBackTo4));
    asm volatile("ADD SP, SP, #8\nPOP {R0-R11}");
    asm volatile("BX R12");
}
__attribute__((optnone)) __attribute__((naked)) void LRPPatch05(void)
{
    asm volatile("PUSH {R0-R11}\nSUB SP, SP, #8");
    asm volatile("VMOV R0, S16\nVMOV R1, S24\nSTR R0, [SP, #0]\nSTR R1, [SP, #4]");
    asm volatile("MOV R0, SP\nBL LRPSwitch");
    asm volatile("VMOV S0, R0\nLDR R0, [SP, #0]\nLDR R1, [SP, #4]\nVMOV S16, R0\nVMOV S24, R1");
    asm volatile(
        "MOV R12, %0\n"
    :: "r" (pLRPBackTo5));
    asm volatile("ADD SP, SP, #8\nPOP {R0-R11}");
    asm volatile("BX R12");
}
#else
__attribute__((optnone)) __attribute__((naked)) void MaskPatch(void)
{
    asm volatile("LDR S10, [X8, #0xA20]\nLDR X26, [X26, #0xFA0]\nMOV X23, XZR\nADD X19, X19, #8"); // org
    asm volatile("BL RadarMaskSwitch\nBR X0");
}
__attribute__((optnone)) __attribute__((naked)) void LRPPatch01(void)
{
    asm volatile("SUB SP, SP, #16");
    asm volatile("STR S1, [SP, #0]\nSTR S0, [SP, #4]\nSTR X8, [SP, #8]");
    asm volatile("MOV X0, SP\nBL LRPSwitch");
    asm volatile("LDR S1, [SP, #0]\nLDR S0, [SP, #4]");
    asm volatile(
        "MOV X0, %0\n"
    :: "r" (pLRPBackTo1));
    asm volatile("LDR X8, [SP, #8]\nADD SP, SP, #16");
    asm volatile("BR X0");
}
__attribute__((optnone)) __attribute__((naked)) void LRPPatch02(void)
{
    asm volatile("SUB SP, SP, #16");
    asm volatile("STR S1, [SP, #0]\nSTR S0, [SP, #4]\nSTR W8, [SP, #8]");
    asm volatile("MOV X0, SP\nBL LRPSwitch");
    asm volatile("LDR S1, [SP, #0]\nLDR S0, [SP, #4]");
    asm volatile(
        "MOV X0, %0\n"
    :: "r" (pLRPBackTo2));
    asm volatile("LDR W8, [SP, #8]\nADD SP, SP, #16");
    asm volatile("BR X0");
}
__attribute__((optnone)) __attribute__((naked)) void LRPPatch03(void)
{
    asm volatile("SUB SP, SP, #16");
    asm volatile("STR S2, [SP, #0]\nSTR S1, [SP, #4]\nSTR W8, [SP, #8]");
    asm volatile("MOV X0, SP\nBL LRPSwitch");
    asm volatile("FMOV S3, S0\nLDR S2, [SP, #0]\nLDR S1, [SP, #4]\nFMOV S0, #1.0");
    asm volatile(
        "MOV X0, %0\n"
    :: "r" (pLRPBackTo3));
    asm volatile("LDR W8, [SP, #8]\nADD SP, SP, #16");
    asm volatile("BR X0");
}
__attribute__((optnone)) __attribute__((naked)) void LRPPatch04(void)
{
    asm volatile("SUB SP, SP, #16");
    asm volatile("STR S13, [SP, #0]\nSTR S14, [SP, #4]\nSTR W8, [SP, #8]");
    asm volatile("MOV X0, SP\nBL LRPSwitch");
    asm volatile("LDR S13, [SP, #0]\nLDR S14, [SP, #4]");
    asm volatile(
        "MOV X0, %0\n"
    :: "r" (pLRPBackTo4));
    asm volatile("LDR W8, [SP, #8]\nADD SP, SP, #16");
    asm volatile("BR X0");
}
__attribute__((optnone)) __attribute__((naked)) void LRPPatch05(void)
{
    asm volatile("SUB SP, SP, #16");
    asm volatile("STR S1, [SP, #0]\nSTR S0, [SP, #4]\nSTR W8, [SP, #8]");
    asm volatile("MOV X0, SP\nBL LRPSwitch");
    asm volatile("LDR S1, [SP, #0]\nLDR S0, [SP, #4]");
    asm volatile(
        "MOV X0, %0\n"
    :: "r" (pLRPBackTo5));
    asm volatile("LDR W8, [SP, #8]\nADD SP, SP, #16");
    asm volatile("BR X0");
}
#endif
// UltraDumbPatches :)

// Configs
char szRetScale[16];
const char* OnRadarScaleDraw(int newVal, void* data)
{
    sprintf(szRetScale, "x%.2f", 0.01f * newVal);
    return szRetScale;
}
void OnRadarSettingChange(int oldVal, int newVal, void* data)
{
    switch((int)(intptr_t)data)
    {
        case 0:
            ToggleRadarOutline(newVal);
            cfgRadarOutline->SetInt(newVal);
            break;
        case 1:
            cfgRadarShape->SetInt(newVal);
            cfgRadarShape->Clamp((int)SHAPE_CIRCLE, (int)MAX_SHAPES - 1);
            nRadarShape = (eRadarShape)cfgRadarShape->GetInt();
            break;
        case 2:
            cfgRadarRectX->SetFloat(0.01f * newVal);
            cfgRadarRectX->Clamp(0.05f, 1.0f);
            RadarRect.x = cfgRadarRectX->GetFloat();
            break;
        case 3:
            cfgRadarRectY->SetFloat(0.01f * newVal);
            cfgRadarRectY->Clamp(0.05f, 1.0f);
            RadarRect.y = cfgRadarRectY->GetFloat();
            break;
    }
    cfg->Save();
}

extern "C" void OnAllModsLoaded()
{
    logger->SetTag("RadarShapeR");

    if((hGame = aml->GetLibHandle("libGTASA.so")) && (pGame = aml->GetLib("libGTASA.so")))
    {
        cfg = new Config("RadarShapeR.SA");

        aml->Hook((void*)aml->GetSym(hGame, "_ZN6CRadar15LimitRadarPointER9CVector2D"), (void*)LRPSwitch, NULL);

        SET_TO(SetMaskVertices, aml->GetSym(hGame, "_ZN9CSprite2d15SetMaskVerticesEiPff"));
        SET_TO(RenderIndexedPrimitive, aml->GetSym(hGame, "_Z35RwIm2DRenderIndexedPrimitive_BUGFIX15RwPrimitiveTypeP14RwOpenGLVertexiPti"));
        SET_TO(TransformRadarPointToScreenSpace, aml->GetSym(hGame, "_ZN6CRadar32TransformRadarPointToScreenSpaceER9CVector2DRKS0_"));

        SET_TO(maskVertices, aml->GetSym(hGame, "_ZN9CSprite2d10maVerticesE"));
        SET_TO(bDrawRadarMap, pGame + BYBIT(0x6E00D8, 0x8BE80C));
        SET_TO(NearScreenZ, aml->GetSym(hGame, "_ZN9CSprite2d11NearScreenZE"));

      #ifdef AML32
        //aml->Redirect(pGame + 0x43F710 + 0x1, (uintptr_t)LRPSwitch);

        aml->Write(pGame + 0x4442B0, "\x00\x21", 2);
        aml->Write(pGame + 0x4442BA, "\x7A\x48", 2);
        pMaskBackTo = pGame + 0x44446A + 0x1;
        pMaskContinueBackTo = pGame + 0x4442B6 + 0x1;
        aml->Redirect(pGame + 0x4442AA + 0x1, (uintptr_t)MaskPatch);

        pLRPBackTo1 = pGame + 0x43EB40 + 0x1;
        aml->Redirect(pGame + 0x43EB1A + 0x1, (uintptr_t)LRPPatch01);

        pLRPBackTo2 = pGame + 0x43EEEE + 0x1;
        aml->Redirect(pGame + 0x43EEC8 + 0x1, (uintptr_t)LRPPatch02);

        pLRPBackTo3 = pGame + 0x43FC04 + 0x1;
        aml->Redirect(pGame + 0x43FBDC + 0x1, (uintptr_t)LRPPatch03);

        pLRPBackTo4 = pGame + 0x440620 + 0x1;
        aml->Redirect(pGame + 0x4405F8 + 0x1, (uintptr_t)LRPPatch04);

        pLRPBackTo5 = pGame + 0x440AD6 + 0x1;
        aml->Redirect(pGame + 0x440AAE + 0x1, (uintptr_t)LRPPatch05);
      #else
        //aml->Redirect(pGame + 0x524B2C, (uintptr_t)LRPSwitch);
        
        pMaskBackTo = pGame + 0x529720;
        pMaskContinueBackTo = pGame + 0x5295B8;
        aml->Redirect(pGame + 0x5295A8, (uintptr_t)MaskPatch);
        
        pLRPBackTo1 = pGame + 0x524068;
        aml->Redirect(pGame + 0x524048, (uintptr_t)LRPPatch01);
        
        pLRPBackTo2 = pGame + 0x524504;
        aml->Redirect(pGame + 0x5244E4, (uintptr_t)LRPPatch02);
        
        pLRPBackTo3 = pGame + 0x525034;
        aml->Redirect(pGame + 0x525010, (uintptr_t)LRPPatch03);
        
        pLRPBackTo4 = pGame + 0x525A7C;
        aml->Redirect(pGame + 0x525A58, (uintptr_t)LRPPatch04);
        
        pLRPBackTo5 = pGame + 0x525F6C;
        aml->Redirect(pGame + 0x525F4C, (uintptr_t)LRPPatch05);
      #endif
    }
    else if((hGame = aml->GetLibHandle("libGTAVC.so")) && (pGame = aml->GetLib("libGTAVC.so")))
    {
        cfg = new Config("RadarShapeR.VC"); // If i'll ever do this..!


    }

    sautils = (ISAUtils*)GetInterface("SAUtils");
    if(cfg)
    {
        ToggleRadarOutline(false); // default

        cfgRadarRectX   = cfg->Bind("RadarRectX", 1.0f);         if(cfgRadarRectX->LoadedUndefault())   OnRadarSettingChange(100 * 1.0f, cfgRadarRectX->GetFloat() * 100, SETID_RECTX);
        cfgRadarRectY   = cfg->Bind("RadarRectY", 1.0f);         if(cfgRadarRectY->LoadedUndefault())   OnRadarSettingChange(100 * 1.0f, cfgRadarRectY->GetFloat() * 100, SETID_RECTY);
        cfgRadarOutline = cfg->Bind("RadarOutline", false);      if(cfgRadarOutline->LoadedUndefault()) OnRadarSettingChange(false, true, SETID_OUTLINE);
        cfgRadarShape   = cfg->Bind("RadarShape", SHAPE_CIRCLE); if(cfgRadarShape->LoadedUndefault())   OnRadarSettingChange(0, cfgRadarShape->GetInt(), SETID_SHAPE);

        if(sautils)
        {
            sautils->AddSliderItem(eTypeOfSettings::SetType_Game, "RADAR RECT X SCALE", 100 * cfgRadarRectX->GetFloat(), 100 * 0.05f, 100 * 1.0f, OnRadarSettingChange, OnRadarScaleDraw, SETID_RECTX);
            sautils->AddSliderItem(eTypeOfSettings::SetType_Game, "RADAR RECT Y SCALE", 100 * cfgRadarRectY->GetFloat(), 100 * 0.05f, 100 * 1.0f, OnRadarSettingChange, OnRadarScaleDraw, SETID_RECTY);

            sautils->AddClickableItem(eTypeOfSettings::SetType_Game, "DISPLAY RADAR OUTLINE", cfgRadarOutline->GetInt() != 0, 0, sizeofA(aYesNo)-1, aYesNo, OnRadarSettingChange, SETID_OUTLINE);
            sautils->AddClickableItem(eTypeOfSettings::SetType_Game, "DISPLAY RADAR SHAPE", cfgRadarShape->GetInt(), 0, sizeofA(aShapes)-1, aShapes, OnRadarSettingChange, SETID_SHAPE);
        }
    }
}