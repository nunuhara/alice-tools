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
#include "system4/ex.h"
#include "system4/file.h"
#include "system4/ini.h"
#include "system4/string.h"
#include "alice.h"
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
	const char *pje_path;
	struct string *pje_dir;
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
	// alice-tools extensions
	struct string *mod_ain;
	struct string *mod_text;
	struct string_list mod_jam;
	struct string *ex_input;
	struct string *ex_name;
	struct string *ex_mod_name;
	struct string_list archives;
	int major_version;
	int minor_version;
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
	struct string_list ex_source;
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

static struct string *pje_string_ptr(struct ini_entry *entry)
{
	if (entry->value.type != INI_STRING)
		ERROR("Invalid value for '%s': not a string", entry->name->text);
	return entry->value.s;
}

static struct string *pje_string(struct ini_entry *entry)
{
	return string_dup(pje_string_ptr(entry));
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
	struct string *pje_dir = directory_name(path);

	config->major_version = 4;
	config->minor_version = 0;

	for (int i = 0; i < ini_size; i++) {
		if (!strcmp(ini[i].name->text, "ProjectName")) {
			config->project_name = pje_string(&ini[i]);
		} else if (!strcmp(ini[i].name->text, "CodeName")) {
			config->code_name = pje_string(&ini[i]);
		} else if (!strcmp(ini[i].name->text, "GameVersion")) {
			config->game_version = pje_integer(&ini[i]);
		} else if (!strcmp(ini[i].name->text, "SourceDir")) {
			config->source_dir = string_path_join(pje_dir, pje_string_ptr(&ini[i])->text);
		} else if (!strcmp(ini[i].name->text, "HLLDir")) {
			config->hll_dir = string_path_join(pje_dir, pje_string_ptr(&ini[i])->text);
		} else if (!strcmp(ini[i].name->text, "ObjDir")) {
			config->obj_dir = string_path_join(pje_dir, pje_string_ptr(&ini[i])->text);
		} else if (!strcmp(ini[i].name->text, "OutputDir")) {
			config->output_dir = string_path_join(pje_dir, pje_string_ptr(&ini[i])->text);
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
		} else if (!strcmp(ini[i].name->text, "ModJam")) {
			pje_string_list(&ini[i], &config->mod_jam);
		} else if (!strcmp(ini[i].name->text, "ExInput")) {
			config->ex_input = string_path_join(pje_dir, pje_string_ptr(&ini[i])->text);
		} else if (!strcmp(ini[i].name->text, "ExName")) {
			config->ex_name = pje_string(&ini[i]);
		} else if (!strcmp(ini[i].name->text, "ExModName")) {
			config->ex_mod_name = pje_string(&ini[i]);
		} else if (!strcmp(ini[i].name->text, "Archives")) {
			pje_string_list(&ini[i], &config->archives);
		} else if (!strcmp(ini[i].name->text, "CodeVersion")) {
			if (!parse_version(pje_string_ptr(&ini[i])->text, &config->major_version, &config->minor_version)) {
				ALICE_ERROR("Invalid CodeVersion");
			}
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
	if (!config->source_dir) {
		config->source_dir = string_path_join(pje_dir, "source");
	}
	if (!config->output_dir) {
		config->output_dir = string_path_join(pje_dir, "run");
	}
	if (!config->hll_dir) {
		config->hll_dir = string_path_join(pje_dir, "hll");
	}
	if (!config->obj_dir) {
		config->obj_dir = string_path_join(pje_dir, "obj");
	}

	config->pje_path = path;
	config->pje_dir = pje_dir;
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
	struct string *file = string_path_join(dir, inc->text);
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
			string_list_append(system ? &job->system_source : &job->source, string_path_join(dir, source->items[i]->text));
		} else if (!strcmp(ext, "hll")) {
			if (i+1 >= source->n)
				ERROR("Missing HLL name in source list: %s", source->items[i]->text);
			if (strchr(source->items[i+1]->text, '.'))
				ERROR("HLL name contains '.': %s", source->items[i+1]->text);
			string_list_append(&job->headers, string_path_join(dir, source->items[i]->text));
			string_list_append(&job->headers, string_dup(source->items[i+1]));
			i++;
		} else if (!strcmp(ext, "txtex") || !strcmp(ext, "x")) {
			string_list_append(&job->ex_source, string_path_join(dir, source->items[i]->text));
		} else {
			ERROR("Unhandled file extension in source list: '%s'", ext);
		}
	}
}

