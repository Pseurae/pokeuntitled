#include "global.h"
#include "data.h"
#include "dynamic_placeholder_text_util.h"
#include "field_effect.h"
#include "graphics.h"
#include "international_string_util.h"
#include "item_menu.h"
#include "main.h"
#include "menu.h"
#include "menu_helpers.h"
#include "palette.h"
#include "pokemon.h"
#include "pokemon_icon.h"
#include "pokemon_summary_screen.h"
#include "scanline_effect.h"
#include "stats_display.h"
#include "strings.h"
#include "text_window.h"

#include "constants/rgb.h"

#include "bg.h"
#include "decompress.h"
#include "gpu_regs.h"
#include "malloc.h"
#include "string_util.h"

static void VBlankCB(void);
static void MainCB2(void);
static void CB2_InitStatsDisplay(void);
static bool8 InitStatsDisplay(void);
static void Task_StatDisplayFadeIn(u8 taskId);
static void Task_StatDisplayStartFadeOut(u8 taskId);
static void Task_StatDisplayFadeOut(u8 taskId);
static void DestroyPartyMonIcons(void);
static void UpdatePartyMonIcons(void);
static void Task_ChangeStatsMon(u8 taskId);
static void Task_HandleInput(u8 taskId);

static EWRAM_DATA struct StatsDisplayData sDisplay = { 0 };

static const u8 sTextColor[3] = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_WHITE, TEXT_COLOR_DARK_GRAY};

static void VBlankCB(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

static void MainCB2(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    DoScheduledBgTilemapCopiesToVram();
    UpdatePaletteFade();
}

void OpenStatsDisplay(u8 type, void (* callback)(void))
{
    if (callback != NULL)
        sDisplay.savedCallback = callback;

    SetMainCallback2(CB2_InitStatsDisplay);
}

static u8 CreateMonSprite(u16 species)
{
    u8 spriteId = CreateMonSprite_PicBox(species, 40, 64, 5);
    return spriteId;
}

static void CB2_InitStatsDisplay(void)
{
    while (1)
    {
        if (IsActiveOverworldLinkBusy() == TRUE)
            break;
        if (InitStatsDisplay() == TRUE)
            break;
        if (MenuHelpers_IsLinkActive() == TRUE)
            break;
    }
}

static void Task_StatDisplayStartFadeOut(u8 taskId)
{
    BeginNormalPaletteFade(PALETTES_ALL, -2, 0, 16, RGB_BLACK);
    gTasks[taskId].func = Task_StatDisplayFadeOut;
}

static void Task_StatDisplayFadeOut(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        DestroyPartyMonIcons();
        FreeAllWindowBuffers();

        if (sDisplay.savedCallback != NULL)
            SetMainCallback2(sDisplay.savedCallback);

        DestroyTask(taskId);
    }
}


static void PrintPokemonName(void)
{
    u16 species = GetMonData(sDisplay.selectedMon, MON_DATA_SPECIES2);

    FillWindowPixelBuffer(1, PIXEL_FILL(0));
    AddTextPrinterParameterized3(1, FONT_SMALL, 2, 1, sTextColor, 0, gSpeciesNames[species]);
}

static void CreateMonTypeIcons(void)
{
    u8 i, spriteId;

    LoadCompressedSpriteSheet(&gSpriteSheet_MoveTypes);
    LoadCompressedPalette(gMoveTypes_Pal, 0x1D0, 0x60);

    for (i = 0; i < 2; i++)
    {
        spriteId = CreateSprite(&gSpriteTemplate_MoveTypes, 0, 0, 2);
        sDisplay.typeSpriteIds[i] = spriteId;
        gSprites[spriteId].invisible = TRUE;
    }
}

static void SetTypeSpritePosAndPal(u8 typeId, u8 x, u8 y, u8 spriteArrayId)
{
    struct Sprite *sprite = &gSprites[sDisplay.typeSpriteIds[spriteArrayId]];
    StartSpriteAnim(sprite, typeId);
    sprite->oam.paletteNum = gMoveTypeToOamPaletteNum[typeId];
    sprite->x = x + 16;
    sprite->y = y + 8;
    sprite->invisible = FALSE;
}

#define MON_TYPE_ICON_XPOS1 160
#define MON_TYPE_ICON_XPOS2 200
#define MON_TYPE_ICON_YPOS 9

