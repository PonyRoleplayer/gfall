#ifndef DF_SERVICE_H
#define DF_SERVICE_H

/*
 * What a merchant offers when spoken to: classic's small panel, with Talk and
 * what they sell or do (GNRC01I0.IMG), or a smith's Repair, Talk and Sell
 * (REPR01I0.IMG). A temple's is the first, its second row Cure Disease
 * (DFU's guild service, without the guild's other rows yet). A tavern's is
 * TVRN00I0.IMG (DaggerfallTavernWindow): Room asks how many days, Food lists
 * the menu with its prices, Talk, Exit. A guild's is GILD00I0.IMG (Join
 * Guild, Talk, the person's service) or GILD01I0.IMG for a member, the
 * service's name written on its row (DaggerfallGuildServicePopupWindow). A
 * list (training's skills) is shown on a scroll, a line clicked.
 * Read docs/game.md (People) before editing.
 *
 * Plain C: hostcheck drives it by clicks.
 */

/* Which panel. */
enum {
    SERVICE_PANEL_SELL,
    SERVICE_PANEL_BANK,
    SERVICE_PANEL_REPAIR,
    SERVICE_PANEL_CURE,
    SERVICE_PANEL_TAVERN,
    SERVICE_PANEL_GUILD,
    SERVICE_PANEL_GUILD_MEMBER
};

/* What a click asks. */
enum {
    SERVICE_STAY,
    SERVICE_LEAVE,
    SERVICE_TALK,
    SERVICE_SELL,
    SERVICE_BANK,
    SERVICE_REPAIR,
    SERVICE_CURE,
    /* A tavern: the days typed for a room (service_days), a thing off the menu (service_food). */
    SERVICE_ROOM,
    SERVICE_FOOD,
    /* A guild's: Join Guild, its service, a line of a list (service_picked). */
    SERVICE_JOIN,
    SERVICE_GUILD,
    SERVICE_PICKED
};

int service_open(int panel);
void service_close(void);
void service_compose(void);
const unsigned char *service_pixels(void);
int service_changed(void);
int service_click(int x, int y);
/* A key: the days' digits, Backspace, Enter. As a click. */
int service_key(int vk);
int service_days(void);
int service_food(void);
/* A guild panel's service row's name ("Training"); set before it is composed. */
void service_set_label(const char *label);
/* A list over the panel, n lines of what (kept by the caller while it shows); a click on line i is SERVICE_PICKED. */
void service_list(const char *const *what, int n);
int service_picked(void);

#endif
