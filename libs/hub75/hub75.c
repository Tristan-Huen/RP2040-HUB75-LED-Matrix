#include "hub75.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "led_matrix.pio.h"

#define BIT_PLANES BITS_PER_CHAN              //Number of bits per color channel.
#define TRANSFER_SIZE (ROWS/2 * COLS)         //Number of DMA transfers for a single bit plane.     
#define TOP_MASK_CLEAR ~(0x700)               //Bit mask inside is 0b0111_0000_0000
#define BOTTOM_MASK_CLEAR ~(0xE0)             //Bit mask inside is 0b0000_1110_0000
#define TOP_BOTTOM_MASK_CLEAR ~(0x7E0)        //Bit mask inside is 0b0111_1110_0000
#define IN_SHIFT_INSTR 0x4880                 //See the .README for more info.
#define IN_SHIFT_32_INSTR 0x4860              //This is the instruction for `in NULL 32 side 0b010`.

static int dma_chan;
static PIO pio = pio0;
static uint16_t *img;

uint32_t gamma_encode[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
                       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 
                       1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 
                       3, 3, 4, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 
                       7, 8, 8, 8, 9, 9, 9, 10, 10, 10, 11, 11, 11, 12, 12, 
                       13, 13, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18, 19, 
                       19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25, 25, 26, 27, 
                       27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36, 37, 
                       38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 
                       50, 51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 
                       66, 67, 68, 69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 
                       83, 85, 86, 87, 89, 90, 92, 93, 95, 96, 98, 99, 101, 102, 
                       104, 105, 107, 109, 110, 112, 114, 115, 117, 119, 120, 122, 
                       124, 126, 127, 129, 131, 133, 135, 137, 138, 140, 142, 144, 
                       146, 148, 150, 152, 154, 156, 158, 160, 162, 164, 167, 169, 
                       171, 173, 175, 177, 180, 182, 184, 186, 189, 191, 193, 196, 
                       198, 200, 203, 205, 208, 210, 213, 215, 218, 220, 223, 225, 
                       228, 231, 233, 236, 239, 241, 244, 247, 249, 252, 255};

/** 
* @brief DMA handler that gets called when transfers finish.
*
* DMA handler which is called when ARR_LEN/8 transfers finish where ARR_LEN is 
* the image array size. It changes the BCM delay in the state machine
* and changes the read address to the next N/8 values.
*
* Measurements:
* Runs every 76us approximately but this can change.
* For 8 bit planes we get approximately 530us (measured).
*/
static void dma_handler() {

    static uint32_t bit_plane = 0;
    
    bit_plane = (bit_plane + 1) % BIT_PLANES;

    //Ternary is here since since a 32 bit shift is represented differently.
    pio->instr_mem[27] = (bit_plane == 0) ? IN_SHIFT_32_INSTR : IN_SHIFT_INSTR-bit_plane;

    dma_hw->ints0 = 1u << dma_chan;

    dma_channel_set_read_addr(dma_chan, &img[bit_plane * TRANSFER_SIZE],true);
}

/** 
* @brief Extracts a group of bits from a unsigned integer.
*
* Extracts a group of bits from an integer using the number of bits 
* to be extracted, and the starting position.
*
* @param val The integer to extract bits from.
* @param num_bits The number of bits to extract.
* @param start_pos Starting position to extract from (0-based indexing).
* @return The extracted bits.
*/
static inline uint32_t extract_bits(uint32_t val, uint32_t num_bits, uint32_t start_pos) {
    uint32_t mask = ((1 << num_bits) - 1) << start_pos;
    return (val & mask) >> start_pos;
}

/** 
* @brief Gamma adjusts a 24-bit RGB value.
*
* Gamma adjusts a 24-bit RGB value using a lookup table.
*
* @param rgb A 24-bit RGB value.
* @return Gamma adjusted value.
*/
static inline uint32_t gamma_encode_rgb(uint32_t rgb) {
    uint32_t r = extract_bits(rgb,8,16);
    uint32_t g = extract_bits(rgb,8,8);
    uint32_t b = extract_bits(rgb,8,0);

    return (gamma_encode[r] << 16) | (gamma_encode[g] << 8) | gamma_encode[b];
}

void hub75_init(uint16_t *img_ptr) {

    img = img_ptr;

    uint32_t offset = pio_add_program(pio, &led_matrix_program);
    uint32_t sm = pio_claim_unused_sm(pio, true);

    led_matrix_program_init(pio, sm, offset, DATA_BASE_PIN);

    dma_chan = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
    channel_config_set_read_increment(&c, true);
    channel_config_set_dreq(&c, DREQ_PIO0_TX0);

    dma_channel_configure(
        dma_chan,
        &c,
        &pio0_hw->txf[0], //PIO FIFO
        &img[0],       
        TRANSFER_SIZE,    //Amount of elements for each bit plane  
        true              //Start immediately            
    );

    dma_channel_set_irq0_enabled(dma_chan,true);

    irq_set_exclusive_handler(DMA_IRQ_0,dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);
}

//Takes around 2us to 2.7us. Changing waveform width might be due to the if-else?
bool hub75_draw_pixel(uint32_t rgb, uint32_t x, uint32_t y) {

    //Out of bounds checking.
    if (x >= ROWS || y >= COLS || x < 0 || y < 0) {

		return false;
	}

    uint32_t rgb_corrected = gamma_encode_rgb(rgb);

    for (int i = 0; i < BIT_PLANES; i++) {
        uint32_t r_bit = extract_bits(rgb_corrected ,1,i+16);
        uint32_t g_bit = extract_bits(rgb_corrected ,1,i+8);
        uint32_t b_bit = extract_bits(rgb_corrected ,1,i);

        uint8_t combined = (r_bit << 2) | (g_bit << 1) | b_bit;

        //Bottom rows
        if(y >= ROWS/2) {
            uint16_t val = img[x + (y-ROWS/2)*COLS + i * TRANSFER_SIZE];
            val &= BOTTOM_MASK_CLEAR;
            val |= (combined << 5);
            img[x + (y-ROWS/2)*COLS + i * TRANSFER_SIZE] = val;
        }

        //Top rows
        else {
            uint16_t val = img[x + y*COLS + i * TRANSFER_SIZE];
            val &= TOP_MASK_CLEAR;
            val |= (combined << 8);
            img[x + y*COLS + i * TRANSFER_SIZE] = val;
        }
    }

    return true;

}

void hub75_clear_screen() {

    //This is faster than using hub75_draw_pixel repeatedly.
    for (int i = 0; i < ARR_LEN; i++) {
        img[i] &= TOP_BOTTOM_MASK_CLEAR;
    }
}