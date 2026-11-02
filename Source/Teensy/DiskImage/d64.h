/* SPDX-License-Identifier: MIT */
/* Disk image engine (read + write) for .d64 (1541, 35/40 tracks), .d71 (1571, 70 tracks) and .d81 (1581, 80 tracks).
 * Portable C99, no malloc: all state lives in caller structs and sectors are fetched through a callback, so the same
 * code runs on the Teensy (SD/USB file backend) and on the host (tests, VICE mock). Names keep the d64_ prefix. */
#ifndef D64_H
#define D64_H
#include <stddef.h>
#include <stdint.h>

/* Placement: VD_COLD for code (nothing here is reachable from a bus interrupt), D64_FLASH for constant tables.
 * The firmware sets them to FLASHMEM / PROGMEM (DiskImage.h); on a host they are empty. */
#ifndef VD_COLD
#define VD_COLD
#endif
#ifndef D64_FLASH
#define D64_FLASH
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define D64_SECTOR 256

/* Fetch one 256-byte sector. Return 0 on success. */
typedef int (*d64_read_fn)(void *ctx, uint8_t track, uint8_t sector, uint8_t *buf);

/* Store one 256-byte sector. Return 0 on success. NULL = read-only disk. */
typedef int (*d64_write_fn)(void *ctx, uint8_t track, uint8_t sector, const uint8_t *buf);

enum {
    D64_OK = 0,
    D64_E_IO = -1,       /* sector read/write failed or link out of range */
    D64_E_LOOP = -2,     /* chain loop */
    D64_E_FULL = -3,     /* disk (or directory) full   -> DOS 72 */
    D64_E_EXISTS = -4,   /* file exists                -> DOS 63 */
    D64_E_NAME = -5,     /* bad file name              -> DOS 33 */
    D64_E_RO = -6,       /* read-only disk             -> DOS 26 */
    D64_E_LOCKED = -7,   /* file is locked             -> DOS 26 */
    D64_E_NOTFOUND = -8, /* file not found             -> DOS 62 */
    D64_E_TYPE = -9,     /* file type mismatch         -> DOS 64 */
    D64_E_TOOBIG = -10,  /* REL file at its size limit -> DOS 52 */
    D64_E_BADSEC = -11   /* the image's error table marks the sector bad: DOS code in d64_t.bad (20-24, 27, 29, 74) */
};

enum { D64_FMT_D64 = 0, D64_FMT_D71 = 1, D64_FMT_D81 = 2 };

typedef struct {
    d64_read_fn  read;
    d64_write_fn write;
    void       *ctx;
    uint8_t     tracks;          /* 35 or 40 (d64), 70 (d71), 80 (d81) */
    uint8_t     fmt;             /* D64_FMT_* */
    uint8_t     ro;              /* write protect set from the C64 (W-1): writes fail as on a read-only image; d64_open clears it */
    /* Error table (the byte per sector some images carry after the data, as written by copiers / VICE): NULL = none. A read of a
     * sector whose byte is a read error fails with D64_E_BADSEC, after the data was fetched into the buffer (as VICE does), and the
     * first such failure is kept in bad / bad_t / bad_s until the caller clears bad. Writes ignore the table. d64_open clears it. */
    const uint8_t *errtab;
    uint8_t     bad, bad_t, bad_s;
} d64_t;

typedef struct {
    uint8_t  type;               /* raw type byte: bit7 closed, bit6 locked, low 3 = DEL/SEQ/PRG/USR/REL */
    uint8_t  track, sector;      /* first data sector */
    uint16_t blocks;
    char     name[17];           /* PETSCII, $A0 padding stripped, NUL terminated */
    uint8_t  dir_t, dir_s, dir_slot;   /* where this entry lives (for update/scratch) */
    uint8_t  ss_t, ss_s, reclen;       /* REL: first side sector (D81: super side sector), record length */
} d64_entry_t;

typedef struct {
    uint8_t track, sector;       /* next directory sector in chain */
    uint8_t cur_t, cur_s;        /* sector held in buf */
    uint8_t slot;                /* next entry (0-7) in buf */
    uint8_t guard;
    uint8_t buf[D64_SECTOR];
} d64_dir_t;

typedef struct {
    uint8_t  next_t, next_s;     /* next sector in chain (0,x = none) */
    uint8_t  loaded;             /* buf holds a sector */
    uint16_t pos, end;           /* read cursor / one past last valid byte in buf */
    uint16_t guard;
    uint8_t  buf[D64_SECTOR];
} d64_file_t;

