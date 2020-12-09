#include <linux/module.h>
#include <linux/fs.h>
#include <linux/printk.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/string.h>

/*
 * HEADER STUFF
 */

#define BUF_SIZE        64
#define ESC_SEQ_BUF_SIZE    16

struct hd44780_geometry {
    int cols;
    int rows;
    int start_addrs[];
};

struct hd44780 {
    struct cdev cdev;
    struct device *device;
    struct i2c_client *i2c_client;
    struct hd44780_geometry *geometry;

    /* Current cursor positon on the display */
    struct {
        int row;
        int col;
    } pos;

    char buf[BUF_SIZE];
    struct {
        char buf[ESC_SEQ_BUF_SIZE];
        int length;
    } esc_seq_buf;
    bool is_in_esc_seq;
    char p_counter;

    bool backlight;
    bool cursor_blink;
    bool cursor_display;
    /* during initializaion dirty is set true so that display of initialization output
     will be cleared before first write. */
    bool dirty;

    struct mutex lock;
    struct list_head list;
};

void hd44780_write(struct hd44780 *, const char *, size_t);
void hd44780_init_lcd(struct hd44780 *);
void hd44780_print(struct hd44780 *, const char *);
void hd44780_flush(struct hd44780 *);
void hd44780_set_geometry(struct hd44780 *, struct hd44780_geometry *);
void hd44780_set_backlight(struct hd44780 *, bool);
void hd44780_set_cursor_blink(struct hd44780 *, bool);
void hd44780_set_cursor_display(struct hd44780 *, bool);

extern struct hd44780_geometry *hd44780_geometries[];

/*
 * DEVICE INFO
 */

#define BL  0x08
#define E   0x04
#define RW  0x02
#define RS  0x01

#define HD44780_CLEAR_DISPLAY   0x01
#define HD44780_RETURN_HOME 0x02
#define HD44780_ENTRY_MODE_SET  0x04
#define HD44780_DISPLAY_CTRL    0x08
#define HD44780_SHIFT       0x10
#define HD44780_FUNCTION_SET    0x20
#define HD44780_CGRAM_ADDR  0x40
#define HD44780_DDRAM_ADDR  0x80

#define HD44780_DL_8BITS    0x10
#define HD44780_DL_4BITS    0x00
#define HD44780_N_2LINES    0x08
#define HD44780_N_1LINE     0x00

#define HD44780_D_DISPLAY_ON    0x04
#define HD44780_D_DISPLAY_OFF   0x00
#define HD44780_C_CURSOR_ON 0x02
#define HD44780_C_CURSOR_OFF    0x00
#define HD44780_B_BLINK_ON  0x01
#define HD44780_B_BLINK_OFF 0x00

#define HD44780_ID_INCREMENT    0x02
#define HD44780_ID_DECREMENT    0x00
#define HD44780_S_SHIFT_ON  0x01
#define HD44780_S_SHIFT_OFF 0x00

static struct hd44780_geometry hd44780_geometry_20x4 = {
    .cols = 20,
    .rows = 4,
    .start_addrs = {0x00, 0x40, 0x14, 0x54},
};

static struct hd44780_geometry hd44780_geometry_16x2 = {
    .cols = 16,
    .rows = 2,
    .start_addrs = {0x00, 0x40},
};

static struct hd44780_geometry hd44780_geometry_8x1 = {
    .cols = 8,
    .rows = 1,
    .start_addrs = {0x00},
};

struct hd44780_geometry *hd44780_geometries[] = {
    &hd44780_geometry_20x4,
    &hd44780_geometry_16x2,
    &hd44780_geometry_8x1,
    NULL
};

/* Defines possible register that we can write to */
typedef enum { IR, DR } dest_reg;

/*
    READ BLOCKS
*/

static u8 hd44780_read_data(struct hd44780 *lcd)
{

    u8 h = i2c_smbus_read_byte(lcd->i2c_client);
    u8 l = i2c_smbus_read_byte(lcd->i2c_client);
    
    printk (KERN_DEBUG "read hex h=%X l=%X \n", h, l );

    return (u8) ((h <<4) & 0xF0) | (l & 0XF);

}

/*


    WRITE BLOCKS
*/

static void pcf8574_raw_write(struct hd44780 *lcd, u8 data)
{
    i2c_smbus_write_byte(lcd->i2c_client, data);
}

