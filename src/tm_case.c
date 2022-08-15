#include "global.h"

#include "bg.h"
#include "palette.h"
#include "gpu_regs.h"
#include "dma3.h"
#include "malloc.h"
#include "sound.h"
#include "text.h"
#include "sprite.h"
#include "window.h"
#include "blit.h"
#include "string_util.h"

#include "decompress.h"
#include "graphics.h"
#include "task.h"
#include "text_window.h"
#include "menu.h"
#include "menu_helpers.h"
#include "list_menu.h"
#include "item.h"
#include "item_menu.h"
#include "menu_helpers.h"
#include "link.h"
#include "money.h"
#include "shop.h"
#include "pokemon_storage_system.h"
#include "party_menu.h"
#include "data.h"
#include "scanline_effect.h"
#include "strings.h"
#include "list_menu.h"
#include "tm_case.h"
#include "constants/items.h"
#include "constants/songs.h"
#include "constants/rgb.h"

static const u32 sTMCaseTiles[] = INCBIN_U32("graphics/tm_case/tiles.4bpp.lz");
static const u32 gUnknown_8E84A24[] = INCBIN_U32("graphics/tm_case/unk_8E84A24.bin.lz");
static const u32 gUnknown_8E84B70[] = INCBIN_U32("graphics/tm_case/unk_8E84B70.bin.lz");
static const u32 sTMCaseMale_Pal[] = INCBIN_U32("graphics/tm_case/male.gbapal.lz");
static const u32 sTMCaseFemale_Pal[] = INCBIN_U32("graphics/tm_case/female.gbapal.lz");
static const u32 sTMCase_DiskSpriteGfx[] = INCBIN_U32("graphics/tm_case/disk.4bpp.lz");
static const u32 sTMCaseTypes_Pal1[] = INCBIN_U32("graphics/tm_case/types1.gbapal.lz");
static const u32 sTMCaseTypes_Pal2[] = INCBIN_U32("graphics/tm_case/types2.gbapal.lz");
static const u8 sTMCaseHMIcon_Gfx[] = INCBIN_U8("graphics/tm_case/hm.4bpp");

static const u16 sTMCaseMainWindowPalette[] = INCBIN_U16("graphics/tm_case/window.gbapal");

static const u8 sText_ClearTo18[] = _("{CLEAR_TO 18}");

static EWRAM_DATA struct TMCaseControl sTMCaseStaticResources = {};
static EWRAM_DATA struct TMCaseData * sTMCaseDynamicResources = NULL;
static EWRAM_DATA void *sTilemapBuffer = NULL; // tilemap buffer
static EWRAM_DATA struct ListMenuItem * sListMenuItemsBuffer = NULL;
static EWRAM_DATA u8 (* sListMenuStringsBuffer)[29] = NULL;
static EWRAM_DATA u16 * sTMSpritePaletteBuffer = NULL;

static void CB2_SetUpTMCaseUI_Blocking(void);
static bool8 DoSetUpTMCaseUI(void);
static void ResetBufferPointers_NoFree(void);
static void LoadBGTemplates(void);
static bool8 HandleLoadTMCaseGraphicsAndPalettes(void);
static void CreateTMCaseListMenuBuffers(void);
static void InitTMCaseListMenuItems(void);
static void GetTMNumberAndMoveString(u8 * dest, u16 itemId);
static void TMCase_MoveCursorFunc(s32 itemIndex, bool8 onInit, struct ListMenu *list);
static void TMCase_ItemPrintFunc(u8 windowId, u32 itemId, u8 y);
static void TMCase_MoveCursor_UpdatePrintedDescription(s32 itemIndex);
static void PrintListMenuCursorAt_WithColorIdx(u8 a0, u8 a1);
static void CreateTMCaseScrollIndicatorArrowPair_Main(void);
static void TMCaseSetup_GetTMCount(void);
static void TMCaseSetup_InitListMenuPositions(void);
static void TMCaseSetup_UpdateVisualMenuOffset(void);
static void Task_FadeOutAndCloseTMCase(u8 taskId);
static void Task_TMCaseMain(u8 taskId);
static void Task_SelectTMAction_FromFieldBag(u8 taskId);
static void Task_TMContextMenu_HandleInput(u8 taskId);
static void TMHMContextMenuAction_Use(u8 taskId);
static void TMHMContextMenuAction_Give(u8 taskId);
static void PrintError_ThereIsNoPokemon(u8 taskId);
static void PrintError_ItemCantBeHeld(u8 taskId);
static void Task_WaitButtonAfterErrorPrint(u8 taskId);
static void Subtask_CloseContextMenuAndReturnToMain(u8 taskId);
static void TMHMContextMenuAction_Exit(u8 taskId);
static void Task_SelectTMAction_Type1(u8 taskId);
static void Task_SelectTMAction_Type3(u8 taskId);
static void Task_SelectTMAction_FromSellMenu(u8 taskId);
static void Task_AskConfirmSaleWithAmount(u8 taskId);
static void Task_PlaceYesNoBox(u8 taskId);
static void Task_SaleOfTMsCanceled(u8 taskId);
static void Task_InitQuantitySelectUI(u8 taskId);
static void SellTM_PrintQuantityAndSalePrice(s16 quantity, s32 value);
static void Task_QuantitySelect_HandleInput(u8 taskId);
static void Task_PrintSaleConfirmedText(u8 taskId);
static void Task_DoSaleOfTMs(u8 taskId);
static void Task_AfterSale_ReturnToList(u8 taskId);
static void Task_TMCaseDude1(u8 taskId);
static void Task_TMCaseDude_Playback(u8 taskId);
static void InitTMCaseWindows(void);
static void AddTextPrinterParameterized_ColorByIndex(u8 windowId, u8 fontId, const u8 * str, u8 x, u8 y, u8 letterSpacing, u8 lineSpacing, u8 speed, u8 colorIdx);
static void TMCase_SetWindowBorder1(u8 windowId);
static void TMCase_SetWindowBorder2(u8 windowId);
static void TMCase_PrintMessageWithFollowupTask(u8 taskId, u8 fontId, const u8 * str, TaskFunc func);
static void PrintStringTMCaseOnWindow3(void);
static void DrawMoveInfoUIMarkers(void);
static void TMCase_MoveCursor_UpdatePrintedTMInfo(u16 itemId);
static void PlaceHMTileInWindow(u8 windowId, u8 x, u8 y);
static void HandlePrintMoneyOnHand(void);
static void HandleCreateYesNoMenu(u8 taskId, const struct YesNoFuncTable * ptrs);
static u8 AddTMContextMenu(u8 * a0, u8 a1);
static void RemoveTMContextMenu(u8 * a0);
static u8 CreateTMSprite(u16 itemId);
static void SetTMSpriteAnim(struct Sprite *sprite, u8 var);
static void TintTMSpriteByType(u8 type);
static void UpdateTMSpritePosition(struct Sprite *sprite, u8 var);
static void InitSelectedTMSpriteData(u8 a0, u16 itemId);
static void SpriteCB_MoveTMSpriteInCase(struct Sprite *sprite);
static void LoadTMTypePalettes(void);

static const struct BgTemplate sBGTemplates[] = {
    {
        .bg = 0,
        .charBaseIndex = 0,
        .mapBaseIndex = 31,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 1,
        .baseTile = 0x000
    }, 
    {
        .bg = 1,
        .charBaseIndex = 0,
        .mapBaseIndex = 30,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 0,
        .baseTile = 0x000
    }, 
    {
        .bg = 2,
        .charBaseIndex = 0,
        .mapBaseIndex = 29,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 2,
        .baseTile = 0x000
    }
};