typedef struct {
    uint8_t  type;               /* DEL/SEQ/PRG/USR (low 3 bits) */
    uint8_t  dir_t, dir_s, dir_slot;
    uint8_t  first_t, first_s;
    uint8_t  cur_t, cur_s;       /* sector being filled */
    uint16_t pos;                /* next free index in buf (2..256) */
    uint16_t blocks;             /* sectors allocated so far */
    uint8_t  app_t, app_s;       /* append: the file's old last sector (0 = a new file), */
    uint8_t  app_end, app_type;  /*   its old last-byte index and the old directory type byte, */
    uint16_t app_blocks;         /*   and its old length: a failed append puts all of it back */
    uint8_t  buf[D64_SECTOR];
} d64_wfile_t;

/* Relative (REL) file: fixed-length records over a normal data chain, indexed by side sectors.
 *   d64/d71: up to 6 side sectors (720 data blocks); each holds its number, the record length, the list of all 6 and
 *            120 data-block pointers. d81: a super side sector (in the directory entry) points to up to 126 such groups.
 * New records are $FF followed by zeros; growing the file fills its last block with whole empty records, as DOS does.
 * One data block is cached in buf (written back by d64_rel_flush or when another block is needed). */
typedef struct {
    uint8_t  dir_t, dir_s, dir_slot;
    uint8_t  ss_t, ss_s;         /* first side sector (d81: super side sector) */
    uint8_t  lss_t, lss_s;       /* last side sector */
    uint8_t  last_t, last_s;     /* last data block */
    uint8_t  reclen;
    uint8_t  last;               /* byte 1 of the last data block: index of the last used byte */
    uint16_t nblocks, nss;       /* data blocks, side sectors (without the super side sector) */
    uint16_t cur;                /* block index held in buf, 0xFFFF = none */
    uint8_t  cur_t, cur_s, dirty;
    uint8_t  buf[D64_SECTOR];
} d64_rel_t;

/* Memory-image backend (host tests / mock). Writable if data is. */
typedef struct { uint8_t *data; size_t len; } d64_mem_t;
void d64_mem_init(d64_mem_t *m, uint8_t *data, size_t len);
int  d64_open_mem(d64_t *d, d64_mem_t *m);          /* image size selects the format */
int  d64_open(d64_t *d, d64_read_fn rd, void *ctx, uint8_t tracks);   /* tracks: 35/40 = d64, 70 = d71, 80 = d81 */
int  d64_open_size(d64_t *d, d64_read_fn rd, void *ctx, size_t file_size);   /* format from the file size (error bytes allowed); -1 if too small */
/* Error table: d64_errtab_offset = where it starts in a file of this size (-1: the file has none; after d64_open_size), d64_sectors =
 * the image's sector count (= the table's length). d64_open_mem sets errtab itself. d64_errcode: an error byte -> DOS code, 0 = no
 * read error (1 = OK; 2..6, 9, 11, 15 = 20..24, 27, 29, 74; the write errors 25 / 26 / 28 and unknown bytes read fine). */
long d64_errtab_offset(const d64_t *d, size_t file_size);
int  d64_sectors(const d64_t *d);
int  d64_errcode(uint8_t errbyte);

int  d64_sectors_in_track(const d64_t *d, uint8_t track);   /* 0 if invalid */
long d64_sector_offset(const d64_t *d, uint8_t track, uint8_t sector); /* byte offset in image, -1 if invalid */

/* Disk header: name (16 PETSCII, stripped), id (2), dos type (2), NUL terminated. */
int  d64_header(d64_t *d, char name[17], char id[3], char dos[3]);
int  d64_blocks_free(d64_t *d);                     /* 1541 convention: track 18 not counted */

int  d64_dir_first(d64_t *d, d64_dir_t *it);
int  d64_dir_next(d64_t *d, d64_dir_t *it, d64_entry_t *e);   /* 1 entry, 0 end, <0 error */

/* CBM wildcard match: '?' any one char, '*' rest of name. */
int  d64_match(const char *name, const char *pattern);
int  d64_find(d64_t *d, const char *pattern, d64_entry_t *e); /* 1 found, 0 not found, <0 error */

int  d64_file_open(d64_t *d, const d64_entry_t *e, d64_file_t *f);
int  d64_file_read(d64_t *d, d64_file_t *f, uint8_t *out, size_t n); /* bytes read; 0 = EOF; <0 error */
int  d64_file_eof(const d64_file_t *f);             /* 1 when no more bytes will follow */

/* BASIC directory listing (load address $0401), optional name pattern (NULL = all).
 * Returns length written, or 0 if cap is too small. */
size_t d64_build_dir(d64_t *d, const char *pattern, uint8_t *out, size_t cap);

/* ---- write support ----
 * d64_create allocates the first sector and a directory slot (file shows as
 * unclosed "splat" until d64_close_write). replace!=0 scratches an existing
 * file of that name first (DOS "@:"). */
