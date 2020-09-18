/* Copyright (C) 2019 Nunuhara Cabbage <nunuhara@haniwa.technology>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include "little_endian.h"
#include "system4.h"
#include "system4/archive.h"
#include "system4/buffer.h"
#include "system4/cg.h"
#include "system4/qnt.h"
#include "system4/ajp.h"

/*
 * Makeshift AFA extraction for new AFA format with encrypted TOC.
 */

struct file_magic {
	const char * const ext;
	const char * const magic;
	size_t len;
	bool (*skip)(struct buffer *r);
};

static bool skip_ajp(struct buffer *r);
static bool skip_qnt(struct buffer *r);
static bool skip_dcf(struct buffer *r);
static bool skip_flat(struct buffer *r);
static bool skip_zlb(struct buffer *r);
static bool skip_ex(struct buffer *r);
static bool skip_ogg(struct buffer *r);

static struct file_magic file_magic[] = {
	{ ".ajp",  "AJP",     4, skip_ajp  },
	{ ".qnt",  "QNT",     4, skip_qnt  },
	{ ".dcf",  "dcf\x20", 4, skip_dcf  },
	{ ".flat", "ELNA",    4, skip_flat },
	{ ".zlb",  "ZLB",     4, skip_zlb  },
	{ ".ex",   "HEAD",    4, skip_ex   },
	{ ".ogg",  "OggS",    4, skip_ogg  },
	// TODO: ogg
};

struct afa3_entry {
	size_t off;
	size_t size;
};

struct afa3_archive {
	struct archive ar;
	char *filename;
	size_t file_size;
	uint32_t data_off;
	unsigned nr_files;
	struct afa3_entry *files;
	void *mmap_ptr;
};

static bool afa3_exists(struct archive *ar, int no);
static bool afa3_exists_by_name(struct archive *ar, const char *name);
static struct archive_data *afa3_get(struct archive *_ar, int no);
static struct archive_data *afa3_get_by_name(struct archive *_ar, const char *name);
static bool afa3_load_file(struct archive_data *data);
static void afa3_for_each(struct archive *_ar, void (*iter)(struct archive_data *data, void *user), void *user);
static void afa3_free_data(struct archive_data *ar);
static void afa3_free(struct archive *ar);

struct archive_ops afa3_archive_ops = {
	.exists = afa3_exists,
	.exists_by_name = afa3_exists_by_name,
	.get = afa3_get,
	.get_by_name = afa3_get_by_name,
	.load_file = afa3_load_file,
	.for_each = afa3_for_each,
	.free_data = afa3_free_data,
	.free = afa3_free,
};

static const char *get_file_extension(char *data)
{
	for (size_t i = 0; i < sizeof(file_magic)/sizeof(*file_magic); i++) {
		if (!strncmp(data, file_magic[i].magic, file_magic[i].len))
			return file_magic[i].ext;
	}
	return ".dat";
}

static int _afa3_get_by_name(struct afa3_archive *ar, const char *name)
{
	char filename[512];
	for (unsigned i = 0; i < ar->nr_files; i++) {
		snprintf(filename, 512, "%u%s", i, get_file_extension(ar->mmap_ptr+ar->files[i].off));
		if (!strcmp(name, filename))
			return i;
	}
	return -1;
}

static bool afa3_exists(struct archive *ar, int no)
{
	return (unsigned)no < ((struct afa3_archive*)ar)->nr_files;
}

static bool afa3_exists_by_name(struct archive *ar, const char *name)
{
	return _afa3_get_by_name((struct afa3_archive*)ar, name) >= 0;
}

static struct archive_data *afa3_make_data(struct afa3_archive *ar, unsigned no)
{
	struct afa3_entry *e = &ar->files[no];
	struct archive_data *data = xcalloc(1, sizeof(struct archive_data));
	data->data = ar->mmap_ptr+e->off;
	data->size = e->size;
	data->name = xmalloc(256);
	snprintf(data->name, 256, "%d%s", no, get_file_extension((char*)data->data));
	data->no = no;
	data->archive = (struct archive*)ar;
	return data;
}

static struct archive_data *afa3_get(struct archive *_ar, int no)
{
	struct afa3_archive *ar = (struct afa3_archive*)_ar;
	if ((unsigned)no >= ar->nr_files)
		return NULL;
	return afa3_make_data(ar, no);
}

static struct archive_data *afa3_get_by_name(struct archive *_ar, const char *name)
{
	struct afa3_archive *ar = (struct afa3_archive*)_ar;
	int no = _afa3_get_by_name(ar, name);
	if (no < 0)
		return NULL;
	return afa3_make_data(ar, no);
}

static bool afa3_load_file(possibly_unused struct archive_data *data)
{
	return true;
}

static void afa3_for_each(struct archive *_ar, void (*iter)(struct archive_data *data, void *user), void *user)
{
	struct afa3_archive *ar = (struct afa3_archive*)_ar;
	for (unsigned i = 0; i < ar->nr_files; i++) {
		struct archive_data *data = afa3_get(_ar, i);
		if (!data)
			continue;
		iter(data, user);
		afa3_free_data(data);
	}
}

