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
#include <libgen.h>
#include "system4.h"
#include "system4/ain.h"
#include "system4/file.h"
#include "system4/ini.h"
#include "system4/string.h"
#include "ainedit.h"
#include "jaf.h"

struct pje_formation {
	struct string *name;
	unsigned nr_defines;
	struct string **defines;
};

struct string_list {
	unsigned n;
	struct string **items;
};

struct pje_config {
	struct string *project_name;
	struct string *code_name;
	int game_version;
	struct string *source_dir;
	struct string *hll_dir;
	struct string *obj_dir;
	struct string *output_dir;
	unsigned nr_formations;
	struct pje_formation *formations;
	struct string_list system_source;
	struct string_list source;
	struct string *mod_ain;
	struct string *mod_text;
};

struct inc_config {
	struct string_list system_source;
	struct string_list source;
	struct string_list copy_to_run;
	struct string_list copy_to_dll;
	struct string_list copy_to_dp;
	struct string_list load_dll;
};

struct build_job {
	struct string_list system_source;
	struct string_list source;
	struct string_list headers;
};

static void string_list_append(struct string_list *list, struct string *str)
{
	list->items = xrealloc_array(list->items, list->n, list->n+1, sizeof(struct string*));
	list->items[list->n++] = str;
}

static void free_string_list(struct string_list *list)
{
	for (unsigned i = 0; i < list->n; i++) {
		free_string(list->items[i]);
	}
	free(list->items);
}

static struct string *directory_name(const char *path)
{
	char *tmp = strdup(path);
	char *dir = dirname(tmp);
	struct string *r = make_string(dir, strlen(dir));
	free(tmp);
	return r;
}

static struct string *directory_file(struct string *dir, struct string *file)
{
	char tmp[1024];
	if (dir->size + file->size + 2 >= 1024)
		ERROR("path too long");

	memcpy(tmp, dir->text, dir->size);
	tmp[dir->size] = '/';
	memcpy(tmp+dir->size+1, file->text, file->size);
	tmp[dir->size+file->size+1] = '\0';

	for (int i = dir->size+1; i < dir->size + file->size + 1; i++) {
		if (tmp[i] == '\\')
			tmp[i] = '/';
	}

	return make_string(tmp, dir->size+file->size+1);
}

static struct string *pje_string(struct ini_entry *entry)
{
	if (entry->value.type != INI_STRING)
		ERROR("Invalid value for '%s': not a string", entry->name->text);
	return string_dup(entry->value.s);
}

static int pje_integer(struct ini_entry *entry)
{
	if (entry->value.type != INI_INTEGER)
		ERROR("Invalid value for '%s': not an integer", entry->name->text);
	return entry->value.i;
}

static void pje_string_list(struct ini_entry *entry, struct string_list *out)
{
	if (entry->value.type != INI_LIST)
		ERROR("Invalid value for '%s': not a list", entry->name->text);

	out->n = entry->value.list_size;
	out->items = xcalloc(entry->value.list_size, sizeof(struct string*));

	for (unsigned i = 0; i < out->n; i++) {
		if (entry->value.list[i].type != INI_STRING)
			ERROR("Invalid value in '%s': not a string", entry->name->text);
		out->items[i] = string_dup(entry->value.list[i].s);
	}
}