static void SetMonTypeIcons(void)
{
    u8 type1, type2;
    type1 = gBaseStats[sDisplay.selectedMonSpecies].type1;
    type2 = gBaseStats[sDisplay.selectedMonSpecies].type2;

    if (type1 != type2)
    {
        SetTypeSpritePosAndPal(type1, MON_TYPE_ICON_XPOS1, MON_TYPE_ICON_YPOS, 0);
        SetTypeSpritePosAndPal(type2, MON_TYPE_ICON_XPOS2, MON_TYPE_ICON_YPOS, 1);
    }
    else
    {
        SetTypeSpritePosAndPal(type1, MON_TYPE_ICON_XPOS1, MON_TYPE_ICON_YPOS, 0);
        gSprites[sDisplay.typeSpriteIds[1]].invisible = TRUE;
    }
}

static const u8 *const sStatNames[] = {
    gText_HP4,
    gText_Attack3,
    gText_Defense3,
    gText_SpAtk4,
    gText_SpDef4,
    gText_Speed2
};

static void PrintPokemonStatNames(void)
{
    u8 i, x;

    FillWindowPixelBuffer(1, PIXEL_FILL(0));
    for (i = 0; i < NELEMS(sStatNames); i++)
    {
        x = GetStringRightAlignXOffset(FONT_SMALL, sStatNames[i], 40);
        AddTextPrinterParameterized3(5, FONT_SMALL, x, 12 * (i + 1), sTextColor, 0, sStatNames[i]);
    }
}

static const u8 sText_Base[] = _("Base");
static const u8 sText_EVs[] = _("EVs");
static const u8 sText_IVs[] = _("IVs");
static const u8 sStatsLayout[] = _("{DYNAMIC 0}\n{DYNAMIC 1}\n{DYNAMIC 2}\n{DYNAMIC 3}\n{DYNAMIC 4}\n{DYNAMIC 5}");

static void PrintPokemonBaseStats(void)
{
    u8 x;
    u8 *hp, *atk, *def;
    u16 species;

    FillWindowPixelBuffer(2, PIXEL_FILL(0));

    species = GetMonData(sDisplay.selectedMon, MON_DATA_SPECIES);

    hp = Alloc(3);
    atk = Alloc(3);
    def = Alloc(3);

    ConvertIntToDecimalStringN(hp, gBaseStats[species].baseHP, STR_CONV_MODE_RIGHT_ALIGN, 3);
    ConvertIntToDecimalStringN(atk, gBaseStats[species].baseAttack, STR_CONV_MODE_RIGHT_ALIGN, 3);
    ConvertIntToDecimalStringN(def, gBaseStats[species].baseDefense, STR_CONV_MODE_RIGHT_ALIGN, 3);

    ConvertIntToDecimalStringN(gStringVar1, gBaseStats[species].baseSpAttack, STR_CONV_MODE_RIGHT_ALIGN, 3);
    ConvertIntToDecimalStringN(gStringVar2, gBaseStats[species].baseSpDefense, STR_CONV_MODE_RIGHT_ALIGN, 3);
    ConvertIntToDecimalStringN(gStringVar3, gBaseStats[species].baseSpeed, STR_CONV_MODE_RIGHT_ALIGN, 3);

    DynamicPlaceholderTextUtil_Reset();
    DynamicPlaceholderTextUtil_SetPlaceholderPtr(0, hp);
    DynamicPlaceholderTextUtil_SetPlaceholderPtr(1, atk);
    DynamicPlaceholderTextUtil_SetPlaceholderPtr(2, def);
    DynamicPlaceholderTextUtil_SetPlaceholderPtr(3, gStringVar1);
    DynamicPlaceholderTextUtil_SetPlaceholderPtr(4, gStringVar2);
    DynamicPlaceholderTextUtil_SetPlaceholderPtr(5, gStringVar3);

    DynamicPlaceholderTextUtil_ExpandPlaceholders(gStringVar4, sStatsLayout);

    x = GetStringRightAlignXOffset(FONT_SMALL, gStringVar4, 24);
    AddTextPrinterParameterized3(2, FONT_SMALL, x, 12, sTextColor, 0, gStringVar4);

    x = GetStringRightAlignXOffset(FONT_SMALL, sText_Base, 24);
    AddTextPrinterParameterized3(2, FONT_SMALL, x, 0, sTextColor, 0, sText_Base);

    Free(hp);
    Free(atk);
    Free(def);
}