static bool afa3_read_header(FILE *f, struct afa3_archive *ar, int *error)
{
	char buf[12];
	if (fread(buf, 12, 1, f) != 1) {
		*error = ARCHIVE_FILE_ERROR;
		return false;
	}

	fseek(f, 0, SEEK_END);
	ar->file_size = ftell(f);
	fseek(f, 0, SEEK_SET);

	if (strncmp(buf, "AFAH", 4) || LittleEndian_getDW((uint8_t*)buf, 8) != 0x3) {
		*error = ARCHIVE_BAD_ARCHIVE_ERROR;
		return false;
	}

	ar->data_off = LittleEndian_getDW((uint8_t*)buf, 4);
	return true;
}

static bool skip_ajp(struct buffer *r)
{
	// FIXME: validate
	size_t off = r->index;
	buffer_skip(r, 20);
	uint32_t jpeg_off = buffer_read_int32(r);
	uint32_t jpeg_size = buffer_read_int32(r);
	uint32_t mask_off = buffer_read_int32(r);
	uint32_t mask_size = buffer_read_int32(r);
	uint32_t size = jpeg_off + jpeg_size;
	if (mask_size && mask_off > jpeg_off) {
		size = mask_off + mask_size;
	}
	buffer_seek(r, off+size);
	return true;
}

static bool skip_qnt(struct buffer *r)
{
	// FIXME: validate
	struct qnt_header qnt;
	qnt_extract_header(r->buf+r->index, &qnt);
	buffer_seek(r, r->index + qnt.hdr_size + qnt.pixel_size + qnt.alpha_size);
	return true;
}

static bool skip_dcf(struct buffer *r)
{
	// FIXME: validate
	buffer_skip(r, 4);
	uint32_t size = buffer_read_int32(r);
	buffer_skip(r, size);

	// dfdl header
	if (strncmp(buffer_strdata(r), "dfdl", 4)) {
		WARNING("DCF magic2 incorrect");
		return false;
	}
	buffer_skip(r, 4);
	size = buffer_read_int32(r);
	buffer_skip(r, size);

	// dcgd header
	if (strncmp(buffer_strdata(r), "dcgd", 4)) {
		WARNING("DCF magic3 incorrect");
		return false;
	}
	buffer_skip(r, 4);
	size = buffer_read_int32(r);
	buffer_skip(r, size);
	return true;
}

static bool is_flat_section(const char *data)
{
	if (!strncmp(data, "FLAT", 4))
		return true;
	if (!strncmp(data, "TMNL", 4))
		return true;
	if (!strncmp(data, "MTLC", 4))
		return true;
	if (!strncmp(data, "LIBL", 4))
		return true;
	if (!strncmp(data, "TALT", 4))
		return true;
	return false;
}

static bool skip_flat(struct buffer *r)
{
	// FIXME: validate
	size_t size;
	buffer_skip(r, 8);

	while (is_flat_section(buffer_strdata(r))) {
		buffer_skip(r, 4);
		size = buffer_read_int32(r);
		buffer_skip(r, size);
	}
	return true;
}

static bool skip_zlb(struct buffer *r)
{
	// FIXME: validate
	buffer_skip(r, 12);
	unsigned size = buffer_read_int32(r);
	buffer_skip(r, size);
	return true;
}

static bool skip_ex(struct buffer *r)
{
	// FIXME: validate
	buffer_skip(r, 8);
	if (strncmp(buffer_strdata(r), "EXTF", 4)) {
		WARNING("Missing ex file EXTF header at 0x%x", (unsigned)r->index);
		return false;
	}
	buffer_skip(r, 12);
	if (strncmp(buffer_strdata(r), "DATA", 4)) {
		WARNING("Missing ex file DATA header at 0x%x", (unsigned)r->index);
		return false;
	}
	buffer_skip(r, 4);

	unsigned size = buffer_read_int32(r);
	buffer_skip(r, size+4);
	return true;
}

enum {
	OGG_INVALID,
	OGG_START,
	OGG_END,
	OGG_OTHER,
};

static int skip_ogg_page(struct buffer *r)
{
	if (buffer_remaining(r) < 27) {
		WARNING("Ogg page truncated?");
		return OGG_INVALID;
	}

	// read header
	uint8_t *header = (uint8_t*)buffer_strdata(r);
	uint8_t type = header[5];
	uint8_t nr_segments = header[26];
	buffer_skip(r, 27);

	if (buffer_remaining(r) < nr_segments) {
		WARNING("Ogg page truncated?");
		return OGG_INVALID;
	}

	// read segment table
	uint8_t *segment_table = xmalloc(nr_segments);
	memcpy(segment_table, buffer_strdata(r), nr_segments);
	buffer_skip(r, nr_segments);

	// skip segments
	for (unsigned i = 0; i < nr_segments; i++) {
		if (buffer_remaining(r) < segment_table[i]) {
			WARNING("Ogg segment truncated?");
			return OGG_INVALID;
		}
		buffer_skip(r, segment_table[i]);
	}

	if (type & 2)
		return OGG_START;
	if (type & 4)
		return OGG_END;
	return OGG_OTHER;
}