static void (*const sSelectTMActionTasks[])(u8 taskId) = {
    Task_SelectTMAction_FromFieldBag,
    Task_SelectTMAction_Type1,
    Task_SelectTMAction_FromSellMenu,
    Task_SelectTMAction_Type3
};

enum
{
    ACTION_USE,
    ACTION_GIVE,
    ACTION_EXIT
};

static const struct MenuAction sMenuActions_UseGiveExit[] = {
    [ACTION_USE] =  { gMenuText_Use,  TMHMContextMenuAction_Use  },
    [ACTION_GIVE] = { gMenuText_Give, TMHMContextMenuAction_Give },
    [ACTION_EXIT] = { gText_MenuExit, TMHMContextMenuAction_Exit },
};

static const u8 sMenuActionIndices_Field[] = {
    ACTION_USE, 
    ACTION_GIVE, 
    ACTION_EXIT
};

static const u8 sMenuActionIndices_UnionRoom[] = {
    ACTION_GIVE, 
    ACTION_EXIT
};

static const struct YesNoFuncTable sYesNoFuncTable = {
    .yesFunc = Task_PrintSaleConfirmedText, 
    .noFunc = Task_SaleOfTMsCanceled
};

static const u16 sPal3Override[] = {
    RGB(8, 8, 8), 
    RGB(30, 16, 6)
};

static const u8 sTextColors[][3] = {
    {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_WHITE, TEXT_COLOR_DARK_GRAY},
    {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_DARK_GRAY, TEXT_COLOR_LIGHT_GRAY},
    {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_LIGHT_GRAY, TEXT_COLOR_GREEN},
    {TEXT_COLOR_TRANSPARENT, TEXT_DYNAMIC_COLOR_5, TEXT_DYNAMIC_COLOR_1}
};

enum {
    WIN_LIST,
    WIN_INFO,
    WIN_MSG,
    WIN_LABEL,
    WIN_UI_MARKERS,
    WIN_MOVE_INFO,
    WIN_CASHIER_MSG,
    WIN_QUANTITY,
    WIN_MONEYBOX,
    WIN_DUMMY
};

static const struct WindowTemplate sWindowTemplates[] = {
    [WIN_LIST] = {
        .bg = 0, 
        .tilemapLeft = 10, 
        .tilemapTop = 1, 
        .width = 19, 
        .height = 10, 
        .paletteNum = 15, 
        .baseBlock = 129
    },
    [WIN_INFO] = {
        .bg = 0, 
        .tilemapLeft = 12, 
        .tilemapTop = 12, 
        .width = 18, 
        .height = 8, 
        .paletteNum = 10, 
        .baseBlock = 319
    },
    [WIN_MSG] = {
        .bg = 1, 
        .tilemapLeft = 5, 
        .tilemapTop = 15, 
        .width = 15, 
        .height = 4, 
        .paletteNum = 15, 
        .baseBlock = 505
    },
    [WIN_LABEL] = {
        .bg = 0, 
        .tilemapLeft = 0, 
        .tilemapTop = 1, 
        .width = 10, 
        .height = 2, 
        .paletteNum = 15, 
        .baseBlock = 565
    },
    [WIN_UI_MARKERS] = {
        .bg = 0, 
        .tilemapLeft = 1, 
        .tilemapTop = 13, 
        .width = 5, 
        .height = 6, 
        .paletteNum = 12, 
        .baseBlock = 585
    },
    [WIN_MOVE_INFO] = {
        .bg = 0, 
        .tilemapLeft = 7, 
        .tilemapTop = 13, 
        .width = 5, 
        .height = 6, 
        .paletteNum = 12, 
        .baseBlock = 615
    },
    [WIN_CASHIER_MSG] = {
        .bg = 1, 
        .tilemapLeft = 2, 
        .tilemapTop = 15, 
        .width = 0x1a, 
        .height = 4, 
        .paletteNum = 11, 
        .baseBlock = 645
    },
    [WIN_QUANTITY] = {
        .bg = 1, 
        .tilemapLeft = 17, 
        .tilemapTop = 9, 
        .width = 12, 
        .height = 4, 
        .paletteNum = 15, 
        .baseBlock = 749
    },
    [WIN_MONEYBOX] = {
        .bg = 1, 
        .tilemapLeft = 1, 
        .tilemapTop = 1, 
        .width = 10, 
        .height = 2, 
        .paletteNum = 15, 
        .baseBlock = 797
    },
    DUMMY_WIN_TEMPLATE
};

static const struct WindowTemplate sYesNoWindowTemplate = {
    .bg = 1, 
    .tilemapLeft = 21, 
    .tilemapTop = 9, 
    .width = 6, 
    .height = 4, 
    .paletteNum = 15, 
    .baseBlock = 463
};

static const struct WindowTemplate sTMContextWindowTemplates[] = {
    {
        .bg = 1, 
        .tilemapLeft = 24, 
        .tilemapTop = 13, 
        .width = 5, 
        .height = 6, 
        .paletteNum = 15, 
        .baseBlock = 463
    },
    {
        .bg = 1, 
        .tilemapLeft = 24, 
        .tilemapTop = 13, 
        .width = 5, 
        .height = 4, 
        .paletteNum = 15, 
        .baseBlock = 463
    }
};

static const struct OamData sTMSpriteOamData = {
    .size = 2,
    .priority = 2
};

static const union AnimCmd sTMSpriteAnim0[] = {
    ANIMCMD_FRAME(0, 0),
    ANIMCMD_END
};

static const union AnimCmd sTMSpriteAnim1[] = {
    ANIMCMD_FRAME(16, 0),
    ANIMCMD_END
};

static const union AnimCmd *const sTMSpriteAnims[] = {
    sTMSpriteAnim0,
    sTMSpriteAnim1
};

static const struct CompressedSpriteSheet sTMSpriteSheet = {
    (const void *)sTMCase_DiskSpriteGfx,
    0x400,
    TM_CASE_TM_TAG
};

static const struct SpriteTemplate sTMSpriteTemplate = {
    TM_CASE_TM_TAG,
    TM_CASE_TM_TAG,
    &sTMSpriteOamData,
    sTMSpriteAnims,
    NULL,
    gDummySpriteAffineAnimTable,
    SpriteCallbackDummy
};

static const u16 sTMSpritePaletteOffsetByType[] = {
    [TYPE_NORMAL]   = 0x000,
    [TYPE_FIRE]     = 0x010,
    [TYPE_WATER]    = 0x020,
    [TYPE_GRASS]    = 0x030,
    [TYPE_ELECTRIC] = 0x040,
    [TYPE_ROCK]     = 0x050,
    [TYPE_GROUND]   = 0x060,
    [TYPE_ICE]      = 0x070,
    [TYPE_FLYING]   = 0x080,
    [TYPE_FIGHTING] = 0x090,
    [TYPE_GHOST]    = 0x0a0,
    [TYPE_BUG]      = 0x0b0,
    [TYPE_POISON]   = 0x0c0,
    [TYPE_PSYCHIC]  = 0x0d0,
    [TYPE_STEEL]    = 0x0e0,
    [TYPE_DARK]     = 0x0f0,
    [TYPE_DRAGON]   = 0x100
};

void OpenTMCase(u8 type, void (* callback)(void), u8 a2)
{
    ResetBufferPointers_NoFree();
    sTMCaseDynamicResources = Alloc(sizeof(struct TMCaseData));
    sTMCaseDynamicResources->savedCallback = 0;
    sTMCaseDynamicResources->scrollIndicatorArrowPairId = 0xFF;
    sTMCaseDynamicResources->contextMenuWindowId = 0xFF;
    if (type != 5)
        sTMCaseStaticResources.tmCaseMenuType = type;
    if (callback != NULL)
        sTMCaseStaticResources.savedCallback = callback;
    if (a2 != 0xFF)
        sTMCaseStaticResources.unk_05 = a2;
    gTextFlags.autoScroll = FALSE;
    SetMainCallback2(CB2_SetUpTMCaseUI_Blocking);
}

