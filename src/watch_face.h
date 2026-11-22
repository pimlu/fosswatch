#pragma once

/* Available watch faces. */
typedef enum {
    FACE_DIGITAL = 0,   /* clean 4-field running face (default) */
    FACE_ANALOG,        /* classic hands-and-debug face */
    FACE_COUNT
} watch_face_t;

/* Render whichever face is currently selected (and keep the movement tracker
 * fed). Called once per render tick from the main loop. */
void render_watch_screen();

/* Individual faces, exposed so a button handler can pick one directly. */
void render_digital_watch_face();
void render_analog_watch_face();

/* Face selection, for future button handling. */
void watch_face_set(watch_face_t face);
watch_face_t watch_face_get(void);
void watch_face_next(void);   /* cycle to the next face */
