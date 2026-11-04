#include <switch.h>
#include <stdio.h>

typedef struct {
    const char *title;
    const char *playtime;
} DemoGame;

static const DemoGame games[] = {
    {"The Legend of Zelda: Tears of the Kingdom", "65 h o mas"},
    {"Metroid Dread", "18 h o mas"},
    {"Super Mario Odyssey", "30 h o mas"},
    {"Hollow Knight", "42 h o mas"},
    {"Hades", "26 h o mas"},
    {"Animal Crossing: New Horizons", "120 h o mas"},
    {"Celeste", "12 h o mas"},
};

static void draw_screen(int selected, const char *message) {
    consoleClear();
    printf("NOVA HOME - PRUEBA NATIVA\n");
    printf("=========================\n\n");
    printf("Prueba de arranque .nro / libnx\n");
    printf("Usa izquierda/derecha para recorrer la biblioteca.\n\n");

    for (unsigned int i = 0; i < sizeof(games) / sizeof(games[0]); i++) {
        printf("%c  %s\n", (int)i == selected ? '>' : ' ', games[i].title);
    }

    printf("\nTiempo de juego (demo): %s\n", games[selected].playtime);
    printf("\nA: seleccionar    +: salir al Homebrew Menu\n");
    printf("%s\n", message ? message : "");
    consoleUpdate(NULL);
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    consoleInit(NULL);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);

    PadState pad;
    padInitializeDefault(&pad);

    int selected = 0;
    char message[96] = "";
    draw_screen(selected, message);

    while (appletMainLoop()) {
        padUpdate(&pad);
        const u64 pressed = padGetButtonsDown(&pad);

        if (pressed & HidNpadButton_Plus)
            break;

        if (pressed & HidNpadButton_Left) {
            selected = (selected + (int)(sizeof(games) / sizeof(games[0])) - 1)
                % (int)(sizeof(games) / sizeof(games[0]));
            message[0] = '\0';
            draw_screen(selected, message);
        } else if (pressed & HidNpadButton_Right) {
            selected = (selected + 1) % (int)(sizeof(games) / sizeof(games[0]));
            message[0] = '\0';
            draw_screen(selected, message);
        } else if (pressed & HidNpadButton_A) {
            snprintf(message, sizeof(message), "Seleccionado: %s", games[selected].title);
            draw_screen(selected, message);
        }

        svcSleepThread(16000000ULL);
    }

    consoleExit(NULL);
    return 0;
}
