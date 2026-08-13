#include "py32emu/firmware/image.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_error(char *error, size_t size, const char *message)
{
    if (error != NULL && size > 0u) snprintf(error, size, "%s", message);
}

void py32_firmware_image_init(Py32FirmwareImage *image)
{
    memset(image, 0, sizeof(*image));
}

void py32_firmware_image_free(Py32FirmwareImage *image)
{
    size_t i;
    for (i = 0; i < image->symbol_count; ++i) free(image->symbols[i].name);
    free(image->symbols);
    free(image->data);
    py32_firmware_image_init(image);
}

static uint16_t read_le16(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t read_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool range_inside(size_t offset, size_t size, size_t total)
{
    return offset <= total && size <= total - offset;
}

static char *copy_string(const char *text, size_t maximum)
{
    size_t length = 0;
    char *result;
    while (length < maximum && text[length] != '\0') ++length;
    if (length == maximum) return NULL;
    result = malloc(length + 1u);
    if (result != NULL) memcpy(result, text, length + 1u);
    return result;
}

bool py32_firmware_load_elf(Py32FirmwareImage *image, const char *path,
                            uint32_t flash_base, size_t flash_size,
                            char *error, size_t error_size)
{
    FILE *file;
    long file_length;
    uint8_t *file_data = NULL, *flash = NULL;
    Py32FirmwareSymbol *symbols = NULL;
    size_t symbol_count = 0, used = 0;
    uint32_t phoff, shoff;
    uint16_t phentsize, phnum, shentsize, shnum;
    unsigned i;

    file = fopen(path, "rb");
    if (file == NULL) { set_error(error, error_size, "无法打开 ELF 固件"); return false; }
    if (fseek(file, 0, SEEK_END) != 0 || (file_length = ftell(file)) < 0 ||
        fseek(file, 0, SEEK_SET) != 0) goto malformed;
    file_data = malloc((size_t)file_length);
    flash = malloc(flash_size);
    if (file_data == NULL || flash == NULL) {
        set_error(error, error_size, "无法分配 ELF 映像"); goto fail;
    }
    if (fread(file_data, 1, (size_t)file_length, file) != (size_t)file_length)
        goto malformed;
    fclose(file); file = NULL;
    if (file_length < 52 || memcmp(file_data, "\x7F" "ELF", 4) != 0 ||
        file_data[4] != 1u || file_data[5] != 1u ||
        read_le16(file_data + 18) != 40u) goto malformed;
    phoff = read_le32(file_data + 28); shoff = read_le32(file_data + 32);
    phentsize = read_le16(file_data + 42); phnum = read_le16(file_data + 44);
    shentsize = read_le16(file_data + 46); shnum = read_le16(file_data + 48);
    if (phentsize < 32u || !range_inside(phoff, (size_t)phentsize * phnum,
                                         (size_t)file_length)) goto malformed;
    memset(flash, 0xFF, flash_size);
    for (i = 0; i < phnum; ++i) {
        const uint8_t *ph = file_data + phoff + (size_t)i * phentsize;
        uint32_t offset, address, filesz;
        uint64_t end;
        if (read_le32(ph) != 1u) continue;
        offset = read_le32(ph + 4); address = read_le32(ph + 12);
        if (address == 0u) address = read_le32(ph + 8);
        filesz = read_le32(ph + 16); end = (uint64_t)address + filesz;
        /* RAM 初始化段的内容由启动代码从其 Flash LMA 复制，PADDR 即 LMA。 */
        if (address < flash_base || end > (uint64_t)flash_base + flash_size ||
            !range_inside(offset, filesz, (size_t)file_length)) goto malformed;
        memcpy(flash + (address - flash_base), file_data + offset, filesz);
        if ((size_t)(address - flash_base) + filesz > used)
            used = (size_t)(address - flash_base) + filesz;
    }

    if (shnum != 0u) {
        if (shentsize < 40u || !range_inside(shoff, (size_t)shentsize * shnum,
                                             (size_t)file_length)) goto malformed;
        for (i = 0; i < shnum; ++i) {
            const uint8_t *sh = file_data + shoff + (size_t)i * shentsize;
            uint32_t offset, size, link, entsize;
            const uint8_t *linked, *strtab;
            uint32_t str_offset, str_size;
            size_t count, j;
            if (read_le32(sh + 4) != 2u) continue; /* SHT_SYMTAB */
            offset = read_le32(sh + 16); size = read_le32(sh + 20);
            link = read_le32(sh + 24); entsize = read_le32(sh + 36);
            if (entsize < 16u || link >= shnum ||
                !range_inside(offset, size, (size_t)file_length)) goto malformed;
            linked = file_data + shoff + (size_t)link * shentsize;
            str_offset = read_le32(linked + 16); str_size = read_le32(linked + 20);
            if (!range_inside(str_offset, str_size, (size_t)file_length)) goto malformed;
            strtab = file_data + str_offset; count = size / entsize;
            symbols = calloc(count, sizeof(*symbols));
            if (symbols == NULL && count != 0u) goto fail;
            for (j = 0; j < count; ++j) {
                const uint8_t *sym = file_data + offset + j * entsize;
                uint32_t name = read_le32(sym), address = read_le32(sym + 4);
                unsigned type = sym[12] & 0x0Fu;
                char *copied;
                if ((type != 1u && type != 2u) || name >= str_size || address == 0u)
                    continue;
                copied = copy_string((const char *)strtab + name, str_size - name);
                if (copied == NULL) goto fail;
                symbols[symbol_count].address = address;
                symbols[symbol_count].size = read_le32(sym + 8);
                symbols[symbol_count].name = copied;
                ++symbol_count;
            }
            break;
        }
    }
    free(file_data);
    py32_firmware_image_free(image);
    image->data = flash; image->size = used; image->load_address = flash_base;
    image->symbols = symbols; image->symbol_count = symbol_count;
    return true;

malformed:
    set_error(error, error_size, "ELF32 ARM 文件格式或装载范围无效");
fail:
    if (file != NULL) fclose(file);
    if (symbols != NULL) {
        for (i = 0; i < symbol_count; ++i) free(symbols[i].name);
    }
    free(symbols); free(file_data); free(flash); return false;
}

const Py32FirmwareSymbol *py32_firmware_find_symbol(
    const Py32FirmwareImage *image, uint32_t address)
{
    const Py32FirmwareSymbol *best = NULL;
    size_t i;
    if (image == NULL) return NULL;
    for (i = 0; i < image->symbol_count; ++i) {
        const Py32FirmwareSymbol *symbol = &image->symbols[i];
        if (address >= symbol->address &&
            (best == NULL || symbol->address > best->address) &&
            (symbol->size == 0u || address < symbol->address + symbol->size))
            best = symbol;
    }
    return best;
}

bool py32_firmware_load_bin(Py32FirmwareImage *image, const char *path,
                            uint32_t load_address, size_t maximum_size,
                            char *error, size_t error_size)
{
    FILE *file;
    long length;
    uint8_t *data;

    if (image == NULL || path == NULL) return false;
    file = fopen(path, "rb");
    if (file == NULL) {
        set_error(error, error_size, "无法打开固件文件");
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0 || (length = ftell(file)) < 0 ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        set_error(error, error_size, "无法确定固件大小");
        return false;
    }
    if ((size_t)length > maximum_size) {
        fclose(file);
        set_error(error, error_size, "固件超过目标芯片 Flash 容量");
        return false;
    }
    data = length == 0 ? NULL : malloc((size_t)length);
    if (length > 0 && (data == NULL ||
        fread(data, 1, (size_t)length, file) != (size_t)length)) {
        free(data);
        fclose(file);
        set_error(error, error_size, "读取固件失败");
        return false;
    }
    fclose(file);
    py32_firmware_image_free(image);
    image->data = data;
    image->size = (size_t)length;
    image->load_address = load_address;
    return true;
}

static int hex_digit(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static bool hex_byte(const char *text, uint8_t *value)
{
    int high = hex_digit((unsigned char)text[0]);
    int low = hex_digit((unsigned char)text[1]);
    if (high < 0 || low < 0) return false;
    *value = (uint8_t)((high << 4) | low);
    return true;
}

bool py32_firmware_load_hex(Py32FirmwareImage *image, const char *path,
                            uint32_t flash_base, size_t flash_size,
                            char *error, size_t error_size)
{
    FILE *file;
    uint8_t *data;
    char line[600];
    uint32_t upper = 0;
    size_t used = 0;
    unsigned line_number = 0;
    bool eof_seen = false;

    if (image == NULL || path == NULL || flash_size == 0u) return false;
    file = fopen(path, "r");
    if (file == NULL) { set_error(error, error_size, "无法打开 HEX 固件"); return false; }
    data = malloc(flash_size);
    if (data == NULL) { fclose(file); set_error(error, error_size, "无法分配固件映像"); return false; }
    memset(data, 0xFF, flash_size);
    while (fgets(line, sizeof(line), file) != NULL) {
        uint8_t bytes[260];
        uint8_t count, type;
        uint16_t low_address;
        unsigned i, total;
        uint8_t checksum = 0;
        uint64_t absolute;
        ++line_number;
        if (line[0] != ':' || !hex_byte(line + 1, &count)) goto malformed;
        total = (unsigned)count + 5u;
        if (strlen(line) < 1u + total * 2u) goto malformed;
        for (i = 0; i < total; ++i) {
            if (!hex_byte(line + 1u + i * 2u, &bytes[i])) goto malformed;
            checksum = (uint8_t)(checksum + bytes[i]);
        }
        if (checksum != 0u) { set_error(error, error_size, "HEX 记录校验和错误"); goto fail; }
        low_address = (uint16_t)(((uint16_t)bytes[1] << 8) | bytes[2]);
        type = bytes[3];
        if (type == 0u) {
            absolute = (uint64_t)upper + low_address;
            if (absolute < flash_base || absolute + count >
                (uint64_t)flash_base + flash_size) {
                set_error(error, error_size, "HEX 数据超出目标 Flash"); goto fail;
            }
            memcpy(data + (size_t)(absolute - flash_base), bytes + 4, count);
            if ((size_t)(absolute - flash_base) + count > used)
                used = (size_t)(absolute - flash_base) + count;
        } else if (type == 1u) {
            if (count != 0u) goto malformed;
            eof_seen = true; break;
        } else if (type == 4u) {
            if (count != 2u) goto malformed;
            upper = ((uint32_t)bytes[4] << 24) | ((uint32_t)bytes[5] << 16);
        } else if (type != 3u && type != 5u) {
            set_error(error, error_size, "HEX 包含不支持的记录类型"); goto fail;
        }
    }
    fclose(file);
    if (!eof_seen) { free(data); set_error(error, error_size, "HEX 缺少 EOF 记录"); return false; }
    py32_firmware_image_free(image);
    image->data = data; image->size = used; image->load_address = flash_base;
    return true;

malformed:
    if (error != NULL && error_size > 0u)
        snprintf(error, error_size, "HEX 第 %u 行格式错误", line_number);
fail:
    free(data); fclose(file); return false;
}
