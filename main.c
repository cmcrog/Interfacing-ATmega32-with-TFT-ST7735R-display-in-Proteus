/*
 * SD_TFT_TEST.c
 * main.c - SD + TFT + BMP dinámico + Mandelbrot/Julia estáticos + botones PD0/PD1
 */

#define F_CPU 8000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include <string.h>

#include "spi_hal.h"
#include "sd_spi.h"
#include "fat_fs.h"
#include "bmp_stream.h"
#include "tft_st7735.h"

/* ==========================================================
   DECLARACIÓN DE FUNCIÓN NUEVA DE fat_fs.c
   ========================================================== */

// Lista hasta max_files nombres .BMP encontrados en root.
// Cada nombre es una cadena "XXXXX.BMP".
uint8_t FAT_ListBMP(char names[][13], uint8_t max_files);

/* ==========================================================
   BOTONES Y MODOS
   ========================================================== */

// BOTÓN DE CAMBIO DE MODO GENERAL (viewer <-> fractal)
#define BTN_MODE_PORT     PORTD
#define BTN_MODE_PINREG   PIND
#define BTN_MODE_DDR      DDRD
#define BTN_MODE_BIT      PD0   // botón en PD0

// BOTÓN DE CAMBIO DE TIPO DE FRACTAL (Mandelbrot <-> Julia)
#define BTN_FRACTAL_PORT   PORTD
#define BTN_FRACTAL_PINREG PIND
#define BTN_FRACTAL_DDR    DDRD
#define BTN_FRACTAL_BIT    PD1   // botón en PD1

#define MODE_VIEWER    0
#define MODE_FRACTAL   1

#define FRACTAL_MANDEL 0
#define FRACTAL_JULIA  1

/* ==========================================================
   COLORACIÓN MANDELBROT (paleta que ya tenías)
   ========================================================== */

static uint16_t color_from_iter_mandel(uint8_t iter, uint8_t max_iter)
{
    if (iter >= max_iter) {
        // Interior del conjunto: negro para máximo contraste
        return 0x0000;
    }

    // Normalizar iter a 0..255
    uint16_t t0 = (uint16_t)iter * 255 / max_iter;  // 0..255
    // Hacemos que el color "cicle" 3 veces a lo largo de las iteraciones
    uint8_t t = (uint8_t)((t0 * 3) & 0xFF);         // 0..255, con 3 bandas

    uint8_t r, g, b;

    if (t < 85) {
        // Azul (0,0,128) -> Cian (0,255,255)
        r = 0;
        g = (uint8_t)(3 * t);                      // ~0..255
        b = 128 + (uint8_t)(t * 127 / 85);         // 128..255
    }
    else if (t < 170) {
        uint8_t tt = t - 85;
        // Cian (0,255,255) -> Amarillo (255,255,0)
        r = (uint8_t)(3 * tt);                     // 0..255
        g = 255;
        b = (uint8_t)(255 - 3 * tt);               // 255..0
    }
    else {
        uint8_t tt = t - 170;
        // Amarillo (255,255,0) -> Blanco (255,255,255)
        r = 255;
        g = 255;
        b = (uint8_t)(tt * 3);                     // 0..255
    }

    // Convertir a RGB565
    uint16_t color =
        ((r & 0xF8) << 8) |
        ((g & 0xFC) << 3) |
        ((b & 0xF8) >> 3);

    return color;
}

/* ==========================================================
   COLORACIÓN JULIA (paleta clásica distinta)
   ========================================================== */

static uint16_t color_from_iter_julia(uint8_t iter, uint8_t max_iter)
{
    if (iter >= max_iter)
        return 0x0000; // interior negro

    // Normalizar 0..255
    uint16_t t = (uint16_t)iter * 255 / max_iter;

    uint8_t r, g, b;

    if (t < 32) {
        // Negro -> violeta oscuro
        r = 20;
        g = 0;
        b = (uint8_t)(t * 8);              // 0..255
    }
    else if (t < 64) {
        // Violeta -> púrpura brillante
        uint8_t k = t - 32;
        r = (uint8_t)(40 + k * 3);         // ~40..136
        g = 0;
        b = 255;
    }
    else if (t < 128) {
        // Púrpura -> rojo
        uint8_t k = t - 64;
        r = (uint8_t)(k * 4);              // 0..255
        g = 0;
        b = (uint8_t)(255 - k * 4);        // 255..0
    }
    else if (t < 192) {
        // Rojo -> naranja -> amarillo
        uint8_t k = t - 128;
        r = 255;
        g = (uint8_t)(k * 3);              // 0..192
        b = 0;
    }
    else {
        // Amarillo -> blanco
        uint8_t k = t - 192;
        r = 255;
        g = 255;
        b = (uint8_t)(k * 4);              // 0..255 (se satura a 255)
        if (b > 255) b = 255;
    }

    // Pasar a RGB565
    uint16_t color =
        ((r & 0xF8) << 8) |
        ((g & 0xFC) << 3) |
        ((b & 0xF8) >> 3);

    return color;
}