static void pje_free(struct pje_config *config)
{
	if (config->pje_dir)
		free_string(config->pje_dir);
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
	if (config->mod_ain)
		free_string(config->mod_ain);
	if (config->ex_input)
		free_string(config->ex_input);
	if (config->ex_name)
		free_string(config->ex_name);
	if (config->ex_mod_name)
		free_string(config->ex_mod_name);
	free_string_list(&config->system_source);
	free_string_list(&config->source);
	free_string_list(&config->mod_jam);
	free_string_list(&config->archives);
}

static void build_job_free(struct build_job *job)
{
	free_string_list(&job->system_source);
	free_string_list(&job->source);
	free_string_list(&job->headers);
	free_string_list(&job->ex_source);
}

static void pje_build_ain(struct pje_config *config, struct build_job *job)
{
	struct string *output_file = string_path_join(config->output_dir, config->code_name->text);
	NOTICE("AIN    %s", output_file->text);

	unsigned nr_header_files = job->headers.n;
	const char **header_files = xcalloc(nr_header_files, sizeof(const char*));
	for (unsigned i = 0; i < job->headers.n; i++) {
		header_files[i] = job->headers.items[i]->text;
	}

	// consolidate files into list
	// TODO: It's unclear from the docs if ALL SystemSource files should be compiled
	//       before ALL Source files. This becomes an issue if .inc files are used
        //       which contain both SystemSource and Source lists. E.g., should
	//       SystemSource files included from the top-level Source list be compiled
	//       before Source files included from the top-level SystemSource list?
	//       This needs to be confirmed before implementing global constructors.
	unsigned nr_source_files = job->system_source.n + job->source.n;
	const char **source_files = xcalloc(nr_source_files, sizeof(const char*));
	for (unsigned i = 0; i < job->system_source.n; i++) {
		source_files[i] = job->system_source.items[i]->text;
	}
	for (unsigned i = 0; i < job->source.n; i++) {
		source_files[job->system_source.n + i] = job->source.items[i]->text;
	}

	// open/create ain object
	struct ain *ain;
	if (config->mod_ain) {
		int err;
		struct string *mod_ain = string_path_join(config->pje_dir, config->mod_ain->text);
		if (!(ain = ain_open(mod_ain->text, &err))) {
			ALICE_ERROR("Failed to open ain file: %s", ain_strerror);
		}
		ain_init_member_functions(ain, conv_output_utf8);
		free_string(mod_ain);
	} else {
		ain = ain_new(config->major_version, config->minor_version);
	}

	// apply text substitution, if ModText given
	if (config->mod_text) {
		struct string *mod_text = string_path_join(config->pje_dir, config->mod_text->text);
		read_text(mod_text->text, ain);
		free_string(mod_text);
	}

	// build
	jaf_build(ain, source_files, nr_source_files, header_files, nr_header_files);

	// build .jam files
	for (unsigned i = 0; i < config->mod_jam.n; i++) {
		struct string *mod_jam = string_path_join(config->source_dir, config->mod_jam.items[i]->text);
		asm_append_jam(mod_jam->text, ain, 0);
		free_string(mod_jam);
	}

	// write to disk
	ain_write(output_file->text, ain);

	free_string(output_file);
	free(source_files);
	free(header_files);
	ain_free(ain);
}

static bool is_ex_file(const char *name)
{
	char head[4];
	FILE *fp = checked_fopen(name, "rb");
	checked_fread(head, 4, fp);
	fclose(fp);

	return head[0] == 'H' && head[1] == 'E' && head[2] == 'A' && head[3] == 'D';
}

static struct ex *read_input_ex(struct string *ex_input)
{
	if (!ex_input)
		return NULL;

	struct ex *ex;
	if (is_ex_file(ex_input->text)) {
		NOTICE("EX     (input) %s", ex_input->text);
		if (!(ex = ex_read_file(ex_input->text)))
			ALICE_ERROR("Failed to read .ex file: '%s'", ex_input->text);
	} else {
		NOTICE("TXTEX  %s", ex_input->text);
		if (!(ex = ex_parse_file(ex_input->text)))
			ALICE_ERROR("Failed to parse .txtex file: '%s'", ex_input->text);
	}
	return ex;
}