static void hd44780_write_nibble(struct hd44780 *lcd, dest_reg reg, u8 data)
{
    /* Shift the interesting data on the upper 4 bits (b7-b4) */
    data = (data << 4) & 0xF0;

    /* Flip the RS bit if we write do data register */
    if (reg == DR)
        data |= RS;

    /* Keep the RW bit low, because we write */
    data = data | (RW & 0x00);

    /* Flip the backlight bit */
    if (lcd->backlight)
        data |= BL;

    pcf8574_raw_write(lcd, data);
    /* Theoretically wait for tAS = 40ns, practically it's already elapsed */

    /* Raise the E signal... */
    pcf8574_raw_write(lcd, data | E);
    /* Again, "wait" for pwEH = 230ns */

    /* ...and let it fall to clock the data into the HD44780's register */
    pcf8574_raw_write(lcd, data);
    /* And again, "wait" for about tCYC_E - pwEH = 270ns */
}

/*
 * Takes a regular 8-bit instruction and writes it's high nibble into device's
 * instruction register. The low nibble is assumed to be all zeros. This is
 * used with a physical 4-bit bus when the device is still expecting 8-bit
 * instructions.
 */
static void hd44780_write_instruction_high_nibble(struct hd44780 *lcd, u8 data)
{
    u8 h = (data >> 4) & 0x0F;

    hd44780_write_nibble(lcd, IR, h);

    udelay(37);
}

static void hd44780_write_instruction(struct hd44780 *lcd, u8 data)
{
    u8 h = (data >> 4) & 0x0F;
    u8 l = data & 0x0F;

    hd44780_write_nibble(lcd, IR, h);
    hd44780_write_nibble(lcd, IR, l);

    udelay(37);
}

static void hd44780_write_data(struct hd44780 *lcd, u8 data)
{
    u8 h = (data >> 4) & 0x0F;
    u8 l = data & 0x0F;

    hd44780_write_nibble(lcd, DR, h);
    hd44780_write_nibble(lcd, DR, l);

    udelay(37 + 4);
}

static void hd44780_write_char(struct hd44780 *lcd, char ch)
{
    struct hd44780_geometry *geo = lcd->geometry;

    hd44780_write_data(lcd, ch);

    lcd->pos.col++;

    if (lcd->pos.col == geo->cols) {
        lcd->pos.row = (lcd->pos.row + 1) % geo->rows;
        lcd->pos.col = 0;
        hd44780_write_instruction(lcd, HD44780_DDRAM_ADDR | geo->start_addrs[lcd->pos.row]);
    }
}

static void hd44780_clear_display(struct hd44780 *lcd)
{
    hd44780_write_instruction(lcd, HD44780_CLEAR_DISPLAY);

    /* Wait for 1.64 ms because this one needs more time */
    udelay(1640);

    /*
     * CLEAR_DISPLAY instruction also returns cursor to home,
     * so we need to update it locally.
     */
    lcd->pos.row = 0;
    lcd->pos.col = 0;
}

static void hd44780_clear_line(struct hd44780 *lcd)
{
    struct hd44780_geometry *geo;
    int start_addr, col;

    geo = lcd->geometry;
    start_addr = geo->start_addrs[lcd->pos.row];

    hd44780_write_instruction(lcd, HD44780_DDRAM_ADDR | start_addr);

    for (col = 0; col < geo->cols; col++)
        hd44780_write_data(lcd, ' ');

    hd44780_write_instruction(lcd, HD44780_DDRAM_ADDR | start_addr);
}

static void hd44780_handle_new_line(struct hd44780 *lcd)
{
    struct hd44780_geometry *geo = lcd->geometry;

    // clear to end of line
    int pos;
    for (pos=lcd->pos.col;pos<geo->cols;pos++) {
        hd44780_write_data(lcd, ' ');
    }

    lcd->pos.row = (lcd->pos.row + 1) % geo->rows;
    lcd->pos.col = 0;
    hd44780_write_instruction(lcd, HD44780_DDRAM_ADDR
        | geo->start_addrs[lcd->pos.row]);
    hd44780_clear_line(lcd);
}

static void hd44780_handle_carriage_return(struct hd44780 *lcd)
{
    struct hd44780_geometry *geo = lcd->geometry;

    lcd->pos.col = 0;
    hd44780_write_instruction(lcd, HD44780_DDRAM_ADDR
        | geo->start_addrs[lcd->pos.row]);
}

static void hd44780_leave_esc_seq(struct hd44780 *lcd)
{
    memset(lcd->esc_seq_buf.buf, 0, ESC_SEQ_BUF_SIZE);
    lcd->esc_seq_buf.length = 0;
    lcd->is_in_esc_seq = false;
}