static void PrintPokemonEVs(void)
{
    u8 x;
    u8 *hp, *atk, *def;
    u16 species;

    FillWindowPixelBuffer(3, PIXEL_FILL(0));

    species = GetMonData(sDisplay.selectedMon, MON_DATA_SPECIES);

    hp = Alloc(3);
    atk = Alloc(3);
    def = Alloc(3);

    ConvertIntToDecimalStringN(hp, GetMonData(sDisplay.selectedMon, MON_DATA_HP_EV), STR_CONV_MODE_RIGHT_ALIGN, 3);
    ConvertIntToDecimalStringN(atk, GetMonData(sDisplay.selectedMon, MON_DATA_ATK_EV), STR_CONV_MODE_RIGHT_ALIGN, 3);
    ConvertIntToDecimalStringN(def, GetMonData(sDisplay.selectedMon, MON_DATA_DEF_EV), STR_CONV_MODE_RIGHT_ALIGN, 3);

    ConvertIntToDecimalStringN(gStringVar1, GetMonData(sDisplay.selectedMon, MON_DATA_SPATK_EV), STR_CONV_MODE_RIGHT_ALIGN, 3);
    ConvertIntToDecimalStringN(gStringVar2, GetMonData(sDisplay.selectedMon, MON_DATA_SPDEF_EV), STR_CONV_MODE_RIGHT_ALIGN, 3);
    ConvertIntToDecimalStringN(gStringVar3, GetMonData(sDisplay.selectedMon, MON_DATA_SPEED_EV), STR_CONV_MODE_RIGHT_ALIGN, 3);

    DynamicPlaceholderTextUtil_Reset();
    DynamicPlaceholderTextUtil_SetPlaceholderPtr(0, hp);
    DynamicPlaceholderTextUtil_SetPlaceholderPtr(1, atk);
    DynamicPlaceholderTextUtil_SetPlaceholderPtr(2, def);
    DynamicPlaceholderTextUtil_SetPlaceholderPtr(3, gStringVar1);
    DynamicPlaceholderTextUtil_SetPlaceholderPtr(4, gStringVar2);
    DynamicPlaceholderTextUtil_SetPlaceholderPtr(5, gStringVar3);

    DynamicPlaceholderTextUtil_ExpandPlaceholders(gStringVar4, sStatsLayout);

    x = GetStringRightAlignXOffset(FONT_SMALL, gStringVar4, 24);
    AddTextPrinterParameterized3(3, FONT_SMALL, x, 12, sTextColor, 0, gStringVar4);

    x = GetStringRightAlignXOffset(FONT_SMALL, sText_EVs, 24);
    AddTextPrinterParameterized3(3, FONT_SMALL, x, 0, sTextColor, 0, sText_EVs);

    Free(hp);
    Free(atk);
    Free(def);
}

static void PrintPokemonIVs(void)
{
    u8 x;
    u8 *hp, *atk, *def;
    u16 species;

    FillWindowPixelBuffer(4, PIXEL_FILL(0));

    species = GetMonData(sDisplay.selectedMon, MON_DATA_SPECIES);

    hp = Alloc(3);
    atk = Alloc(3);
    def = Alloc(3);

    ConvertIntToDecimalStringN(hp, GetMonData(sDisplay.selectedMon, MON_DATA_HP_IV), STR_CONV_MODE_RIGHT_ALIGN, 3);
    ConvertIntToDecimalStringN(atk, GetMonData(sDisplay.selectedMon, MON_DATA_ATK_IV), STR_CONV_MODE_RIGHT_ALIGN, 3);
    ConvertIntToDecimalStringN(def, GetMonData(sDisplay.selectedMon, MON_DATA_DEF_IV), STR_CONV_MODE_RIGHT_ALIGN, 3);

    ConvertIntToDecimalStringN(gStringVar1, GetMonData(sDisplay.selectedMon, MON_DATA_SPATK_IV), STR_CONV_MODE_RIGHT_ALIGN, 3);
    ConvertIntToDecimalStringN(gStringVar2, GetMonData(sDisplay.selectedMon, MON_DATA_SPDEF_IV), STR_CONV_MODE_RIGHT_ALIGN, 3);
    ConvertIntToDecimalStringN(gStringVar3, GetMonData(sDisplay.selectedMon, MON_DATA_SPEED_IV), STR_CONV_MODE_RIGHT_ALIGN, 3);

    DynamicPlaceholderTextUtil_Reset();
    DynamicPlaceholderTextUtil_SetPlaceholderPtr(0, hp);
    DynamicPlaceholderTextUtil_SetPlaceholderPtr(1, atk);
    DynamicPlaceholderTextUtil_SetPlaceholderPtr(2, def);
    DynamicPlaceholderTextUtil_SetPlaceholderPtr(3, gStringVar1);
    DynamicPlaceholderTextUtil_SetPlaceholderPtr(4, gStringVar2);
    DynamicPlaceholderTextUtil_SetPlaceholderPtr(5, gStringVar3);

    DynamicPlaceholderTextUtil_ExpandPlaceholders(gStringVar4, sStatsLayout);

    x = GetStringRightAlignXOffset(FONT_SMALL, gStringVar4, 24);
    AddTextPrinterParameterized3(4, FONT_SMALL, x, 12, sTextColor, 0, gStringVar4);

    x = GetStringRightAlignXOffset(FONT_SMALL, sText_IVs, 24);
    AddTextPrinterParameterized3(4, FONT_SMALL, x, 0, sTextColor, 0, sText_IVs);

    Free(hp);
    Free(atk);
    Free(def);
}