static void pje_parse(const char *path, struct pje_config *config)
{
	int ini_size;
	struct ini_entry *ini = ini_parse(path, &ini_size);

	for (int i = 0; i < ini_size; i++) {
		if (!strcmp(ini[i].name->text, "ProjectName")) {
			config->project_name = pje_string(&ini[i]);
		} else if (!strcmp(ini[i].name->text, "CodeName")) {
			config->code_name = pje_string(&ini[i]);
		} else if (!strcmp(ini[i].name->text, "GameVersion")) {
			config->game_version = pje_integer(&ini[i]);
		} else if (!strcmp(ini[i].name->text, "SourceDir")) {
			config->source_dir = pje_string(&ini[i]);
		} else if (!strcmp(ini[i].name->text, "HLLDir")) {
			config->hll_dir = pje_string(&ini[i]);
		} else if (!strcmp(ini[i].name->text, "ObjDir")) {
			config->obj_dir = pje_string(&ini[i]);
		} else if (!strcmp(ini[i].name->text, "OutputDir")) {
			config->output_dir = pje_string(&ini[i]);
		} else if (!strcmp(ini[i].name->text, "SystemSource")) {
			pje_string_list(&ini[i], &config->system_source);
		} else if (!strcmp(ini[i].name->text, "Source")) {
			pje_string_list(&ini[i], &config->source);
		} else if (!strcmp(ini[i].name->text, "HLL")) {
			WARNING("HLL renaming not supported");
		} else if (!strcmp(ini[i].name->text, "SyncFolder")) {
			WARNING("SyncFolder not supported");
		} else if (!strcmp(ini[i].name->text, "SyncLock")) {
			WARNING("SyncLock not supported");
		} else if (!strcmp(ini[i].name->text, "ModAin")) {
			config->mod_ain = pje_string(&ini[i]);
		} else if (!strcmp(ini[i].name->text, "ModText")) {
			config->mod_text = pje_string(&ini[i]);
		} else if (ini[i].value.type == INI_FORMATION) {
			WARNING("Formations not supported");
		} else {
			ERROR("Unhandled pje variable: %s", ini[i].name->text);
		}
		ini_free_entry(&ini[i]);
	}
	free(ini);

	// defaults
	if (!config->project_name)
		config->project_name = cstr_to_string("UntitledProject");
	if (!config->code_name)
		config->code_name = cstr_to_string("out.ain");
	if (!config->source_dir)
		config->source_dir = cstr_to_string("source");
	if (!config->hll_dir)
		config->hll_dir = cstr_to_string("hll");
	if (!config->obj_dir)
		config->obj_dir = cstr_to_string("obj");
	if (!config->output_dir)
		config->output_dir = cstr_to_string("run");
}

static void pje_parse_inc(const char *path, struct inc_config *config)
{
	int ini_size;
	struct ini_entry *ini = ini_parse(path, &ini_size);

	for (int i = 0; i < ini_size; i++) {
		if (!strcmp(ini[i].name->text, "Source")) {
			pje_string_list(&ini[i], &config->source);
		} else if (!strcmp(ini[i].name->text, "SystemSource")) {
			pje_string_list(&ini[i], &config->system_source);
		} else if (!strcmp(ini[i].name->text, "CopyToRun")) {
			pje_string_list(&ini[i], &config->copy_to_run);
		} else if (!strcmp(ini[i].name->text, "CopyToDLL")) {
			pje_string_list(&ini[i], &config->copy_to_dll);
		} else if (!strcmp(ini[i].name->text, "CopyToDP")) {
			pje_string_list(&ini[i], &config->copy_to_dp);
		} else if (!strcmp(ini[i].name->text, "LoadDLL")) {
			pje_string_list(&ini[i], &config->load_dll);
		} else {
			ERROR("Unhandled variable in file '%s': %s", path, ini[i].name->text);
		}
		ini_free_entry(&ini[i]);
	}
	free(ini);
}

static void pje_free_inc(struct inc_config *config)
{
	free_string_list(&config->system_source);
	free_string_list(&config->source);
	free_string_list(&config->copy_to_run);
	free_string_list(&config->copy_to_dll);
	free_string_list(&config->copy_to_dp);
	free_string_list(&config->load_dll);
}

static const char *extname(const char *s)
{
	const char *e = strrchr(s, '.');
	return e ? e+1 : NULL;
}

static void pje_read_source(struct build_job *job, struct string *dir, struct string_list *source, bool system);

static void pje_read_inc(struct build_job *job, struct string *dir, struct string *inc)
{
	struct inc_config config = {0};
	struct string *file = directory_file(dir, inc);
	struct string *file_dir = directory_name(file->text);
	pje_parse_inc(file->text, &config);

	pje_read_source(job, file_dir, &config.system_source, true);
	pje_read_source(job, file_dir, &config.source, false);

	pje_free_inc(&config);
	free_string(file);
	free_string(file_dir);
}