static void hd44780_flush_esc_seq(struct hd44780 *lcd)
{
    /* Write \e that initiated current esc seq */
    hd44780_write_char(lcd, '\e');

    /* Flush current esc seq */
    hd44780_write(lcd, lcd->esc_seq_buf.buf, lcd->esc_seq_buf.length);

    hd44780_leave_esc_seq(lcd);
}

/*
VT100 sequence buffer parser
*/
static int hd44780_parse_vt100_buff(struct hd44780 *lcd) {
    char* idx= lcd->esc_seq_buf.buf;
    int numlen;
    int num1=-1;
    int num2=-1;
    struct hd44780_geometry *geo = lcd->geometry;
    int pos;

    // skip the '['
    idx++;
    numlen=strspn( idx, "0123456789");
    if (numlen)
    {
        if (numlen>1)
        {
            // first number too long
            printk (KERN_INFO "first number too long: %s \n", idx );
            hd44780_flush_esc_seq(lcd);
            return 0;
        }
        num1 = *idx - '0';
        idx++;
        if ( *idx == ';' ) {
            idx++;
            numlen=strspn( idx, "0123456789");
            if (numlen) {
                if (numlen>2)
                {
                    // second number too long
                    printk (KERN_INFO "second number too long: %s \n", idx );
                    hd44780_flush_esc_seq(lcd);
                    return 0;
                }
                num2 = *idx - '0';
                idx++;
                if (numlen==2) {
                    num2 = num2 * 10 + *idx - '0';
                    idx++;
                }
            }
        }
    } else {
        if (*idx == ';') {
            idx++;
        }
    }
    switch( *idx ) {
    case 'A':
    case 'B':
    case 'C':
    case 'D':
        if (num2 > -1) {
            // Not a valid escape sequence, shoud not have second number
            printk (KERN_INFO "Not a valid escape sequence, should not have second number: %s \n", idx );
            hd44780_flush_esc_seq(lcd);
            return 0;
        }
        if (num1 <= 0)
        {
            num1 = 1;
        }
        switch(*idx) {
        case 'A':
            lcd->pos.row -= num1;
            break;
        case 'B':
            lcd->pos.row += num1;
            break;
        case 'C':
            lcd->pos.col += num1;
            break;
        case 'D':
            lcd->pos.col -= num1;
            break;
        }
        lcd->pos.row %= geo->rows;
        lcd->pos.col %= geo->cols;
        hd44780_write_instruction(lcd, HD44780_DDRAM_ADDR | (geo->start_addrs[lcd->pos.row] + lcd->pos.col));
        hd44780_leave_esc_seq(lcd);
        return 1;

    case 'H':
        if (num1 > -1)
        {
            if (num1 <= 1)
            {
                num1 = 1;
            }
            if (num2 <= 1 )
            {
                num2 = 1;
            }
        } else {
            num1 = 1;
            num2 = 1;
        }
        lcd->pos.row = (num1-1) % geo->rows;
        lcd->pos.col = (num2-1) % geo->cols;

        if (lcd->pos.row == 0 && lcd->pos.col == 0) {
            hd44780_write_instruction(lcd, HD44780_RETURN_HOME);
        } else {
            hd44780_write_instruction(lcd, HD44780_DDRAM_ADDR | (geo->start_addrs[lcd->pos.row] + lcd->pos.col));
        }
        hd44780_leave_esc_seq(lcd);
        return 1;

    case 'J':
        if (num1 == 2) {
            if (num2 == -1) {
                // Clear screen
                int prev_row = lcd->pos.row;
                int prev_col = lcd->pos.col;
                hd44780_clear_display(lcd);
                hd44780_write_instruction(lcd, HD44780_DDRAM_ADDR | (geo->start_addrs[prev_row] + prev_col));
                hd44780_leave_esc_seq(lcd);
                return 1;
            } else {
                // Not a valid escape sequence, J has second number
                printk (KERN_INFO "Not a valid escape sequence, should not have second number: %s \n", idx );
                hd44780_flush_esc_seq(lcd);
                return 0;
            }
        } else {
            // Not a valid escape sequence, only 2J is supported
            printk (KERN_INFO "Not a valid escape sequence, sonly 2J supported: %s \n", idx );
            hd44780_flush_esc_seq(lcd);
            return 0;
        }
        break;
    case 'K':
        if (num2 > -1) {
            // Not a valid escape sequence, shoud not have second number
            printk (KERN_INFO "Not a valid escape sequence, should not have second number: %s \n", idx );
            hd44780_flush_esc_seq(lcd);
            return 0;
        }
        if (num1 <=0) {
            // Clear line from cursor right
            for (pos=lcd->pos.col;pos<geo->cols;pos++) {
                hd44780_write_data(lcd, ' ');
            }
        } else if (num1 == 1) {
            //Clear line from cursor left
            hd44780_write_instruction(lcd, HD44780_DDRAM_ADDR | geo->start_addrs[lcd->pos.row]);
            for (pos=lcd->pos.col;pos<lcd->pos.col;pos++) {
                hd44780_write_data(lcd, ' ');
            }

        } else if (num1 == 2){
            // Clear entire line
            hd44780_write_instruction(lcd, HD44780_DDRAM_ADDR | geo->start_addrs[lcd->pos.row]);
            for (pos=lcd->pos.col;pos<geo->cols;pos++) {
                hd44780_write_data(lcd, ' ');
            }
        } else {
            printk (KERN_INFO "Not a valid escape sequence, first number rang [0-2]: %s \n", idx );
            hd44780_flush_esc_seq(lcd);
            return 0;
        }
        // reposition
        hd44780_write_instruction(lcd, HD44780_DDRAM_ADDR | geo->start_addrs[lcd->pos.row+lcd->pos.col]);
        break;
    case 'm':
        if (num2 > -1) {
            // Not a valid escape sequence, m has second number
            hd44780_flush_esc_seq(lcd);
            return 0;
        }
        if (num1 < 0)
        {
            // turn off character modes
            hd44780_set_cursor_blink( lcd, false );
            hd44780_set_cursor_display( lcd, false );
            hd44780_leave_esc_seq(lcd);
            return 1;
        } else if (num1 == 4 ) {
            // underline mode
            hd44780_set_cursor_display( lcd, true );
            hd44780_leave_esc_seq(lcd);
            return 1;
        } else if (num1 == 5 ) {
            // blink mode
            hd44780_set_cursor_blink( lcd, true );
            hd44780_leave_esc_seq(lcd);
            return 1;
        } else {
            // not a valid number
            hd44780_flush_esc_seq(lcd);
            return 0;
        }
        break;

    default:
        hd44780_flush_esc_seq(lcd);
        return 0;
    }
    hd44780_flush_esc_seq(lcd);
    return 0;
}

