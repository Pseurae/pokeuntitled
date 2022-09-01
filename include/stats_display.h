#ifndef GUARD_STATS_DISPLAY_H
#define GUARD_STATS_DISPLAY_H

struct StatsDisplayData
{
    void (*savedCallback)(void);
    struct Sprite *monIconSprites[PARTY_SIZE];
    u8 selectedIndex;
    struct Pokemon *selectedMon;
    u16 selectedMonSpecies;
    u8 portraitSpriteId;
    u8 typeSpriteIds[2];
};

void OpenStatsDisplay(u8 type, void (* callback)(void));

#endif //GUARD_STATS_DISPLAY_H