static void pje_read_source(struct build_job *job, struct string *dir, struct string_list *source, bool system)
{
	for (unsigned i = 0; i < source->n; i++) {
		const char *ext = extname(source->items[i]->text);
		if (!strcmp(ext, "inc")) {
			pje_read_inc(job, dir, source->items[i]);
		} else if (!strcmp(ext, "jaf")) {
			string_list_append(system ? &job->system_source : &job->source, directory_file(dir, source->items[i]));
		} else if (!strcmp(ext, "hll")) {
			if (i+1 >= source->n)
				ERROR("Missing HLL name in source list: %s", source->items[i]->text);
			if (strchr(source->items[i+1]->text, '.'))
				ERROR("HLL name contains '.': %s", source->items[i+1]->text);
			string_list_append(&job->headers, directory_file(dir, source->items[i]));
			string_list_append(&job->headers, string_dup(source->items[i+1]));
			i++;
		} else {
			ERROR("Unhandled file extension in source list: '%s'", ext);
		}
	}
}

static void pje_free(struct pje_config *config)
{
	if (config->project_name)
		free_string(config->project_name);
	if (config->code_name)
		free_string(config->code_name);
	if (config->source_dir)
		free_string(config->source_dir);
	if (config->hll_dir)
		free_string(config->hll_dir);
	if (config->obj_dir)
		free_string(config->obj_dir);
	if (config->output_dir)
		free_string(config->output_dir);
	free_string_list(&config->system_source);
	free_string_list(&config->source);
}

static void build_job_free(struct build_job *job)
{
	free_string_list(&job->system_source);
	free_string_list(&job->source);
	free_string_list(&job->headers);
}

void pje_build(const char *path, int major_version, int minor_version)
{
	struct build_job job = {0};
	struct pje_config config = {0};
	pje_parse(path, &config);

	struct string *pje_dir = directory_name(path);
	struct string *src_dir = directory_file(pje_dir, config.source_dir);
	struct string *out_dir = directory_file(pje_dir, config.output_dir);
	if (mkdir_p(out_dir->text)) {
		ERROR("Creating output directory '%s': %s", out_dir->text, strerror(errno));
	}

	// collect source file names
	pje_read_source(&job, src_dir, &config.system_source, true);
	pje_read_source(&job, src_dir, &config.source, false);

	// TODO: parse hll files
	unsigned nr_header_files = job.headers.n;
	const char **header_files = xcalloc(nr_header_files, sizeof(const char*));
	for (unsigned i = 0; i < job.headers.n; i++) {
		header_files[i] = job.headers.items[i]->text;
	}

	// consolidate files into list
	// TODO: It's unclear from the docs if ALL SystemSource files should be compiled
	//       before ALL Source files. This becomes an issue if .inc files are used
        //       which contain both SystemSource and Source lists. E.g., should
	//       SystemSource files included from the top-level Source list be compiled
	//       before Source files included from the top-level SystemSource list?
	//       This needs to be confirmed before implementing global constructors.
	unsigned nr_source_files = job.system_source.n + job.source.n;
	const char **source_files = xcalloc(nr_source_files, sizeof(const char*));
	for (unsigned i = 0; i < job.system_source.n; i++) {
		source_files[i] = job.system_source.items[i]->text;
	}
	for (unsigned i = 0; i < job.source.n; i++) {
		source_files[job.system_source.n + i] = job.source.items[i]->text;
	}

	// open/create ain object
	struct ain *ain;
	if (config.mod_ain) {
		int err;
		struct string *mod_ain = directory_file(pje_dir, config.mod_ain);
		if (!(ain = ain_open(mod_ain->text, &err))) {
			ERROR("Failed to open ain file: %s", ain_strerror);
		}
		free_string(mod_ain);
	} else {
		ain = ain_new(major_version, minor_version);
	}

	// apply text substitution, if ModText given
	if (config.mod_text) {
		struct string *mod_text = directory_file(pje_dir, config.mod_text);
		read_text(mod_text->text, ain);
		free_string(mod_text);
	}

	// build
	jaf_build(ain, source_files, nr_source_files, header_files, nr_header_files);

	// write to disk
	NOTICE("Writing AIN file...");
	struct string *output_file = directory_file(out_dir, config.code_name);
	ain_write(output_file->text, ain);

	free_string(pje_dir);
	free_string(src_dir);
	free_string(out_dir);
	free_string(output_file);
	free(source_files);
	build_job_free(&job);
	pje_free(&config);
	ain_free(ain);
}