/*
VT100 sequence buffer builder.  fills out lcd->esc_seq_buf
*/

static int hd44780_parse_vt100( char ch, struct hd44780 *lcd ) {
    lcd->esc_seq_buf.buf[ lcd->esc_seq_buf.length++ ] = ch;
    if (lcd->esc_seq_buf.length == 1)
    {
        if (lcd->esc_seq_buf.buf[0] != '[')
        {
            hd44780_flush_esc_seq(lcd);
        }
    } else if (lcd->esc_seq_buf.length == ESC_SEQ_BUF_SIZE ) {
        hd44780_flush_esc_seq(lcd);
    } else {
        // check for end character (not digit or semi-colon)
        if (!strchr( "0123456789;", ch )) {
            lcd->esc_seq_buf.buf[lcd->esc_seq_buf.length] = 0;
            // now we have a null terminated string parse it
            return hd44780_parse_vt100_buff( lcd );
        }
    }
    return 0;
}


void hd44780_flush(struct hd44780 *lcd)
{
    while (lcd->is_in_esc_seq)
        hd44780_flush_esc_seq(lcd);
}

static void hd44780_handle_esc_seq_char(struct hd44780 *lcd, char ch)
{
    if (hd44780_parse_vt100(ch, lcd)) {
        hd44780_leave_esc_seq(lcd);
    }
}

void hd44780_write(struct hd44780 *lcd, const char *buf, size_t count)
{
    size_t i;
    char ch;

    if (lcd->dirty) {
        hd44780_clear_display(lcd);
        lcd->dirty = false;
    }

    for (i = 0; i < count; i++) {
        ch = buf[i];

        if (lcd->is_in_esc_seq) {
            hd44780_handle_esc_seq_char(lcd, ch);
        } else {
            switch (ch) {
            case '\r':
                hd44780_handle_carriage_return(lcd);
                break;
            case '\n':
                hd44780_handle_new_line(lcd);
                break;
            case '\e':
                lcd->is_in_esc_seq = true;
                break;
            case 0x11: // ^S
                hd44780_set_backlight( lcd, false );
                break;
            case 0x13: // ^Q
                hd44780_set_backlight( lcd, true );
                break;
            default:
                hd44780_write_char(lcd, ch);
                break;
            }
        }
    }
}

void hd44780_print(struct hd44780 *lcd, const char *str)
{
    hd44780_write(lcd, str, strlen(str));
}

void hd44780_set_geometry(struct hd44780 *lcd, struct hd44780_geometry *geo)
{
    lcd->geometry = geo;

    if (lcd->is_in_esc_seq)
        hd44780_leave_esc_seq(lcd);

    hd44780_clear_display(lcd);
}