static void PrintPokemonInfo(void)
{
    PrintPokemonBaseStats();
    PrintPokemonEVs();
    PrintPokemonIVs();
}

static void SetSelectedMon(u8 i)
{
    sDisplay.selectedMon = &gPlayerParty[sDisplay.selectedIndex];
    sDisplay.selectedMonSpecies = GetMonData(sDisplay.selectedMon, MON_DATA_SPECIES);
}

static void ChangeStatsMon(u8 taskId, s8 delta)
{
    delta += gPlayerPartyCount;

    sDisplay.selectedIndex = (sDisplay.selectedIndex + delta) % gPlayerPartyCount;
    SetSelectedMon(sDisplay.selectedIndex);

    gTasks[taskId].data[0] = 0;
    gTasks[taskId].func = Task_ChangeStatsMon;
}

static void Task_ChangeStatsMon(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    switch (data[0])
    {
    case 0:
        UpdatePartyMonIcons();
        break;
    case 1:
        PrintPokemonName();
        PrintPokemonInfo();
        SetMonTypeIcons();
        break;
    case 2:
        FreeResourcesAndDestroySprite(&gSprites[sDisplay.portraitSpriteId], sDisplay.portraitSpriteId);
        sDisplay.portraitSpriteId = CreateMonSprite(sDisplay.selectedMonSpecies);
        break;
    default:
        gTasks[taskId].func = Task_HandleInput;
        data[0] = 0;
        return;
    }
    data[0]++;
}

static void Task_HandleInput(u8 taskId)
{
    if (JOY_NEW(A_BUTTON | B_BUTTON))
    {
        Task_StatDisplayStartFadeOut(taskId);
    }
    else if (JOY_NEW(DPAD_LEFT))
    {
        ChangeStatsMon(taskId, -1);
    }
    else if (JOY_NEW(DPAD_RIGHT))
    {
        ChangeStatsMon(taskId, 1);
    }
}

static void Task_StatDisplayFadeIn(u8 taskId)
{
    if (!gPaletteFade.active)
        gTasks[taskId].func = Task_HandleInput;
}

static void UpdatePartyMonIcons(void)
{
    u8 i;
    
    for (i = 0; i < gPlayerPartyCount; i++)
    {
        if (i != sDisplay.selectedIndex)
        {
            sDisplay.monIconSprites[i]->oam.objMode = ST_OAM_OBJ_BLEND;
            StartSpriteAnim(sDisplay.monIconSprites[i], 4);
        }
        else
        {
            sDisplay.monIconSprites[i]->oam.objMode = ST_OAM_OBJ_NORMAL;
            StartSpriteAnim(sDisplay.monIconSprites[i], 0);
        }
    }
}

static void DrawPartyMonIcons(void)
{
    u8 i, x, y, spriteId;
    u16 species;

    SetGpuReg(REG_OFFSET_BLDCNT, BLDCNT_TGT2_ALL);
    SetGpuReg(REG_OFFSET_BLDALPHA, BLDALPHA_BLEND(7, 11));

    for (i = 0; i < gPlayerPartyCount; i++)
    {
        x = 40 + 0x20 * i;
        y = 130;

        species = GetMonData(&gPlayerParty[i], MON_DATA_SPECIES);

        LoadMonIconPalette(species);
        spriteId = CreateMonIcon(species, SpriteCB_MonIcon, x, y, 1, GetMonData(&gPlayerParty[i], MON_DATA_PERSONALITY), TRUE);

        gSprites[spriteId].oam.priority = 0;

        sDisplay.monIconSprites[i] = &gSprites[spriteId];
    }
}

static void DestroyPartyMonIcons(void)
{
    u8 i;
    FreeMonIconPalettes();

    for (i = 0; i < gPlayerPartyCount; i++)
    {
        FreeAndDestroyMonIconSprite(sDisplay.monIconSprites[i]);
    }
}

