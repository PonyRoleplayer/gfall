#ifndef DF_SERVICE_H
#define DF_SERVICE_H

/*
 * What a merchant offers when spoken to: classic's small panel, with Talk and
 * what they sell or do (GNRC01I0.IMG), or a smith's Repair, Talk and Sell
 * (REPR01I0.IMG). Read docs/game.md (People) before editing.
 *
 * Plain C: hostcheck drives it by clicks.
 */

/* Which panel. */
enum {
    SERVICE_PANEL_SELL,
    SERVICE_PANEL_BANK,
    SERVICE_PANEL_REPAIR
};

/* What a click asks. */
enum {
    SERVICE_STAY,
    SERVICE_LEAVE,
    SERVICE_TALK,
    SERVICE_SELL,
    SERVICE_BANK,
    SERVICE_REPAIR
};

int service_open(int panel);
void service_close(void);
void service_compose(void);
const unsigned char *service_pixels(void);
int service_changed(void);
int service_click(int x, int y);

#endif