void hd44780_set_backlight(struct hd44780 *lcd, bool backlight)
{
    lcd->backlight = backlight;
    pcf8574_raw_write(lcd, backlight ? BL : 0x00);
}

static void hd44780_update_display_ctrl(struct hd44780 *lcd)
{

    hd44780_write_instruction(lcd, HD44780_DISPLAY_CTRL
        | HD44780_D_DISPLAY_ON
        | (lcd->cursor_display ? HD44780_C_CURSOR_ON
            : HD44780_C_CURSOR_OFF)
        | (lcd->cursor_blink ? HD44780_B_BLINK_ON
            : HD44780_B_BLINK_OFF));
}

void hd44780_set_cursor_blink(struct hd44780 *lcd, bool cursor_blink)
{
    lcd->cursor_blink = cursor_blink;
    hd44780_update_display_ctrl(lcd);
}

void hd44780_set_cursor_display(struct hd44780 *lcd, bool cursor_display)
{
    lcd->cursor_display= cursor_display;
    hd44780_update_display_ctrl(lcd);
}
void hd44780_init_lcd(struct hd44780 *lcd)
{
    hd44780_write_instruction_high_nibble(lcd, HD44780_FUNCTION_SET
        | HD44780_DL_8BITS);
    mdelay(5);

    hd44780_write_instruction_high_nibble(lcd, HD44780_FUNCTION_SET
        | HD44780_DL_8BITS);
    udelay(100);

    hd44780_write_instruction_high_nibble(lcd, HD44780_FUNCTION_SET
        | HD44780_DL_8BITS);

    hd44780_write_instruction_high_nibble(lcd, HD44780_FUNCTION_SET
        | HD44780_DL_4BITS);

    hd44780_write_instruction(lcd, HD44780_FUNCTION_SET | HD44780_DL_4BITS
        | HD44780_N_2LINES);

    hd44780_write_instruction(lcd, HD44780_DISPLAY_CTRL | HD44780_D_DISPLAY_ON
        | HD44780_C_CURSOR_ON | HD44780_B_BLINK_ON);

    hd44780_clear_display(lcd);

    hd44780_write_instruction(lcd, HD44780_ENTRY_MODE_SET
        | HD44780_ID_INCREMENT | HD44780_S_SHIFT_OFF);
}


/*
 * DRIVER STUFF
 */

#define CLASS_NAME  "hd44780"
#define NAME        "hd44780"
#define NUM_DEVICES 8



static struct class *hd44780_class;
static dev_t dev_no;
/* We start with -1 so that first returned minor is 0 */
static atomic_t next_minor = ATOMIC_INIT(-1);

static LIST_HEAD(hd44780_list);
static DEFINE_SPINLOCK(hd44780_list_lock);

/* Device attributes */

static ssize_t geometry_show(struct device *dev, struct device_attribute *attr,
        char *buf)
{
    struct hd44780 *lcd;
    struct hd44780_geometry *geo;

    lcd = dev_get_drvdata(dev);
    geo = lcd->geometry;

    return scnprintf(buf, PAGE_SIZE, "%dx%d\n", geo->cols, geo->rows);
}

static ssize_t geometry_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    struct hd44780 *lcd;
    struct hd44780_geometry *geo;
    int cols = 0, rows = 0, i;

    sscanf(buf, "%dx%d", &cols, &rows);

    for (i = 0; hd44780_geometries[i] != NULL; i++) {
        geo = hd44780_geometries[i];

        if (geo->cols == cols && geo->rows == rows) {
            lcd = dev_get_drvdata(dev);

            mutex_lock(&lcd->lock);
            hd44780_set_geometry(lcd, geo);
            mutex_unlock(&lcd->lock);

            break;
        }
    }

    return count;
}
static DEVICE_ATTR_RW(geometry);

static ssize_t backlight_show(struct device *dev, struct device_attribute *attr,
        char *buf)
{
    struct hd44780 *lcd = dev_get_drvdata(dev);

    return scnprintf(buf, PAGE_SIZE, "%d\n", lcd->backlight);
}

static ssize_t backlight_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    struct hd44780 *lcd = dev_get_drvdata(dev);

    mutex_lock(&lcd->lock);
    hd44780_set_backlight(lcd, buf[0] == '1');
    mutex_unlock(&lcd->lock);

    return count;
}
static DEVICE_ATTR_RW(backlight);

static ssize_t cursor_blink_show(struct device *dev, struct device_attribute *attr,
        char *buf)
{
    struct hd44780 *lcd = dev_get_drvdata(dev);

    return scnprintf(buf, PAGE_SIZE, "%d\n", lcd->cursor_blink);
}

