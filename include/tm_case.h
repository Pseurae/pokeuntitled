#ifndef GUARD_TM_CASE_H
#define GUARD_TM_CASE_H

struct TMCaseControl
{
    void (* savedCallback)(void);
    u8 tmCaseMenuType;
    u8 unk_05;
    u8 unk_06;
    u16 selectedRow;
    u16 scrollOffset;
};

struct TMCaseData
{
    void (* savedCallback)(void);
    u8 tmSpriteId;
    u8 maxTMsShown;
    u8 numTMs;
    u8 contextMenuWindowId;
    u8 scrollIndicatorArrowPairId;
    u16 currItem;
    const u8 * menuActionIndices;
    u8 numMenuActions;
    s16 seqId;
    u8 filler_14[8];
};

enum TMCaseType
{
    TMCASE_FROMFIELD,
    TMCASE_FROMPARTYGIVE,
    TMCASE_FROMMARTSELL,
    TMCASE_FROMPOKEMONSTORAGEPC,
    TMCASE_FROMBATTLE,
    TMCASE_NA
};

#define TM_CASE_TM_TAG 400

void OpenTMCase(u8 type, void (* exitCallback)(void), u8 a2);
void ResetTMCaseCursorPos(void);

#endif //GUARD_TM_CASE_H