/* ==========================================================
   PUNTO FIJO Q5.11
   ========================================================== */

#define Q      11
#define Q_ONE  (1 << Q)

typedef int16_t q5_11_t;

static inline q5_11_t qmul(q5_11_t a, q5_11_t b)
{
    int32_t t = (int32_t)a * (int32_t)b;
    return (q5_11_t)(t >> Q);
}

static inline q5_11_t q_from_int(int16_t n)
{
    return (q5_11_t)(n * Q_ONE);
}

/* ==========================================================
   PARÁMETROS DE FRACTAL: MANDELBROT Y JULIA
   ========================================================== */

typedef struct {
    q5_11_t center_re;
    q5_11_t center_im;
    q5_11_t scale;
    uint8_t max_iter;
} FractalParams;

// Mandelbrot: centrado en el cuerpo principal
static const FractalParams FRACTAL_MANDEL_PARAMS = {
    .center_re = (q5_11_t)(-1536),  // -0.75 * 2048
    .center_im = (q5_11_t)(0),      // 0.0
    .scale     = (q5_11_t)(3072),   // 1.5 * 2048
    .max_iter  = 120
};

// Julia con C = -0.8 + 0.156i
#define JULIA_C_RE  ((q5_11_t)-1638) // -0.8  * 2048
#define JULIA_C_IM  ((q5_11_t)  319) //  0.156* 2048

static const FractalParams FRACTAL_JULIA_PARAMS = {
    .center_re = (q5_11_t)(0),      // centro 0+0i
    .center_im = (q5_11_t)(0),
    .scale     = (q5_11_t)(3072),   // +/-1.5
    .max_iter  = 120
};

/* ==========================================================
   RENDER DEL FRACTAL (MANDELBROT O JULIA)
   ========================================================== */

static uint8_t current_fractal_type = FRACTAL_MANDEL;

static void draw_fractal(void)
{
    const FractalParams *p;

    if (current_fractal_type == FRACTAL_JULIA)
        p = &FRACTAL_JULIA_PARAMS;
    else
        p = &FRACTAL_MANDEL_PARAMS;

    uint8_t  max_iter = p->max_iter;
    q5_11_t  re_min   = p->center_re - p->scale;
    q5_11_t  re_max   = p->center_re + p->scale;
    q5_11_t  im_min   = p->center_im - p->scale;
    q5_11_t  im_max   = p->center_im + p->scale;

    q5_11_t re_step = (q5_11_t)((int32_t)(re_max - re_min) / (TFT_WIDTH  - 1));
    q5_11_t im_step = (q5_11_t)((int32_t)(im_max - im_min) / (TFT_HEIGHT - 1));

    const q5_11_t escape2 = q_from_int(4); // |z|^2 > 4

    for (uint16_t py = 0; py < TFT_HEIGHT; py++)
    {
        q5_11_t cy_pixel = im_min + (q5_11_t)((int32_t)im_step * py);

        TFT_SetAddrWindow(0, py, TFT_WIDTH - 1, py);
        TFT_StartWrite();

        q5_11_t cx_pixel = re_min;

        for (uint16_t px = 0; px < TFT_WIDTH; px++)
        {
            q5_11_t zx, zy, cx, cy;

            if (current_fractal_type == FRACTAL_MANDEL) {
                // Mandelbrot: z0=0, c = punto
                zx = 0;
                zy = 0;
                cx = cx_pixel;
                cy = cy_pixel;
            } else {
                // Julia: z0 = punto, c = constante fija
                zx = cx_pixel;
                zy = cy_pixel;
                cx = JULIA_C_RE;
                cy = JULIA_C_IM;
            }

            uint8_t iter = 0;

            while (iter < max_iter)
            {
                q5_11_t zx2 = qmul(zx, zx);
                q5_11_t zy2 = qmul(zy, zy);
                q5_11_t zxy = qmul(zx, zy);

                q5_11_t zx_new = zx2 - zy2 + cx;
                q5_11_t zy_new = (q5_11_t)((int16_t)(zxy << 1) + cy);

                zx = zx_new;
                zy = zy_new;

                q5_11_t mag2 = qmul(zx, zx) + qmul(zy, zy);
                if (mag2 > escape2)
                    break;

                iter++;
            }

            uint16_t color;
            if (current_fractal_type == FRACTAL_MANDEL)
                color = color_from_iter_mandel(iter, max_iter);
            else
                color = color_from_iter_julia(iter, max_iter);

            TFT_WriteColor(color);

            cx_pixel = (q5_11_t)(cx_pixel + re_step);
        }

        TFT_EndWrite();
    }
}

/* ==========================================================
   GALERÍA BMP: lista dinámica desde la SD
   ========================================================== */

#define MAX_BMP_FILES 16
static char   bmp_list[MAX_BMP_FILES][13];
static uint8_t bmp_count = 0;