static ssize_t cursor_blink_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    struct hd44780 *lcd = dev_get_drvdata(dev);

    mutex_lock(&lcd->lock);
    hd44780_set_cursor_blink(lcd, buf[0] == '1');
    mutex_unlock(&lcd->lock);

    return count;
}
static DEVICE_ATTR_RW(cursor_blink);

static ssize_t cursor_display_show(struct device *dev, struct device_attribute *attr,
        char *buf)
{
    struct hd44780 *lcd = dev_get_drvdata(dev);

    return scnprintf(buf, PAGE_SIZE, "%d\n", lcd->cursor_display);
}

static ssize_t cursor_display_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    struct hd44780 *lcd = dev_get_drvdata(dev);

    mutex_lock(&lcd->lock);
    hd44780_set_cursor_display(lcd, buf[0] == '1');
    mutex_unlock(&lcd->lock);

    return count;
}
static DEVICE_ATTR_RW(cursor_display);


static ssize_t character_show(u8 charNum, struct device *dev, struct device_attribute *attr, char* buf )
{

    struct hd44780 *lcd = dev_get_drvdata(dev);

    u8 code[8];
    int idx;

    mutex_lock(&lcd->lock);
    hd44780_write_instruction( lcd, (u8) HD44780_CGRAM_ADDR | (charNum*8) );
    for (idx=0;idx<8;idx++)   {
        code[idx] = hd44780_read_data( lcd  );
    }
    mutex_unlock(&lcd->lock);

    printk (KERN_DEBUG "read hex= %X %X %X %X %X %X %X %X in character %i\n", code[0], code[1], code[2], code[3], code[4], code[5], code[6], code[7], charNum );

    for( idx=0;idx<8;idx++)
    {
        if (code[idx]<=9)
        {
            code[idx]='0'+code[idx];
        } else {
            code[idx]='A'-10+code[idx];
        }
    }
    printk (KERN_DEBUG "showing = %c%c%c%c%c%c%c%c from character %i\n", code[0], code[1], code[2], code[3], code[4], code[5], code[6], code[7], charNum );
    return scnprintf(buf, PAGE_SIZE, "%c%c%c%c%c%c%c%c\n", code[0], code[1], code[2], code[3], code[4], code[5], code[6], code[7]);
}

static ssize_t character_store(int charNum, struct device *dev, struct device_attribute *attr, const char* buf, size_t count )
{
    struct hd44780 *lcd = dev_get_drvdata(dev);

    u8 code[8];
    int idx;

    printk (KERN_DEBUG "storing = %c%c%c%c%c%c%c%c in character %i\n", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], charNum );

    // 8 chars + null
    if (count!=9) {
        return -EINVAL;
    }

    for( idx=0;idx<8;idx++)
    {
        if (buf[idx] <= '9')
        {
            if (buf[idx] >='0')
            {
                code[idx] = (u8)(buf[idx] - '0');
            }
            else {
                return -EINVAL;
            }
        }
        else if (buf[idx] <='V') {
            if (buf[idx] >='A') {
                code[idx] = (u8)(buf[idx] - 'A' + 10);
            }
            else
            {
                return -EINVAL;
            }
        }
    }
    printk (KERN_DEBUG "storing hex= %X %X %X %X %X %X %X %X in character %i\n", code[0], code[1], code[2], code[3], code[4], code[5], code[6], code[7], charNum );

    mutex_lock(&lcd->lock);
    hd44780_write_instruction( lcd, (u8) HD44780_CGRAM_ADDR | (charNum*8) );
    for (idx=0;idx<8;idx++)   {
        hd44780_write_data( lcd, code[idx]  );
    }
    mutex_unlock(&lcd->lock);
    
    return 9;
}

static ssize_t char0_store(struct device *dev, struct device_attribute *attr, const char* buf, size_t count ) {
    return character_store( 0, dev, attr, buf, count );
}
static ssize_t char0_show(struct device *dev, struct device_attribute *attr, char* buf ) {
    return character_show( 0, dev, attr, buf );
}
static DEVICE_ATTR_RW(char0);

static ssize_t char1_store(struct device *dev, struct device_attribute *attr, const char* buf, size_t count ) {
    return character_store( 1, dev, attr, buf, count );
}
static ssize_t char1_show(struct device *dev, struct device_attribute *attr, char* buf ) {
    return character_show( 1, dev, attr, buf );
}
static DEVICE_ATTR_RW(char1);