static bool skip_ogg(struct buffer *r)
{
	int type = skip_ogg_page(r);
	if (type == OGG_INVALID)
		return false;
	if (type != OGG_START) {
		WARNING("First page in Ogg file has wrong type: %d", type);
		return false;
	}

	while ((type = skip_ogg_page(r)) != OGG_END) {
		if (type == OGG_INVALID)
			return false;
		if (type == OGG_START) {
			WARNING("Begging of stream page in middle of Ogg file?");
			return false;
		}
	}

	return true;
}

static bool afa3_read_file(struct buffer *r, struct afa3_entry *file)
{
	file->off = r->index;
	char *magic = buffer_strdata(r);
	for (size_t i = 0; i < sizeof(file_magic)/sizeof(*file_magic); i++) {
		if (!strncmp(magic, file_magic[i].magic, file_magic[i].len)) {
			file_magic[i].skip(r);
			file->size = r->index - file->off;
			return true;
		}
	}
	return false;
}

static bool buffer_seek_next_file(struct buffer *r)
{
	while (r->index < r->size - 4) {
		for (size_t i = 0; i < sizeof(file_magic)/sizeof(*file_magic); i++) {
			if (!strncmp((char*)r->buf+r->index, file_magic[i].magic, file_magic[i].len)) {
				return true;
			}
		}
		r->index++;
	}
	return false;
}

static void afa3_read_files(struct afa3_archive *ar)
{
	struct buffer r;
	buffer_init(&r, ar->mmap_ptr, ar->file_size);
	buffer_seek(&r, ar->data_off+8);

	int nr_alloc = 256;
	int file_no = 0;
	ar->files = xcalloc(nr_alloc, sizeof(struct afa3_entry));

	while (r.index+4 < ar->file_size) {
		if (file_no >= nr_alloc) {
			ar->files = xrealloc_array(ar->files, nr_alloc, nr_alloc+256, sizeof(struct afa3_entry));
			nr_alloc += 256;
		}
		if (afa3_read_file(&r, &ar->files[file_no])) {
			file_no++;
		} else {
			unsigned old_index = r.index;
			WARNING("UNKNOWN FILE TYPE at 0x%x (magic: %02x %02x %02x %02x)", (unsigned)r.index,
				r.buf[r.index], r.buf[r.index+1], r.buf[r.index+2], r.buf[r.index+3]);
			// try to find next file (brute force)
			if (!buffer_seek_next_file(&r))
				break;
			WARNING("Skipped %u bytes", r.index - old_index);
		}
	}

	ar->nr_files = file_no;
}

struct afa3_archive *afa3_open(const char *file, int flags, int *error)
{
#ifdef _WIN32
	flags &= ~ARCHIVE_MMAP;
#endif
	FILE *fp = NULL;
	struct afa3_archive *ar = xcalloc(1, sizeof(struct afa3_archive));

	if (!(fp = fopen(file, "rb"))) {
		WARNING("fopen failed: %s", strerror(errno));
		*error = ARCHIVE_FILE_ERROR;
		goto exit_err;
	}
	if (!afa3_read_header(fp, ar, error)) {
		WARNING("afa3_read_header failed");
		fclose(fp);
		goto exit_err;
	}

	// read archive into memory
	if (flags & ARCHIVE_MMAP) {
		int fd = open(file, O_RDONLY);
		if (fd < 0) {
			WARNING("open failed: %s", strerror(errno));
			*error = ARCHIVE_FILE_ERROR;
			goto exit_err;
		}
		ar->mmap_ptr = mmap(0, ar->file_size, PROT_READ, MAP_SHARED, fd, 0);
		close(fd);
		if (ar->mmap_ptr == MAP_FAILED) {
			WARNING("mmap failed: %s", strerror(errno));
			*error = ARCHIVE_FILE_ERROR;
			goto exit_err;
		}
		ar->ar.mmapped = true;
	} else {
		ar->mmap_ptr = xmalloc(ar->file_size);
		if (fread(ar->mmap_ptr, ar->file_size, 1, fp) != 1) {
			WARNING("fread failed: %s", strerror(errno));
			*error = ARCHIVE_FILE_ERROR;
			goto exit_err;
		}
	}

	afa3_read_files(ar);
	ar->filename = strdup(file);
	ar->ar.ops = &afa3_archive_ops;
	return ar;
exit_err:
	if (fp)
		fclose(fp);
	free(ar);
	return NULL;
}

static void afa3_free_data(struct archive_data *data)
{
	free(data->name);
	free(data);
}

static void afa3_free(struct archive *_ar)
{
	struct afa3_archive *ar = (struct afa3_archive*)_ar;
	if (_ar->mmapped) {
		munmap(ar->mmap_ptr, ar->file_size);
	} else {
		free(ar->mmap_ptr);
	}
	free(ar->filename);
	free(ar->files);
	free(ar);
}