static void CB2_Idle(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    DoScheduledBgTilemapCopiesToVram();
    UpdatePaletteFade();
}

static void VBlankCB_Idle(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

static void CB2_SetUpTMCaseUI_Blocking(void)
{
    while (1)
    {
        if (IsActiveOverworldLinkBusy() == TRUE)
            break;
        if (DoSetUpTMCaseUI() == TRUE)
            break;
        if (MenuHelpers_IsLinkActive() == TRUE)
            break;
    }
}

static bool8 DoSetUpTMCaseUI(void)
{
    u8 taskId;

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
        LoadBGTemplates();
        sTMCaseDynamicResources->seqId = 0;
        gMain.state++;
        break;
    case 7:
        InitTMCaseWindows();
        gMain.state++;
        break;
    case 8:
        if (HandleLoadTMCaseGraphicsAndPalettes())
            gMain.state++;
        break;
    case 9:
        SortBerriesOrTMHMs(&gBagPockets[POCKET_TM_HM - 1]);
        gMain.state++;
        break;
    case 10:
        TMCaseSetup_GetTMCount();
        TMCaseSetup_InitListMenuPositions();
        TMCaseSetup_UpdateVisualMenuOffset();
        gMain.state++;
        break;
    case 11:
        DrawMoveInfoUIMarkers();
        gMain.state++;
        break;
    case 12:
        CreateTMCaseListMenuBuffers();
        InitTMCaseListMenuItems();
        gMain.state++;
        break;
    case 13:
        PrintStringTMCaseOnWindow3();
        gMain.state++;
        break;
    case 14:
        taskId = CreateTask(Task_TMCaseMain, 0);
        gTasks[taskId].data[0] = ListMenuInit(&gMultiuseListMenuTemplate, sTMCaseStaticResources.scrollOffset, sTMCaseStaticResources.selectedRow);
        gMain.state++;
        break;
    case 15:
        CreateTMCaseScrollIndicatorArrowPair_Main();
        gMain.state++;
        break;
    case 16:
        sTMCaseDynamicResources->tmSpriteId = CreateTMSprite(BagGetItemIdByPocketPosition(POCKET_TM_HM, sTMCaseStaticResources.scrollOffset + sTMCaseStaticResources.selectedRow));
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
    default:
        SetVBlankCallback(VBlankCB_Idle);
        SetMainCallback2(CB2_Idle);
        return TRUE;
    }

    return FALSE;
}

static void ResetBufferPointers_NoFree(void)
{
    sTMCaseDynamicResources = NULL;
    sTilemapBuffer = NULL;
    sListMenuItemsBuffer = NULL;
    sListMenuStringsBuffer = NULL;
    sTMSpritePaletteBuffer = NULL;
}

static void LoadBGTemplates(void)
{
    void ** ptr;
    ResetVramOamAndBgCntRegs();
    ResetAllBgsCoordinates();
    ptr = &sTilemapBuffer;
    *ptr = AllocZeroed(0x800);
    ResetBgsAndClearDma3BusyFlags(0);
    InitBgsFromTemplates(0, sBGTemplates, NELEMS(sBGTemplates));
    SetBgTilemapBuffer(2, *ptr);
    ScheduleBgCopyTilemapToVram(1);
    ScheduleBgCopyTilemapToVram(2);
    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_OBJ_1D_MAP | DISPCNT_OBJ_ON);
    SetGpuReg(REG_OFFSET_BLDCNT, 0);
    ShowBg(0);
    ShowBg(1);
    ShowBg(2);
}

static bool8 HandleLoadTMCaseGraphicsAndPalettes(void)
{
    switch (sTMCaseDynamicResources->seqId)
    {
    case 0:
        ResetTempTileDataBuffers();
        DecompressAndCopyTileDataToVram(1, sTMCaseTiles, 0, 0, 0);
        sTMCaseDynamicResources->seqId++;
        break;
    case 1:
        if (FreeTempTileDataBuffersIfPossible() != TRUE)
        {
            LZDecompressWram(gUnknown_8E84A24, sTilemapBuffer);
            sTMCaseDynamicResources->seqId++;
        }
        break;
    case 2:
        LZDecompressWram(gUnknown_8E84B70, GetBgTilemapBuffer(1));
        sTMCaseDynamicResources->seqId++;
        break;
    case 3:
        if (gSaveBlock2Ptr->playerGender == MALE)
            LoadCompressedPalette(sTMCaseMale_Pal, 0, 0x80);
        else
            LoadCompressedPalette(sTMCaseFemale_Pal, 0, 0x80);
        sTMCaseDynamicResources->seqId++;
        break;
    case 4:
        LoadCompressedSpriteSheet(&sTMSpriteSheet);
        sTMCaseDynamicResources->seqId++;
        break;
    default:
        LoadTMTypePalettes();
        sTMCaseDynamicResources->seqId = 0;
        return TRUE;
    }

    return FALSE;
}

static void CreateTMCaseListMenuBuffers(void)
{
    struct BagPocket * pocket = &gBagPockets[POCKET_TM_HM - 1];
    sListMenuItemsBuffer = Alloc((pocket->capacity + 1) * sizeof(struct ListMenuItem));
    sListMenuStringsBuffer = Alloc(sTMCaseDynamicResources->numTMs * 29);
}

static const u8 gText_Close[] = _("Close");

static void InitTMCaseListMenuItems(void)
{
    struct BagPocket * pocket = &gBagPockets[POCKET_TM_HM - 1];
    u16 i;

    for (i = 0; i < sTMCaseDynamicResources->numTMs; i++)
    {
        GetTMNumberAndMoveString(sListMenuStringsBuffer[i], pocket->itemSlots[i].itemId);
        sListMenuItemsBuffer[i].name = sListMenuStringsBuffer[i];
        sListMenuItemsBuffer[i].id = i;
    }
    sListMenuItemsBuffer[i].name = gText_Close;
    sListMenuItemsBuffer[i].id = -2;
    gMultiuseListMenuTemplate.items = sListMenuItemsBuffer;
    gMultiuseListMenuTemplate.totalItems = sTMCaseDynamicResources->numTMs + 1;
    gMultiuseListMenuTemplate.windowId = 0;
    gMultiuseListMenuTemplate.header_X = 0;
    gMultiuseListMenuTemplate.item_X = 8;
    gMultiuseListMenuTemplate.cursor_X = 0;
    gMultiuseListMenuTemplate.lettersSpacing = 0;
    gMultiuseListMenuTemplate.itemVerticalPadding = 2;
    gMultiuseListMenuTemplate.upText_Y = 2;
    gMultiuseListMenuTemplate.maxShowed = sTMCaseDynamicResources->maxTMsShown;
    gMultiuseListMenuTemplate.fontId = FONT_SHORT;
    gMultiuseListMenuTemplate.cursorPal = 2;
    gMultiuseListMenuTemplate.fillValue = 0;
    gMultiuseListMenuTemplate.cursorShadowPal = 3;
    gMultiuseListMenuTemplate.moveCursorFunc = TMCase_MoveCursorFunc;
    gMultiuseListMenuTemplate.itemPrintFunc = TMCase_ItemPrintFunc;
    gMultiuseListMenuTemplate.cursorKind = 0;
    gMultiuseListMenuTemplate.scrollMultiple = 0;
}

const u8 gText_FontSize0[] = _("{FONT_SMALL}");
const u8 gText_FontSize2[] = _("{FONT_SHORT}");