static void draw_bmp(BMP_Image *img)
{
    if (img->width  > TFT_WIDTH)  img->width  = TFT_WIDTH;
    if (img->height > TFT_HEIGHT) img->height = TFT_HEIGHT;

    uint16_t buf[TFT_WIDTH];

    uint8_t ox = (TFT_WIDTH  - img->width ) / 2;
    uint8_t oy = (TFT_HEIGHT - img->height) / 2;

    for (uint16_t y = 0; y < img->height; y++)
    {
        if (BMP_ReadRow(img, y, buf) != 0) break;

        TFT_SetAddrWindow(ox, oy + y, ox + img->width - 1, oy + y);
        TFT_StartWrite();
        for (uint16_t x = 0; x < img->width; x++)
            TFT_WriteColor(buf[x]);
        TFT_EndWrite();
    }
}

static void gallery_step(void)
{
    static uint8_t init = 0;
    static uint8_t okSD = 0, okFAT = 0;
    static uint8_t index = 0;
    static BMP_Image img;

    if (!init) {
        uint8_t s = SD_Init();
        okSD = (s == SD_OK);

        if (okSD && FAT_Init() == 0) {
            okFAT = 1;
            // Construir lista de BMPs una sola vez
            bmp_count = FAT_ListBMP(bmp_list, MAX_BMP_FILES);
        }

        init = 1;
    }

    if (!(okSD && okFAT)) {
        TFT_FillScreen(0x2104); // gris oscuro
        _delay_ms(300);
        return;
    }

    if (bmp_count == 0) {
        // No hay BMP en la SD
        TFT_FillScreen(0x001F); // azul
        _delay_ms(500);
        return;
    }

    if (index >= bmp_count)
        index = 0;

    if (BMP_Open(&img, bmp_list[index]) == 0)
        draw_bmp(&img);
    else
        TFT_FillScreen(0xF800); // rojo -> error al abrir

    index++;
    if (index >= bmp_count) index = 0;

    _delay_ms(800); // tiempo visible por imagen
}

/* ==========================================================
   FRACTAL STEP: estático, redibuja solo si cambia tipo
   ========================================================== */

static void fractal_step(void)
{
    static uint8_t drawn = 0;
    static uint8_t last_type = 0xFF;

    if (!drawn || last_type != current_fractal_type) {
        draw_fractal();
        drawn = 1;
        last_type = current_fractal_type;
    } else {
        _delay_ms(100);
    }
}

/* ==========================================================
   GESTIÓN DE BOTONES (flanco de bajada)
   ========================================================== */

static uint8_t button_pressed_edge_PD0(void)
{
    static uint8_t prev = 1;
    uint8_t now = (BTN_MODE_PINREG & (1 << BTN_MODE_BIT)) ? 1 : 0;
    uint8_t pressed = 0;

    if (prev == 1 && now == 0) {
        _delay_ms(20);
        if (!(BTN_MODE_PINREG & (1 << BTN_MODE_BIT)))
            pressed = 1;
    }

    prev = now;
    return pressed;
}

static uint8_t button_pressed_edge_PD1(void)
{
    static uint8_t prev = 1;
    uint8_t now = (BTN_FRACTAL_PINREG & (1 << BTN_FRACTAL_BIT)) ? 1 : 0;
    uint8_t pressed = 0;

    if (prev == 1 && now == 0) {
        _delay_ms(20);
        if (!(BTN_FRACTAL_PINREG & (1 << BTN_FRACTAL_BIT)))
            pressed = 1;
    }

    prev = now;
    return pressed;
}

/* ==========================================================
   MAIN
   ========================================================== */

int main(void)
{
    SPI_Init(4);
    TFT_Init();

    // Botón PD0 (modo) -> entrada con pull-up
    BTN_MODE_DDR  &= ~(1 << BTN_MODE_BIT);
    BTN_MODE_PORT |=  (1 << BTN_MODE_BIT);

    // Botón PD1 (tipo de fractal) -> entrada con pull-up
    BTN_FRACTAL_DDR  &= ~(1 << BTN_FRACTAL_BIT);
    BTN_FRACTAL_PORT |=  (1 << BTN_FRACTAL_BIT);

    uint8_t mode = MODE_FRACTAL;  // arrancamos mostrando fractal

    while (1)
    {
        // Botón PD0: cambiar entre visor y fractal
        if (button_pressed_edge_PD0()) {
            mode = (mode == MODE_FRACTAL) ? MODE_VIEWER : MODE_FRACTAL;
            TFT_FillScreen(0x0000); // limpiar pantalla al cambiar
        }

        // Botón PD1: solo tiene efecto en modo fractal
        if (mode == MODE_FRACTAL && button_pressed_edge_PD1()) {
            current_fractal_type ^= 1;   // toggle Mandelbrot/Julia
            TFT_FillScreen(0x0000);      // limpiar para redibujo
        }

        if (mode == MODE_VIEWER)
            gallery_step();
        else
            fractal_step();
    }
}