static struct ex *read_source_ex(struct build_job *job)
{
	struct ex *source_ex = NULL;
	for (unsigned i = 0; i < job->ex_source.n; i++) {
		NOTICE("TXTEX  %s", job->ex_source.items[i]->text);
		struct ex *this_ex = ex_parse_file(job->ex_source.items[i]->text);
		if (!source_ex) {
			source_ex = this_ex;
		} else {
			ex_append(source_ex, this_ex);
			ex_free(this_ex);
		}
	}
	return source_ex;
}

/*
 * ExInput   - either a .ex or .txtex file
 * ExName    - name of the main .ex file
 * ExModName - name of the mod .ex file
 *
 * .txtex files in the source directory are either appended to ExInput, or (if
 * ExModName is given) written to a separate .ex file.
 *
 * If .txtex files are present in the source directory, either ExName or
 * ExModName must be given.
 *
 * If ExModName is given, ExInput must also be given, and there must be .txtex
 * files present in the source directory.
 */
static void pje_build_ex(struct pje_config *config, struct build_job *job)
{
	if (!config->ex_name && !config->ex_mod_name) {
		if (job->ex_source.n > 0)
			ALICE_ERROR("'%s': Ex source files found but ExName/ExModName not given", config->pje_path);
		return;
	}
	if (config->ex_mod_name && job->ex_source.n == 0)
		ALICE_ERROR("'%s': ExModName present but no .txtex files found in source directory", config->pje_path);

	// Read ExInput file; this can be either a .txtex file or a .ex file
	struct ex *ex = read_input_ex(config->ex_input);
	// Read txtex files from source directory
	struct ex *source_ex = read_source_ex(job);

	// If ExModName is given, txtex data is written to a separate .ex file;
	// otherwise the data is appended to the ExInput file.
	if (config->ex_mod_name) {
		// source files are for a mod .ex file
		if (ex && source_ex) {
			struct ex *tmp = ex_extract_append(ex, source_ex);
			ex_free(source_ex);
			source_ex = tmp;
		}
	} else {
		// source files are for the main .ex file
		if (ex && source_ex) {
			// appending to existing .ex file
			ex_append(ex, source_ex);
			ex_free(source_ex);
			source_ex = NULL;
		} else if (source_ex) {
			// creating new .ex file from sources
			ex = source_ex;
			source_ex = NULL;
		}
	}

	// Write the ExInput file to output directory
	if (ex && config->ex_name) {
		NOTICE("EX     %s", config->ex_name->text);
		struct string *out = string_path_join(config->output_dir, config->ex_name->text);
		ex_write_file(out->text, ex);
		free_string(out);
	}

	// Write the mod .ex file to output directory
	if (source_ex && config->ex_mod_name) {
		NOTICE("EX     %s", config->ex_mod_name->text);
		struct string *mod_out = string_path_join(config->output_dir, config->ex_mod_name->text);
		ex_write_file(mod_out->text, source_ex);
		free_string(mod_out);
	}

	if (ex)
		ex_free(ex);
	if (source_ex)
		ex_free(source_ex);
}

static void pje_build_archives(struct pje_config *config)
{
	for (unsigned i = 0; i < config->archives.n; i++) {
		struct string *path = string_path_join(config->pje_dir, config->archives.items[i]->text);
		NOTICE("AFA    %s", path->text);
		ar_pack(path->text, 2);
		free_string(path);
	}
}

void pje_build(const char *path)
{
	struct pje_config config = {0};
	pje_parse(path, &config);

	if (mkdir_p(config.output_dir->text)) {
		ALICE_ERROR("Creating output directory '%s': %s", config.output_dir->text, strerror(errno));
	}

	struct build_job job = {0};
	pje_read_source(&job, config.source_dir, &config.system_source, true);
	pje_read_source(&job, config.source_dir, &config.source, false);

	pje_build_ain(&config, &job);
	pje_build_ex(&config, &job);
	pje_build_archives(&config);
	build_job_free(&job);
	pje_free(&config);
}