static void GetTMNumberAndMoveString(u8 * dest, u16 itemId)
{
    StringCopy(gStringVar4, gText_FontSize0);
    if (itemId >= ITEM_HM01)
    {
        StringAppend(gStringVar4, sText_ClearTo18);
        StringAppend(gStringVar4, gText_NumberClear01);
        ConvertIntToDecimalStringN(gStringVar1, itemId - ITEM_HM01 + 1, STR_CONV_MODE_LEADING_ZEROS, 1);
        StringAppend(gStringVar4, gStringVar1);
    }
    else
    {
        StringAppend(gStringVar4, gText_NumberClear01);
        ConvertIntToDecimalStringN(gStringVar1, itemId - ITEM_TM01 + 1, STR_CONV_MODE_LEADING_ZEROS, 2);
        StringAppend(gStringVar4, gStringVar1);
    }
    StringAppend(gStringVar4, gText_Space);
    StringAppend(gStringVar4, gText_FontSize2);
    StringAppend(gStringVar4, gMoveNames[ItemIdToBattleMoveId(itemId)]);
    StringCopy(dest, gStringVar4);
}

static void TMCase_MoveCursorFunc(s32 itemIndex, bool8 onInit, struct ListMenu *list)
{
    u16 itemId;

    if (itemIndex == -2)
        itemId = 0;
    else
        itemId = BagGetItemIdByPocketPosition(POCKET_TM_HM, itemIndex);

    if (onInit != TRUE)
    {
        PlaySE(SE_SELECT);
        InitSelectedTMSpriteData(sTMCaseDynamicResources->tmSpriteId, itemId);
    }
    TMCase_MoveCursor_UpdatePrintedDescription(itemIndex);
    TMCase_MoveCursor_UpdatePrintedTMInfo(itemId);
}

static void TMCase_ItemPrintFunc(u8 windowId, u32 itemId, u8 y)
{
    if (itemId != -2)
    {
        if (!ItemId_GetImportance(BagGetItemIdByPocketPosition(POCKET_TM_HM, itemId)))
        {
            ConvertIntToDecimalStringN(gStringVar1, BagGetQuantityByPocketPosition(POCKET_TM_HM, itemId), STR_CONV_MODE_RIGHT_ALIGN, 3);
            StringExpandPlaceholders(gStringVar4, gText_xVar1);
            AddTextPrinterParameterized_ColorByIndex(windowId, FONT_SMALL, gStringVar4, 0x7E, y, 0, 0, 0xFF, 1);
        }
        else
        {
            PlaceHMTileInWindow(windowId, 8, y);
        }
    }
}

const u8 gText_TMCaseWillBePutAway[] = _("The TM Case will be\nput away.");
const u8 gText_TMCase[] = _("TM Case");

static void TMCase_MoveCursor_UpdatePrintedDescription(s32 itemIndex)
{
    const u8 * str;
    if (itemIndex != -2)
    {
        str = ItemId_GetDescription(BagGetItemIdByPocketPosition(POCKET_TM_HM, itemIndex));
    }
    else
    {
        str = gText_TMCaseWillBePutAway;
    }
    FillWindowPixelBuffer(WIN_INFO, 0);
    AddTextPrinterParameterized_ColorByIndex(WIN_INFO, FONT_SHORT, str, 2, 3, 1, 0, 0, 0);
}

static void FillBG2RowWithPalette_2timesNplus1(s32 a0)
{
    SetBgTilemapPalette(2, 0, 12, 30, 8, 2 * a0 + 1);
    ScheduleBgCopyTilemapToVram(2);
}

static void PrintListMenuCursorByID_WithColorIdx(u8 a0, u8 a1)
{
    PrintListMenuCursorAt_WithColorIdx(ListMenuGetYCoordForPrintingArrowCursor(a0), a1);
}

static void PrintListMenuCursorAt_WithColorIdx(u8 a0, u8 a1)
{
    if (a1 == 0xFF)
    {
        FillWindowPixelRect(WIN_LIST, 0, 0, a0, GetFontAttribute(FONT_SHORT, FONTATTR_MAX_LETTER_WIDTH), GetFontAttribute(FONT_SHORT, FONTATTR_MAX_LETTER_HEIGHT));
        CopyWindowToVram(WIN_LIST, COPYWIN_GFX);
    }
    else
    {
        AddTextPrinterParameterized_ColorByIndex(WIN_LIST, FONT_SHORT, gText_SelectorArrow2, 0, a0, 0, 0, 0, a1);
    }
}

static void CreateTMCaseScrollIndicatorArrowPair_Main(void)
{
    sTMCaseDynamicResources->scrollIndicatorArrowPairId = AddScrollIndicatorArrowPairParameterized(WIN_MSG, 0xA0, 0x08, 0x58, sTMCaseDynamicResources->numTMs - sTMCaseDynamicResources->maxTMsShown + 1, 0x6E, 0x6E, &sTMCaseStaticResources.scrollOffset);
}

static void CreateTMCaseScrollIndicatorArrowPair_SellQuantitySelect(void)
{
    sTMCaseDynamicResources->currItem = 1;
    sTMCaseDynamicResources->scrollIndicatorArrowPairId = AddScrollIndicatorArrowPairParameterized(WIN_MSG, 0x98, 0x48, 0x68, 2, 0x6E, 0x6E, &sTMCaseDynamicResources->currItem);
}

static void RemoveTMCaseScrollIndicatorArrowPair(void)
{
    if (sTMCaseDynamicResources->scrollIndicatorArrowPairId != 0xFF)
    {
        RemoveScrollIndicatorArrowPair(sTMCaseDynamicResources->scrollIndicatorArrowPairId);
        sTMCaseDynamicResources->scrollIndicatorArrowPairId = 0xFF;
    }
}

void ResetTMCaseCursorPos(void)
{
    sTMCaseStaticResources.selectedRow = 0;
    sTMCaseStaticResources.scrollOffset = 0;
}

static void TMCaseSetup_GetTMCount(void)
{
    struct BagPocket * pocket = &gBagPockets[POCKET_TM_HM - 1];
    u16 i;

    // BagPocketCompaction(pocket->itemSlots, pocket->capacity);
    CompactItemsInBagPocket(pocket);
    sTMCaseDynamicResources->numTMs = 0;
    for (i = 0; i < pocket->capacity; i++)
    {
        if (pocket->itemSlots[i].itemId == ITEM_NONE)
            break;
        sTMCaseDynamicResources->numTMs++;
    }
    sTMCaseDynamicResources->maxTMsShown = min(sTMCaseDynamicResources->numTMs + 1, 5);
}

static void TMCaseSetup_InitListMenuPositions(void)
{
    if (sTMCaseStaticResources.scrollOffset != 0)
    {
        if (sTMCaseStaticResources.scrollOffset + sTMCaseDynamicResources->maxTMsShown > sTMCaseDynamicResources->numTMs + 1)
            sTMCaseStaticResources.scrollOffset = sTMCaseDynamicResources->numTMs + 1 - sTMCaseDynamicResources->maxTMsShown;
    }
    if (sTMCaseStaticResources.scrollOffset + sTMCaseStaticResources.selectedRow >= sTMCaseDynamicResources->numTMs + 1)
    {
        if (sTMCaseDynamicResources->numTMs + 1 < 2)
            sTMCaseStaticResources.selectedRow = 0;
        else
            sTMCaseStaticResources.selectedRow = sTMCaseDynamicResources->numTMs;
    }
}

static void TMCaseSetup_UpdateVisualMenuOffset(void)
{
    u8 i;
    if (sTMCaseStaticResources.selectedRow > 3)
    {
        for (i = 0; i <= sTMCaseStaticResources.selectedRow - 3 && sTMCaseStaticResources.scrollOffset + sTMCaseDynamicResources->maxTMsShown != sTMCaseDynamicResources->numTMs + 1; i++)
        {
            do {} while (0);
            sTMCaseStaticResources.selectedRow--;
            sTMCaseStaticResources.scrollOffset++;
        }
    }
}