int  d64_create(d64_t *d, const char *name, uint8_t type, int replace, d64_wfile_t *w);
int  d64_write(d64_t *d, d64_wfile_t *w, const uint8_t *data, size_t n); /* n or <0; on D64_E_FULL the file is removed */
int  d64_close_write(d64_t *d, d64_wfile_t *w);
/* DOS ",A": reopen a file for writing at its end (the entry shows as unclosed until d64_close_write). D64_E_NOTFOUND,
 * D64_E_TYPE for a REL file. On D64_E_FULL the file is put back as it was before the append. */
int  d64_append(d64_t *d, const char *name, d64_wfile_t *w);
int  d64_scratch_entry(d64_t *d, const d64_entry_t *e);          /* frees chain, marks entry deleted */
int  d64_scratch(d64_t *d, const char *pattern);                 /* returns files scratched or <0 */
int  d64_rename(d64_t *d, const char *oldname, const char *newname);
int  d64_format(d64_t *d, const char *name, const char *id);     /* id==NULL: quick (BAM + directory only) */
int  d64_format_blank(d64_t *d, const char *name, const char *id);   /* an all-zero new image: header, BAM, first directory sector only */
/* DOS "V": rebuild the BAM from the directory, the data chains and REL side sectors; unclosed (splat) files are deleted.
 * Returns the number of files deleted, or <0. A broken chain is followed as far as it is valid. */
int  d64_validate(d64_t *d);
/* lock flag (bit 6 of the type: no scratch) of every closed file matching pattern: lock=1 set, 0 clear, -1 toggle.
 * Returns the number of files changed, or <0. */
int  d64_lock(d64_t *d, const char *pattern, int lock);
/* DOS "C:new=old1,old2,...": new file, type of the first source, holding the sources one after the other. Sources are
 * patterns (first match); D64_E_NOTFOUND if one is missing, D64_E_EXISTS if newname is taken, D64_E_TYPE for a REL source
 * among several. On failure nothing is left behind (D64_E_FULL included). */
int  d64_copy(d64_t *d, const char *newname, const char *const *src, int nsrc);

/* ---- REL files ---- */
int      d64_rel_create(d64_t *d, const char *name, uint8_t reclen, d64_rel_t *r);   /* reclen 1..254; one block of empty records */
int      d64_rel_open(d64_t *d, const d64_entry_t *e, d64_rel_t *r);
uint32_t d64_rel_records(const d64_rel_t *r);
int      d64_rel_extend(d64_t *d, d64_rel_t *r, uint32_t nrec);    /* make records 1..nrec exist; D64_E_TOOBIG / D64_E_FULL */
int      d64_rel_get(d64_t *d, d64_rel_t *r, uint32_t off, uint8_t *b); /* byte at offset off of the record data */
int      d64_rel_put(d64_t *d, d64_rel_t *r, uint32_t off, uint8_t b);
int      d64_rel_flush(d64_t *d, d64_rel_t *r);

/* ---- raw blocks (direct access: U1/U2, B-A, B-F) ----
 * Sector numbers are checked against the geometry: D64_E_IO for an illegal track/sector or a failed transfer. */
int  d64_block_read(d64_t *d, uint8_t t, uint8_t s, uint8_t *buf);
int  d64_block_write(d64_t *d, uint8_t t, uint8_t s, const uint8_t *buf);   /* D64_E_RO on a read-only image */
/* Mark t/s used in the BAM. 0 = allocated; D64_E_EXISTS = already used, nt and ns = the next free sector (0/0 = none);
 * D64_E_IO = illegal. */
int  d64_block_alloc(d64_t *d, uint8_t t, uint8_t s, uint8_t *nt, uint8_t *ns);
int  d64_block_free(d64_t *d, uint8_t t, uint8_t s);                          /* mark t/s free in the BAM */

/* Generic BASIC-listing builder (also used for host directory listings). */
typedef struct { uint8_t *p; size_t cap, len, base; uint16_t link_at; int ok; } d64_bas_t;
void   d64_bas_begin(d64_bas_t *b, uint8_t *out, size_t cap);
/* Incremental form: build one line at a time into a small buffer. base = bytes already emitted
 * (load address included) so the line link pointers stay right; base==0 also emits the load address. */
void   d64_bas_begin_at(d64_bas_t *b, uint8_t *out, size_t cap, size_t base);
void   d64_bas_header(d64_bas_t *b, const char *name, const char *id, const char *dos);
void   d64_bas_entry(d64_bas_t *b, uint16_t blocks, const char *name, const char *type3, int closed, int locked);
void   d64_bas_footer(d64_bas_t *b, uint16_t free_blocks);
size_t d64_bas_end(d64_bas_t *b);                    /* length, or 0 if the buffer overflowed */

size_t d64_sizeof_d64(void);  size_t d64_sizeof_mem(void);
size_t d64_sizeof_dir(void);  size_t d64_sizeof_wfile(void);  size_t d64_sizeof_file(void);  size_t d64_sizeof_entry(void);
size_t d64_sizeof_rel(void);

#ifdef __cplusplus
}
#endif
#endif