static ssize_t char2_store(struct device *dev, struct device_attribute *attr, const char* buf, size_t count ) {
    return character_store( 2, dev, attr, buf, count );
}
static ssize_t char2_show(struct device *dev, struct device_attribute *attr, char* buf ) {
    return character_show( 2, dev, attr, buf );
}
static DEVICE_ATTR_RW(char2);

static ssize_t char3_store(struct device *dev, struct device_attribute *attr, const char* buf, size_t count ) {
    return character_store( 3, dev, attr, buf, count );
}
static ssize_t char3_show(struct device *dev, struct device_attribute *attr, char* buf ) {
    return character_show( 3, dev, attr, buf );
}
static DEVICE_ATTR_RW(char3);

static ssize_t char4_store(struct device *dev, struct device_attribute *attr, const char* buf, size_t count ) {
    return character_store( 4, dev, attr, buf, count );
}
static ssize_t char4_show(struct device *dev, struct device_attribute *attr, char* buf ) {
    return character_show( 4, dev, attr, buf );
}
static DEVICE_ATTR_RW(char4);

static ssize_t char5_store(struct device *dev, struct device_attribute *attr, const char* buf, size_t count ) {
    return character_store( 5, dev, attr, buf, count );
}
static ssize_t char5_show(struct device *dev, struct device_attribute *attr, char* buf ) {
    return character_show( 5, dev, attr, buf );
}
static DEVICE_ATTR_RW(char5);

static ssize_t char6_store(struct device *dev, struct device_attribute *attr, const char* buf, size_t count ) {
    return character_store( 6, dev, attr, buf, count );
}
static ssize_t char6_show(struct device *dev, struct device_attribute *attr, char* buf ) {
    return character_show( 6, dev, attr, buf );
}
static DEVICE_ATTR_RW(char6);

static ssize_t char7_store(struct device *dev, struct device_attribute *attr, const char* buf, size_t count ) {
    return character_store( 7, dev, attr, buf, count );
}
static ssize_t char7_show(struct device *dev, struct device_attribute *attr, char* buf ) {
    return character_show( 7, dev, attr, buf );
}
static DEVICE_ATTR_RW(char7);

static struct attribute *hd44780_device_attrs[] = {
    &dev_attr_geometry.attr,
    &dev_attr_backlight.attr,
    &dev_attr_cursor_blink.attr,
    &dev_attr_cursor_display.attr,
    &dev_attr_char0.attr,
    &dev_attr_char1.attr,
    &dev_attr_char2.attr,
    &dev_attr_char3.attr,
    &dev_attr_char4.attr,
    &dev_attr_char5.attr,
    &dev_attr_char6.attr,
    &dev_attr_char7.attr,
    NULL
};
ATTRIBUTE_GROUPS(hd44780_device);

/* File operations */

static int hd44780_file_open(struct inode *inode, struct file *filp)
{
    filp->private_data = container_of(inode->i_cdev, struct hd44780, cdev);

    printk (KERN_DEBUG "opening %p on %p", filp->private_data, filp );

    return 0;
}

static int hd44780_file_release(struct inode *inode, struct file *filp)
{
    struct hd44780 *lcd = filp->private_data;
    printk (KERN_DEBUG "releasing %p on %p", filp->private_data, filp );
    return 0;
}

static ssize_t hd44780_file_write(struct file *filp, const char __user *buf, size_t count, loff_t *offp)
{
    struct hd44780 *lcd;
    size_t n;
    size_t written = 0;

    lcd = filp->private_data;

    mutex_lock(&lcd->lock);
    // TODO: Consider using an interruptible lock
    printk (KERN_DEBUG "writing %i bytes to %p", count, lcd );

    while (written<count) {
        n = min( count-written, (size_t)BUF_SIZE);

       // TODO: Support partial writes during errors?
       if (copy_from_user(lcd->buf, buf+written, n)) {
           mutex_unlock(&lcd->lock);
           return -EFAULT;
       }

       hd44780_write(lcd, lcd->buf, n);
       written += n;
    }
    if (lcd->is_in_esc_seq) {
        hd44780_flush_esc_seq(lcd);
    }
    printk (KERN_DEBUG "done writing %i bytes to %p", written, lcd );

    mutex_unlock(&lcd->lock);

    return written;
}

static void hd44780_init(struct hd44780 *lcd, struct hd44780_geometry *geometry,
        struct i2c_client *i2c_client)
{
    lcd->geometry = geometry;
    lcd->i2c_client = i2c_client;
    lcd->pos.row = 0;
    lcd->pos.col = 0;
    memset(lcd->esc_seq_buf.buf, 0, ESC_SEQ_BUF_SIZE);
    lcd->esc_seq_buf.length = 0;
    lcd->p_counter = 0;
    lcd->is_in_esc_seq = false;
    lcd->backlight = true;
    lcd->cursor_blink = true;
    lcd->cursor_display = true;
    mutex_init(&lcd->lock);
}