static void LoadStatsDisplayGraphicsAndPalettes(void)
{
    DmaFill16(3, 0, VRAM, VRAM_SIZE);
    DmaFill32(3, 0, OAM, OAM_SIZE);
    DmaFill16(3, 0, PLTT, PLTT_SIZE);
}

static const struct WindowTemplate sWindowTemplates[] = {
    // Label
    {
        .bg = 0, 
        .tilemapLeft = 1, 
        .tilemapTop = 1, 
        .width = 8, 
        .height = 2, 
        .paletteNum = 15, 
        .baseBlock = 1
    },

    // Name
    {
        .bg = 0, 
        .tilemapLeft = 12, 
        .tilemapTop = 1, 
        .width = 6, 
        .height = 2, 
        .paletteNum = 15, 
        .baseBlock = 33
    },

    // Base Stats
    {
        .bg = 0, 
        .tilemapLeft = 18, 
        .tilemapTop = 4, 
        .width = 3,
        .height = 9,
        .paletteNum = 15, 
        .baseBlock = 62
    },

    // EVs
    {
        .bg = 0, 
        .tilemapLeft = 21, 
        .tilemapTop = 4, 
        .width = 3,
        .height = 9,
        .paletteNum = 15, 
        .baseBlock = 124
    },

    // IVs
    {
        .bg = 0, 
        .tilemapLeft = 24, 
        .tilemapTop = 4, 
        .width = 3,
        .height = 9,
        .paletteNum = 15, 
        .baseBlock = 182
    },

    // Stat Names
    {
        .bg = 0, 
        .tilemapLeft = 12, 
        .tilemapTop = 4, 
        .width = 5,
        .height = 9,
        .paletteNum = 15, 
        .baseBlock = 240
    },
};

static const char gText_StatsDisplay[] = _("Stats Display");

static void PrintStatsDisplayLabel(void)
{
    AddTextPrinterParameterized3(0, FONT_SMALL, 2, 1, sTextColor, 0, gText_StatsDisplay);
}

static void InitStatsDisplayWindows(void)
{
    u8 i;

    InitWindows(sWindowTemplates);
    DeactivateAllTextPrinters();
    
    LoadPalette(GetTextWindowPalette(0), 0xF0, 0x20);

    for (i = 0; i < NELEMS(sWindowTemplates); i++)
    {
        FillWindowPixelBuffer(i, PIXEL_FILL(0));
        PutWindowTilemap(i);
    }

    ScheduleBgCopyTilemapToVram(0);
}

static bool8 InitStatsDisplay(void)
{
    switch (gMain.state)
    {
    case 0:
        SetVBlankHBlankCallbacksToNull();
        ClearScheduledBgCopiesToVram();
        gMain.state++;
        break;
    case 1:
        ScanlineEffect_Stop();
        gMain.state++;
        break;
    case 2:
        FreeAllSpritePalettes();
        gMain.state++;
        break;
    case 3:
        ResetPaletteFade();
        gMain.state++;
        break;
    case 4:
        ResetSpriteData();
        gMain.state++;
        break;
    case 5:
        ResetTasks();
        gMain.state++;
        break;
    case 6:
        sDisplay.selectedIndex = 0;
        SetSelectedMon(sDisplay.selectedIndex);
        gMain.state++;
        break;
    case 7:
        LoadStatsDisplayGraphicsAndPalettes();
        gMain.state++;
        break;
    case 8:
        InitStatsDisplayWindows();
        gMain.state++;
        break;
    case 9:
        PrintStatsDisplayLabel();
        gMain.state++;
        break;
    case 10:
        DrawPartyMonIcons();
        UpdatePartyMonIcons();
        CreateMonTypeIcons();
        gMain.state++;
        break;
    case 11:
        PrintPokemonName();
        PrintPokemonInfo();
        SetMonTypeIcons();
        gMain.state++;
        break;
    case 12:
        sDisplay.portraitSpriteId = CreateMonSprite(sDisplay.selectedMonSpecies);
        gMain.state++;
        break;
    case 13:
        PrintPokemonStatNames();
        gMain.state++;
        break;
    case 14:
        gMain.state++;
        break;
    case 16:
        CreateTask(Task_StatDisplayFadeIn, 0);
        gMain.state++;
        break;
    case 17:
        BlendPalettes(PALETTES_ALL, 16, 0);
        gMain.state++;
        break;
    case 18:
        BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
        gMain.state++;
        break;
    case 19:
        SetVBlankCallback(VBlankCB);
        SetMainCallback2(MainCB2);
        return TRUE;
    default:
        gMain.state++;
        break;
    }
    return FALSE;
}