static void DestroyTMCaseBuffers(void)
{
    if (sTMCaseDynamicResources != NULL)
        Free(sTMCaseDynamicResources);
    if (sTilemapBuffer != NULL)
        Free(sTilemapBuffer);
    if (sListMenuItemsBuffer != NULL)
        Free(sListMenuItemsBuffer);
    if (sListMenuStringsBuffer != NULL)
        Free(sListMenuStringsBuffer);
    if (sTMSpritePaletteBuffer != NULL)
        Free(sTMSpritePaletteBuffer);
    FreeAllWindowBuffers();
}

static void Task_BeginFadeOutFromTMCase(u8 taskId)
{
    BeginNormalPaletteFade(PALETTES_ALL, -2, 0, 16, RGB_BLACK);
    gTasks[taskId].func = Task_FadeOutAndCloseTMCase;
}

static void Task_FadeOutAndCloseTMCase(u8 taskId)
{
    s16 * data = gTasks[taskId].data;

    if (!gPaletteFade.active)
    {
        DestroyListMenuTask(data[0], &sTMCaseStaticResources.scrollOffset, &sTMCaseStaticResources.selectedRow);
        if (sTMCaseDynamicResources->savedCallback != NULL)
            SetMainCallback2(sTMCaseDynamicResources->savedCallback);
        else
            SetMainCallback2(sTMCaseStaticResources.savedCallback);
        RemoveTMCaseScrollIndicatorArrowPair();
        DestroyTMCaseBuffers();
        DestroyTask(taskId);
    }
}

static void Task_TMCaseMain(u8 taskId)
{
    s16 * data = gTasks[taskId].data;
    s32 input;

    if (!gPaletteFade.active)
    {
        if (IsActiveOverworldLinkBusy() != TRUE)
        {
            input = ListMenu_ProcessInput(data[0]);
            ListMenuGetScrollAndRow(data[0], &sTMCaseStaticResources.scrollOffset, &sTMCaseStaticResources.selectedRow);
            if (JOY_NEW(SELECT_BUTTON) && sTMCaseStaticResources.unk_05 == 1)
            {
                PlaySE(SE_SELECT);
                gSpecialVar_ItemId = ITEM_NONE;
                Task_BeginFadeOutFromTMCase(taskId);
            }
            else
            {
                switch (input)
                {
                case -1:
                    break;
                case -2:
                    PlaySE(SE_SELECT);
                    gSpecialVar_ItemId = 0;
                    Task_BeginFadeOutFromTMCase(taskId);
                    break;
                default:
                    PlaySE(SE_SELECT);
                    FillBG2RowWithPalette_2timesNplus1(1);
                    RemoveTMCaseScrollIndicatorArrowPair();
                    PrintListMenuCursorByID_WithColorIdx(data[0], 2);
                    data[1] = input;
                    data[2] = BagGetQuantityByPocketPosition(POCKET_TM_HM, input);
                    gSpecialVar_ItemId = BagGetItemIdByPocketPosition(POCKET_TM_HM, input);
                    gTasks[taskId].func = sSelectTMActionTasks[sTMCaseStaticResources.tmCaseMenuType];
                    break;
                }
            }
        }
    }
}

static void Subtask_ReturnToTMCaseMain(u8 taskId)
{
    FillBG2RowWithPalette_2timesNplus1(0);
    CreateTMCaseScrollIndicatorArrowPair_Main();
    gTasks[taskId].func = Task_TMCaseMain;
}

static void Task_SelectTMAction_FromFieldBag(u8 taskId)
{
    u8 * strbuf;
    TMCase_SetWindowBorder2(2);
    if (!MenuHelpers_IsLinkActive() && InUnionRoom() != TRUE)
    {
        AddTMContextMenu(&sTMCaseDynamicResources->contextMenuWindowId, 0);
        sTMCaseDynamicResources->menuActionIndices = sMenuActionIndices_Field;
        sTMCaseDynamicResources->numMenuActions = NELEMS(sMenuActionIndices_Field);
    }
    else
    {
        AddTMContextMenu(&sTMCaseDynamicResources->contextMenuWindowId, 1);
        sTMCaseDynamicResources->menuActionIndices = sMenuActionIndices_UnionRoom;
        sTMCaseDynamicResources->numMenuActions = NELEMS(sMenuActionIndices_UnionRoom);
    }
    PrintMenuActionTexts(sTMCaseDynamicResources->contextMenuWindowId, FONT_SHORT, GetMenuCursorDimensionByFont(FONT_SHORT, 0), 2, 0, GetFontAttribute(FONT_SHORT, FONTATTR_MAX_LETTER_HEIGHT) + 2, sTMCaseDynamicResources->numMenuActions, sMenuActions_UseGiveExit, sTMCaseDynamicResources->menuActionIndices);
    InitMenuNormal(sTMCaseDynamicResources->contextMenuWindowId, FONT_SHORT, 0, 2, GetFontAttribute(FONT_SHORT, FONTATTR_MAX_LETTER_HEIGHT) + 2, sTMCaseDynamicResources->numMenuActions, 0);
    strbuf = Alloc(256);
    GetTMNumberAndMoveString(strbuf, gSpecialVar_ItemId);
    StringAppend(strbuf, gText_Var1IsSelected + 2); // +2 skips over the stringvar
    AddTextPrinterParameterized_ColorByIndex(WIN_MSG, FONT_SHORT, strbuf, 0, 2, 1, 0, 0, 1);
    Free(strbuf);
    if (ItemId_GetImportance(gSpecialVar_ItemId))
    {
        PlaceHMTileInWindow(WIN_MSG, 0, 2);
        CopyWindowToVram(WIN_MSG, COPYWIN_GFX);
    }
    ScheduleBgCopyTilemapToVram(0);
    ScheduleBgCopyTilemapToVram(1);
    gTasks[taskId].func = Task_TMContextMenu_HandleInput;
}

static void Task_TMContextMenu_HandleInput(u8 taskId)
{
    s8 input;

    if (IsActiveOverworldLinkBusy() != TRUE)
    {
        input = Menu_ProcessInputNoWrap();
        switch (input)
        {
        case -1:
            PlaySE(SE_SELECT);
            sMenuActions_UseGiveExit[sTMCaseDynamicResources->menuActionIndices[sTMCaseDynamicResources->numMenuActions - 1]].func.void_u8(taskId);
            break;
        case -2:
            break;
        default:
            PlaySE(SE_SELECT);
            sMenuActions_UseGiveExit[sTMCaseDynamicResources->menuActionIndices[input]].func.void_u8(taskId);
            break;
        }
    }
}

static void TMHMContextMenuAction_Use(u8 taskId)
{
    RemoveTMContextMenu(&sTMCaseDynamicResources->contextMenuWindowId);
    ClearStdWindowAndFrameToTransparent(WIN_MSG, FALSE);
    ClearWindowTilemap(2);
    PutWindowTilemap(0);
    ScheduleBgCopyTilemapToVram(0);
    ScheduleBgCopyTilemapToVram(1);
    if (CalculatePlayerPartyCount() == 0)
    {
        PrintError_ThereIsNoPokemon(taskId);
    }
    else
    {
        gItemUseCB = ItemUseCB_TMHM;
        sTMCaseDynamicResources->savedCallback = CB2_ShowPartyMenuForItemUse;
        Task_BeginFadeOutFromTMCase(taskId);
    }
}