static struct file_operations fops = {
    .open = hd44780_file_open,
    .release = hd44780_file_release,
    .write = hd44780_file_write,
};

static int hd44780_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    dev_t devt;
    struct hd44780 *lcd;
    struct device *device;
    int ret, minor;

    minor = atomic_inc_return(&next_minor);
    devt = MKDEV(MAJOR(dev_no), minor);

    lcd = (struct hd44780 *)kmalloc(sizeof(struct hd44780), GFP_KERNEL);
    if (!lcd) {
        return -ENOMEM;
    }

    hd44780_init(lcd, hd44780_geometries[0], client);

    spin_lock(&hd44780_list_lock);
    list_add(&lcd->list, &hd44780_list);
    spin_unlock(&hd44780_list_lock);

    cdev_init(&lcd->cdev, &fops);
    ret = cdev_add(&lcd->cdev, devt, 1);
    if (ret) {
        pr_warn("Can't add cdev\n");
        goto exit;
    }

    device = device_create_with_groups(hd44780_class, NULL, devt, NULL,
        hd44780_device_groups, "lcd%d", MINOR(devt));

    if (IS_ERR(device)) {
        ret = PTR_ERR(device);
        pr_warn("Can't create device\n");
        goto del_exit;
    }

    dev_set_drvdata(device, lcd);
    lcd->device = device;

    hd44780_init_lcd(lcd);

    hd44780_print(lcd, "hd44780-i2c on /dev/");
    hd44780_print(lcd, device->kobj.name);
    lcd->dirty = true;

    return 0;

del_exit:
    cdev_del(&lcd->cdev);

    spin_lock(&hd44780_list_lock);
    list_del(&lcd->list);
    spin_unlock(&hd44780_list_lock);
exit:
    kfree(lcd);

    return ret;
}

static struct hd44780 * get_hd44780_by_i2c_client(struct i2c_client *i2c_client)
{
    struct hd44780 *lcd;

    spin_lock(&hd44780_list_lock);
    list_for_each_entry(lcd, &hd44780_list, list) {
        if (lcd->i2c_client->addr == i2c_client->addr) {
            spin_unlock(&hd44780_list_lock);
            return lcd;
        }
    }
    spin_unlock(&hd44780_list_lock);

    return NULL;
}


static int hd44780_remove(struct i2c_client *client)
{
    struct hd44780 *lcd;
    lcd = get_hd44780_by_i2c_client(client);
    device_destroy(hd44780_class, lcd->device->devt);
    cdev_del(&lcd->cdev);

    spin_lock(&hd44780_list_lock);
    list_del(&lcd->list);
    spin_unlock(&hd44780_list_lock);

    kfree(lcd);

    return 0;
}

static const struct i2c_device_id hd44780_id[] = {
    { NAME, 0},
    { }
};

static struct i2c_driver hd44780_driver = {
    .driver = {
        .name   = NAME,
        .owner  = THIS_MODULE,
    },
    .probe = hd44780_probe,
    .remove = hd44780_remove,
    .id_table = hd44780_id,
};

static int __init hd44780_mod_init(void)
{
    int ret;

    ret = alloc_chrdev_region(&dev_no, 0, NUM_DEVICES, NAME);
    if (ret) {
        pr_warn("Can't allocate chardev region");
        return ret;
    }

    hd44780_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(hd44780_class)) {
        ret = PTR_ERR(hd44780_class);
        pr_warn("Can't create %s class\n", CLASS_NAME);
        goto exit;
    }

    ret = i2c_add_driver(&hd44780_driver);
    if (ret) {
        pr_warn("Can't register I2C driver %s\n", hd44780_driver.driver.name);
        goto destroy_exit;
    }

    return 0;

destroy_exit:
    class_destroy(hd44780_class);
exit:
    unregister_chrdev_region(dev_no, NUM_DEVICES);

    return ret;
}
module_init(hd44780_mod_init);

static void __exit hd44780_mod_exit(void)
{
    i2c_del_driver(&hd44780_driver);
    class_destroy(hd44780_class);
    unregister_chrdev_region(dev_no, NUM_DEVICES);
}
module_exit(hd44780_mod_exit);

MODULE_AUTHOR("Claude Warren <claude@xenei.com>");
MODULE_DESCRIPTION("HD44780 I2C via PCF8574 driver");
MODULE_LICENSE("GPL");