static void TMHMContextMenuAction_Give(u8 taskId)
{
    s16 * data = gTasks[taskId].data;
    u16 itemId = BagGetItemIdByPocketPosition(POCKET_TM_HM, data[1]);
    RemoveTMContextMenu(&sTMCaseDynamicResources->contextMenuWindowId);
    ClearStdWindowAndFrameToTransparent(WIN_MSG, FALSE);
    ClearWindowTilemap(2);
    PutWindowTilemap(1);
    PutWindowTilemap(4);
    PutWindowTilemap(5);
    ScheduleBgCopyTilemapToVram(0);
    ScheduleBgCopyTilemapToVram(1);
    if (!ItemId_GetImportance(itemId))
    {
        if (CalculatePlayerPartyCount() == 0)
        {
            PrintError_ThereIsNoPokemon(taskId);
        }
        else
        {
            sTMCaseDynamicResources->savedCallback = CB2_ChooseMonToGiveItem;
            Task_BeginFadeOutFromTMCase(taskId);
        }
    }
    else
    {
        PrintError_ItemCantBeHeld(taskId);
    }
}

static void PrintError_ThereIsNoPokemon(u8 taskId)
{
    TMCase_PrintMessageWithFollowupTask(taskId, FONT_NORMAL, gText_NoPokemon, Task_WaitButtonAfterErrorPrint);
}

static void PrintError_ItemCantBeHeld(u8 taskId)
{
    CopyItemName(gSpecialVar_ItemId, gStringVar1);
    StringExpandPlaceholders(gStringVar4, gText_Var1CantBeHeld);
    TMCase_PrintMessageWithFollowupTask(taskId, FONT_NORMAL, gStringVar4, Task_WaitButtonAfterErrorPrint);
}

static void Task_WaitButtonAfterErrorPrint(u8 taskId)
{
    if (JOY_NEW(A_BUTTON))
    {
        PlaySE(SE_SELECT);
        Subtask_CloseContextMenuAndReturnToMain(taskId);
    }
}

static void Subtask_CloseContextMenuAndReturnToMain(u8 taskId)
{
    s16 * data = gTasks[taskId].data;

    DestroyListMenuTask(data[0], &sTMCaseStaticResources.scrollOffset, &sTMCaseStaticResources.selectedRow);
    data[0] = ListMenuInit(&gMultiuseListMenuTemplate, sTMCaseStaticResources.scrollOffset, sTMCaseStaticResources.selectedRow);
    PrintListMenuCursorByID_WithColorIdx(data[0], 1);
    ClearDialogWindowAndFrameToTransparent(WIN_CASHIER_MSG, FALSE);
    ClearWindowTilemap(6);
    PutWindowTilemap(1);
    PutWindowTilemap(4);
    PutWindowTilemap(5);
    ScheduleBgCopyTilemapToVram(0);
    ScheduleBgCopyTilemapToVram(1);
    Subtask_ReturnToTMCaseMain(taskId);
}

static void TMHMContextMenuAction_Exit(u8 taskId)
{
    s16 * data = gTasks[taskId].data;

    RemoveTMContextMenu(&sTMCaseDynamicResources->contextMenuWindowId);
    ClearStdWindowAndFrameToTransparent(WIN_MSG, FALSE);
    ClearWindowTilemap(2);
    PutWindowTilemap(0);
    PrintListMenuCursorByID_WithColorIdx(data[0], 1);
    PutWindowTilemap(1);
    PutWindowTilemap(4);
    PutWindowTilemap(5);
    ScheduleBgCopyTilemapToVram(0);
    ScheduleBgCopyTilemapToVram(1);
    Subtask_ReturnToTMCaseMain(taskId);
}

static void Task_SelectTMAction_Type1(u8 taskId)
{
    s16 * data = gTasks[taskId].data;

    if (!ItemId_GetImportance(BagGetItemIdByPocketPosition(POCKET_TM_HM, data[1])))
    {
        sTMCaseDynamicResources->savedCallback = CB2_GiveHoldItem;
        Task_BeginFadeOutFromTMCase(taskId);
    }
    else
    {
        PrintError_ItemCantBeHeld(taskId);
    }
}

static void Task_SelectTMAction_Type3(u8 taskId)
{
    s16 * data = gTasks[taskId].data;

    if (!ItemId_GetImportance(BagGetItemIdByPocketPosition(POCKET_TM_HM, data[1])))
    {
        sTMCaseDynamicResources->savedCallback = CB2_ReturnToPokeStorage;
        Task_BeginFadeOutFromTMCase(taskId);
    }
    else
    {
        PrintError_ItemCantBeHeld(taskId);
    }
}

static void Task_SelectTMAction_FromSellMenu(u8 taskId)
{
    s16 * data = gTasks[taskId].data;

    if (ItemId_GetPrice(gSpecialVar_ItemId) == 0)
    {
        CopyItemName(gSpecialVar_ItemId, gStringVar1);
        StringExpandPlaceholders(gStringVar4, gText_CantBuyKeyItem);
        TMCase_PrintMessageWithFollowupTask(taskId, FONT_NORMAL, gStringVar4, Subtask_CloseContextMenuAndReturnToMain);
    }
    else
    {
        data[8] = 1;
        if (data[2] == 1)
        {
            HandlePrintMoneyOnHand();
            Task_AskConfirmSaleWithAmount(taskId);
        }
        else
        {
            if (data[2] > 99)
                data[2] = 99;
            CopyItemName(gSpecialVar_ItemId, gStringVar1);
            StringExpandPlaceholders(gStringVar4, gText_HowManyToSell);
            TMCase_PrintMessageWithFollowupTask(taskId, FONT_NORMAL, gStringVar4, Task_InitQuantitySelectUI);
        }
    }
}

static void Task_AskConfirmSaleWithAmount(u8 taskId)
{
    s16 * data = gTasks[taskId].data;

    ConvertIntToDecimalStringN(gStringVar3, ItemId_GetPrice(BagGetItemIdByPocketPosition(POCKET_TM_HM, data[1])) / 2 * data[8], STR_CONV_MODE_LEFT_ALIGN, 6);
    StringExpandPlaceholders(gStringVar4, gText_ICanPayVar1);
    TMCase_PrintMessageWithFollowupTask(taskId, FONT_NORMAL, gStringVar4, Task_PlaceYesNoBox);
}

static void Task_PlaceYesNoBox(u8 taskId)
{
    HandleCreateYesNoMenu(taskId, &sYesNoFuncTable);
}

static void Task_SaleOfTMsCanceled(u8 taskId)
{
    s16 * data = gTasks[taskId].data;

    ClearStdWindowAndFrameToTransparent(WIN_MONEYBOX, FALSE);
    RemoveMoneyLabelObject();
    ClearDialogWindowAndFrameToTransparent(WIN_CASHIER_MSG, FALSE);
    PutWindowTilemap(0);
    PutWindowTilemap(1);
    PutWindowTilemap(3);
    PutWindowTilemap(4);
    PutWindowTilemap(5);
    ScheduleBgCopyTilemapToVram(0);
    ScheduleBgCopyTilemapToVram(1);
    PrintListMenuCursorByID_WithColorIdx(data[0], 1);
    Subtask_ReturnToTMCaseMain(taskId);
}

static void Task_InitQuantitySelectUI(u8 taskId)
{
    s16 * data = gTasks[taskId].data;

    TMCase_SetWindowBorder1(7);
    ConvertIntToDecimalStringN(gStringVar1, 1, STR_CONV_MODE_LEADING_ZEROS, 2);
    StringExpandPlaceholders(gStringVar4, gText_xVar1);
    AddTextPrinterParameterized_ColorByIndex(WIN_QUANTITY, FONT_SMALL, gStringVar4, 4, 10, 1, 0, 0, 1);
    SellTM_PrintQuantityAndSalePrice(1, ItemId_GetPrice(BagGetItemIdByPocketPosition(POCKET_TM_HM, data[1])) / 2 * data[8]);
    HandlePrintMoneyOnHand();
    CreateTMCaseScrollIndicatorArrowPair_SellQuantitySelect();
    ScheduleBgCopyTilemapToVram(0);
    ScheduleBgCopyTilemapToVram(1);
    gTasks[taskId].func = Task_QuantitySelect_HandleInput;
}

static void SellTM_PrintQuantityAndSalePrice(s16 quantity, s32 amount)
{
    FillWindowPixelBuffer(WIN_QUANTITY, 0x11);
    ConvertIntToDecimalStringN(gStringVar1, quantity, STR_CONV_MODE_LEADING_ZEROS, 2);
    StringExpandPlaceholders(gStringVar4, gText_xVar1);
    AddTextPrinterParameterized_ColorByIndex(WIN_QUANTITY, FONT_SMALL, gStringVar4, 4, 10, 1, 0, 0, 1);
    PrintMoneyAmount(WIN_QUANTITY, 0x38, 0x0A, amount, 0);
}

static void Task_QuantitySelect_HandleInput(u8 taskId)
{
    s16 * data = gTasks[taskId].data;

    if (AdjustQuantityAccordingToDPadInput(&data[8], data[2]) == 1)
    {
        SellTM_PrintQuantityAndSalePrice(data[8], ItemId_GetPrice(BagGetItemIdByPocketPosition(POCKET_TM_HM, data[1])) / 2 * data[8]);
    }
    else if (JOY_NEW(A_BUTTON))
    {
        PlaySE(SE_SELECT);
        ClearStdWindowAndFrameToTransparent(WIN_QUANTITY, FALSE);
        ScheduleBgCopyTilemapToVram(0);
        ScheduleBgCopyTilemapToVram(1);
        RemoveTMCaseScrollIndicatorArrowPair();
        Task_AskConfirmSaleWithAmount(taskId);
    }
    else if (JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_SELECT);
        ClearStdWindowAndFrameToTransparent(WIN_QUANTITY, FALSE);
        ClearStdWindowAndFrameToTransparent(WIN_MONEYBOX, FALSE);
        RemoveMoneyLabelObject();
        ClearDialogWindowAndFrameToTransparent(WIN_CASHIER_MSG, FALSE);
        PutWindowTilemap(3);
        PutWindowTilemap(0);
        PutWindowTilemap(1);
        ScheduleBgCopyTilemapToVram(0);
        ScheduleBgCopyTilemapToVram(1);
        RemoveTMCaseScrollIndicatorArrowPair();
        PrintListMenuCursorByID_WithColorIdx(data[0], 1);
        Subtask_ReturnToTMCaseMain(taskId);
    }
}

static void Task_PrintSaleConfirmedText(u8 taskId)
{
    s16 * data = gTasks[taskId].data;

    PutWindowTilemap(0);
    ScheduleBgCopyTilemapToVram(0);
    CopyItemName(gSpecialVar_ItemId, gStringVar2);
    ConvertIntToDecimalStringN(gStringVar1, ItemId_GetPrice(BagGetItemIdByPocketPosition(POCKET_TM_HM, data[1])) / 2 * data[8], STR_CONV_MODE_LEFT_ALIGN, 6);
    StringExpandPlaceholders(gStringVar4, gText_TurnedOverVar1ForVar2);
    TMCase_PrintMessageWithFollowupTask(taskId, FONT_NORMAL, gStringVar4, Task_DoSaleOfTMs);
}

static void Task_DoSaleOfTMs(u8 taskId)
{
    s16 * data = gTasks[taskId].data;

    PlaySE(SE_SHOP);
    RemoveBagItem(gSpecialVar_ItemId, data[8]);
    AddMoney(&gSaveBlock1Ptr->money, ItemId_GetPrice(gSpecialVar_ItemId) / 2 * data[8]);
    // RecordItemPurchase(gSpecialVar_ItemId, data[8], 2);
    DestroyListMenuTask(data[0], &sTMCaseStaticResources.scrollOffset, &sTMCaseStaticResources.selectedRow);
    TMCaseSetup_GetTMCount();
    TMCaseSetup_InitListMenuPositions();
    InitTMCaseListMenuItems();
    data[0] = ListMenuInit(&gMultiuseListMenuTemplate, sTMCaseStaticResources.scrollOffset, sTMCaseStaticResources.selectedRow);
    PrintListMenuCursorByID_WithColorIdx(data[0], 2);
    PrintMoneyAmountInMoneyBox(WIN_MONEYBOX, GetMoney(&gSaveBlock1Ptr->money), 0);
    gTasks[taskId].func = Task_AfterSale_ReturnToList;
}

static void Task_AfterSale_ReturnToList(u8 taskId)
{
    if (JOY_NEW(A_BUTTON) || JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_SELECT);
        ClearStdWindowAndFrameToTransparent(WIN_MONEYBOX, FALSE);
        RemoveMoneyLabelObject();
        ClearDialogWindowAndFrameToTransparent(WIN_CASHIER_MSG, FALSE);
        PutWindowTilemap(1);
        PutWindowTilemap(3);
        PutWindowTilemap(4);
        PutWindowTilemap(5);
        Subtask_CloseContextMenuAndReturnToMain(taskId);
    }
}

static void InitTMCaseWindows(void)
{
    u8 i;

    InitWindows(sWindowTemplates);
    DeactivateAllTextPrinters();
    LoadMessageBoxGfx(WIN_LIST, 0x64, 0xB0);
    LoadUserWindowBorderGfx(WIN_LIST, 0x78, 0xD0);
    LoadPalette(sTMCaseMainWindowPalette, 0xF0, 0x20);
    LoadPalette(sTMCaseMainWindowPalette, 0xA0, 0x20);
    LoadPalette(sPal3Override, 0xF6, 0x04);
    LoadPalette(sPal3Override, 0xD6, 0x04);
    ListMenuLoadStdPalAt(0xc0, 0x01);

    for (i = 0; i < 9; i++)
        FillWindowPixelBuffer(i, 0x00);

    PutWindowTilemap(0);
    PutWindowTilemap(1);
    PutWindowTilemap(3);
    PutWindowTilemap(4);
    PutWindowTilemap(5);
    ScheduleBgCopyTilemapToVram(0);
}

static void AddTextPrinterParameterized_ColorByIndex(u8 windowId, u8 fontId, const u8 * str, u8 x, u8 y, u8 letterSpacing, u8 lineSpacing, u8 speed, u8 colorIdx)
{
    AddTextPrinterParameterized4(windowId, fontId, x, y, letterSpacing, lineSpacing, sTextColors[colorIdx], speed, str);
}

static void TMCase_SetWindowBorder1(u8 windowId)
{
    DrawStdFrameWithCustomTileAndPalette(windowId, FALSE, 0x5B, 0x0E);
}

static void TMCase_SetWindowBorder2(u8 windowId)
{
    DrawStdFrameWithCustomTileAndPalette(windowId, FALSE, 0x78, 0x0D);
}

static void TMCase_PrintMessageWithFollowupTask(u8 taskId, u8 fontId, const u8 * str, TaskFunc func)
{
    DisplayMessageAndContinueTask(taskId, 6, 0x64, 0x0B, fontId, GetPlayerTextSpeedDelay(), str, func);
    ScheduleBgCopyTilemapToVram(1);
}

static void PrintStringTMCaseOnWindow3(void)
{
    u32 distance = 72 - GetStringWidth(FONT_NORMAL, gText_TMCase, 0);
    AddTextPrinterParameterized3(WIN_LABEL, FONT_NORMAL, distance / 2, 1, sTextColors[0], 0, gText_TMCase);
}

static void DrawMoveInfoUIMarkers(void)
{
    BlitMenuInfoIcon(WIN_UI_MARKERS, 19, 0, 0);
    BlitMenuInfoIcon(WIN_UI_MARKERS, 20, 0, 12);
    BlitMenuInfoIcon(WIN_UI_MARKERS, 21, 0, 24);
    BlitMenuInfoIcon(WIN_UI_MARKERS, 22, 0, 36);
    CopyWindowToVram(WIN_UI_MARKERS, COPYWIN_GFX);
}

static void TMCase_MoveCursor_UpdatePrintedTMInfo(u16 itemId)
{
    u8 i;
    u16 move;
    const u8 * str;

    FillWindowPixelRect(WIN_MOVE_INFO, 0, 0, 0, 40, 48);
    if (itemId == ITEM_NONE)
    {
        for (i = 0; i < 4; i++)
        {
            AddTextPrinterParameterized_ColorByIndex(WIN_MOVE_INFO, FONT_SHORT_COPY_1, gText_ThreeDashes, 7, 12 * i, 0, 0, 0xFF, 3);
        }
        CopyWindowToVram(WIN_MOVE_INFO, COPYWIN_GFX);
    }
    else
    {
        move = ItemIdToBattleMoveId(itemId);
        BlitMenuInfoIcon(WIN_MOVE_INFO, gBattleMoves[move].type + 1, 0, 0);
        if (gBattleMoves[move].power < 2)
            str = gText_ThreeDashes;
        else
        {
            ConvertIntToDecimalStringN(gStringVar1, gBattleMoves[move].power, STR_CONV_MODE_RIGHT_ALIGN, 3);
            str = gStringVar1;
        }
        AddTextPrinterParameterized_ColorByIndex(WIN_MOVE_INFO, FONT_SHORT_COPY_1, str, 7, 12, 0, 0, 0xFF, 3);
        if (gBattleMoves[move].accuracy == 0)
            str = gText_ThreeDashes;
        else
        {
            ConvertIntToDecimalStringN(gStringVar1, gBattleMoves[move].accuracy, STR_CONV_MODE_RIGHT_ALIGN, 3);
            str = gStringVar1;
        }
        AddTextPrinterParameterized_ColorByIndex(WIN_MOVE_INFO, FONT_SHORT_COPY_1, str, 7, 24, 0, 0, 0xFF, 3);
        ConvertIntToDecimalStringN(gStringVar1, gBattleMoves[move].pp, STR_CONV_MODE_RIGHT_ALIGN, 3);
        AddTextPrinterParameterized_ColorByIndex(WIN_MOVE_INFO, FONT_SHORT_COPY_1, gStringVar1, 7, 36, 0, 0, 0xFF, 3);
        CopyWindowToVram(WIN_MOVE_INFO, COPYWIN_GFX);
    }
}

static void PlaceHMTileInWindow(u8 windowId, u8 x, u8 y)
{
    BlitBitmapToWindow(windowId, sTMCaseHMIcon_Gfx, x, y, 16, 12);
}

static void HandlePrintMoneyOnHand(void)
{
    AddMoneyLabelObject(19, 11);
    PrintMoneyAmountInMoneyBoxWithBorder(WIN_MONEYBOX, 0x78, 0x0D, GetMoney(&gSaveBlock1Ptr->money));
}

static void HandleCreateYesNoMenu(u8 taskId, const struct YesNoFuncTable *ptrs)
{
    CreateYesNoMenuWithCallbacks(taskId, &sYesNoWindowTemplate, FONT_SHORT, 0, 2, 0x78, 0x0E, ptrs);
}

static u8 AddTMContextMenu(u8 * a0, u8 a1)
{
    if (*a0 == 0xFF)
    {
        *a0 = AddWindow(&sTMContextWindowTemplates[a1]);
        TMCase_SetWindowBorder2(*a0);
        ScheduleBgCopyTilemapToVram(0);
    }
    return *a0;
}

static void RemoveTMContextMenu(u8 * a0)
{
    ClearStdWindowAndFrameToTransparent(*a0, FALSE);
    ClearWindowTilemap(*a0);
    RemoveWindow(*a0);
    ScheduleBgCopyTilemapToVram(0);
    *a0 = 0xFF;
}

static u8 CreateTMSprite(u16 itemId)
{
    u8 spriteId = CreateSprite(&sTMSpriteTemplate, 0x29, 0x2E, 0);
    u8 r5;
    if (itemId == ITEM_NONE)
    {
        UpdateTMSpritePosition(&gSprites[spriteId], 0xFF);
        return spriteId;
    }
    else
    {
        r5 = itemId - 33;
        SetTMSpriteAnim(&gSprites[spriteId], r5);
        TintTMSpriteByType(gBattleMoves[ItemIdToBattleMoveId(itemId)].type);
        UpdateTMSpritePosition(&gSprites[spriteId], r5);
        return spriteId;
    }
}

static void SetTMSpriteAnim(struct Sprite *sprite, u8 idx)
{
    if (idx >= 50)
        StartSpriteAnim(sprite, 1);
    else
        StartSpriteAnim(sprite, 0);
}

static void TintTMSpriteByType(u8 type)
{
    u8 palIndex = IndexOfSpritePaletteTag(TM_CASE_TM_TAG) << 4;
    LoadPalette(sTMSpritePaletteBuffer + sTMSpritePaletteOffsetByType[type], 0x100 | palIndex, 0x20);
    if (sTMCaseStaticResources.tmCaseMenuType == 4)
    {
        BlendPalettes(1 << (0x10 + palIndex), 4, RGB_BLACK);
    }
}

static void UpdateTMSpritePosition(struct Sprite *sprite, u8 var)
{
    s32 x, y;
    if (var == 0xFF)
    {
        x = 0x1B;
        y = 0x36;
        sprite->y2 = 0x14;
    }
    else
    {
        if (var >= 50)
            var -= 50;
        else
            var += 8;
        x = 0x29 - (((0xE00 * var) / 58) >> 8);
        y = 0x2E + (((0x800 * var) / 58) >> 8);
    }
    sprite->x = x;
    sprite->y = y;
}

static void InitSelectedTMSpriteData(u8 spriteId, u16 itemId)
{
    gSprites[spriteId].data[0] = itemId;
    gSprites[spriteId].data[1] = 0;
    gSprites[spriteId].callback = SpriteCB_MoveTMSpriteInCase;
}

static void SpriteCB_MoveTMSpriteInCase(struct Sprite *sprite)
{
    switch (sprite->data[1])
    {
    case 0:
        if (sprite->y2 >= 20)
        {
            if (sprite->data[0] != ITEM_NONE)
            {
                sprite->data[1]++;
                TintTMSpriteByType(gBattleMoves[ItemIdToBattleMoveId(sprite->data[0])].type);
                sprite->data[0] -= ITEM_TM01;
                SetTMSpriteAnim(sprite, sprite->data[0]);
                UpdateTMSpritePosition(sprite, sprite->data[0]);
            }
            else
                sprite->callback = SpriteCallbackDummy;
        }
        else
        {
            sprite->y2 += 10;
        }
        break;
    case 1:
        if (sprite->y2 <= 0)
            sprite->callback = SpriteCallbackDummy;
        else
            sprite->y2 -= 10;
    }
}

static void LoadTMTypePalettes(void)
{
    struct SpritePalette spritePalette;

    sTMSpritePaletteBuffer = Alloc(0x110 * sizeof(u16));
    LZDecompressWram(sTMCaseTypes_Pal1, sTMSpritePaletteBuffer);
    LZDecompressWram(sTMCaseTypes_Pal2, sTMSpritePaletteBuffer + 0x100);
    spritePalette.data = sTMSpritePaletteBuffer + 0x110;
    spritePalette.tag = TM_CASE_TM_TAG;
    LoadSpritePalette(&spritePalette);
}
