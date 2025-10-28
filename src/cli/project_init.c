/* Copyright (C) 2025 Nunuhara Cabbage <nunuhara@haniwa.technology>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include "limits.h"
#include "system4.h"
#include "system4/ain.h"
#include "system4/ex.h"
#include "system4/file.h"
#include "alice.h"
#include "alice/ain.h"
#include "alice/ex.h"
#include "alice/port.h"
#include "alice/project.h"
#include "cli.h"

enum {
	LOPT_MOD = 256,
};

static void mkdir_p_checked(const char *dir)
{
	NOTICE("mkdir -p \"%s\"", dir);
	if (mkdir_p(dir))
		ALICE_ERROR("mkdir_p(\"%s\"): %s", dir, strerror(errno));
}

static void chdir_checked(const char *dir)
{
	NOTICE("chdir \"%s\"", dir);
	if (chdir(dir))
		ALICE_ERROR("chdir(\"%s\"): %s", dir, strerror(errno));
}

static struct port port_open_checked(const char *path)
{
	NOTICE("[Writing %s]", path);
	struct port port;
	if (!port_file_open(&port, path))
		ALICE_ERROR("fopen(\"%s\"): %s", path, strerror(errno));
	return port;
}

static void dump_hll(struct ain *ain)
{
	// cd src/hll
	// alice ain dump --hll $SRCAIN > hll.inc
	// cd ../..
	chdir_checked("src/hll");

	struct port port = port_open_checked("hll.inc");

	port_printf(&port, "SystemSource = {\n");
	for (int i = 0; i < ain->nr_libraries; i++) {
		char *name = conv_output(ain->libraries[i].name);
		size_t name_len = strlen(name);
		port_printf(&port, "\"%s.hll\", \"%s\",\n", name, name);

		char *file_name = xmalloc(name_len + 5);
		memcpy(file_name, name, name_len);
		memcpy(file_name+name_len, ".hll", 5);

		struct port file_port = port_open_checked(file_name);
		ain_dump_library(&file_port, ain, i);
		port_close(&file_port);
		free(file_name);
		free(name);
	}
	port_printf(&port, "}\n");
	port_close(&port);

	chdir_checked("../..");
}

static bool dump_ex(struct ain *ain, const char *dir_name, const char *project_name,
		const char *ex_name)
{
	if (!file_exists(ex_name)) {
		NOTICE("[Couldn't find %s; skipping]", ex_name);
		return false;
	}

	struct ex *ex = ex_read_file(ex_name);
	if (!ex)
		ALICE_ERROR("ex_read_file(\"%s\") failed", ex_name);

	// alice ex dump --split -o ex/$PROJECT_NAME.x $EXPATH
	mkdir_p_checked("ex");

	char out_name[PATH_MAX];
	snprintf(out_name, PATH_MAX, "ex/%s.x", project_name);
	NOTICE("alice ex dump --split -o \"%s\" \"%s\"", out_name, ex_name);
	FILE *out = alice_open_output_file(out_name);
	ex_dump_split(out, ex, "ex");
	ex_free(ex);
	return true;
}

static char *get_project_name(const char *base_name)
{
	char *project_name = xstrdup(base_name);
	char *dot = strrchr(project_name, '.');
	if (dot)
		*dot = '\0';
	return project_name;
}

static char *get_ex_name(const char *dir, const char *project_name)
{
	char name[PATH_MAX];
	snprintf(name, PATH_MAX, "%s/%sEX.ex", dir, project_name);
	return xstrdup(name);
}

static char *get_pje_name(const char *project_name)
{
	size_t project_name_len = strlen(project_name);
	char *pje = xmalloc(project_name_len + 5);
	memcpy(pje, project_name, project_name_len);
	memcpy(pje+project_name_len, ".pje", 5);
	return pje;
}

static int command_project_init(int argc, char *argv[])
{
	bool is_mod = false;
	while (1) {
		int c = alice_getopt(argc, argv, &cmd_project_init);
		switch (c) {
		case 'm':
		case LOPT_MOD:
			is_mod = true;
			break;
		}
		if (c == -1)
			break;
	}

	argc -= optind;
	argv += optind;

	if (argc != 1)
		USAGE_ERROR(&cmd_project_init, "Wrong number of arguments");

	if (!file_exists(argv[0]))
		ALICE_ERROR("File does not exist: '%s'", argv[0]);

	int error;
	struct ain *ain = ain_open(argv[0], &error);
	if (!ain)
		ALICE_ERROR("Error opening ain file '%s': %s", argv[0], ain_strerror(error));

	// e.g. Rance10.ain
	char *base = xstrdup(path_basename(argv[0]));
	// e.g. /path/to/Rance10
	char *dir = xstrdup(path_dirname(argv[0]));
	// e.g. Rance10
	char *project_name = get_project_name(base);
	char *pje_name = get_pje_name(project_name);
	char *ex_name = is_mod ? get_ex_name(dir, project_name) : NULL;
	bool have_ex = false;

	char version[16];
	if (ain->minor_version)
		snprintf(version, 16, "%d.%d", ain->version, ain->minor_version);
	else
		snprintf(version, 16, "%d", ain->version);

	mkdir_p_checked("out");
	mkdir_p_checked("src/hll");
	dump_hll(ain);

	if (is_mod) {
		NOTICE("cp \"%s\" src.ain", argv[0]);
		file_copy(argv[0], "src.ain");
		have_ex = dump_ex(ain, dir, project_name, ex_name);
	}

	// TODO: copy game files if option given

	static const char *pje_fmt =
	"ProjectName = \"%s\"\n"
	"CodeName = \"%s.ain\"\n"
	"CodeVersion = \"%s\"\n"
	"\n"
	"SourceDir = \"src\"\n"
	"OutputDir = \"out\"\n"
	"\n"
	"Source = {\n"
	"    \"src.inc\",\n"
	"}\n";
	static const char *mod_pje_fmt =
	"ProjectName = \"%s\"\n"
	"CodeName = \"%s.ain\"\n"
	"\n"
	"SourceDir = \"src\"\n"
	"OutputDir = \"out\"\n"
	"\n"
	"Source = {\n"
	"    \"src.inc\",\n"
	"}\n"
	"\n"
	"ModJam = {\n"
	"//    \"mod.jam\",\n"
	"}\n"
	"\n"
	"ModAin = \"src.ain\"\n"
	"\n"
	"Archives = {\n"
	"//    \"%sModCG.manifest\",\n"
	"//    \"%sModFlat.manifest\",\n"
	"//    \"%sModSound.manifest\",\n"
	"//    \"%sPact.manifest\",\n"
	"}\n";
	static const char *mod_pje_ex_fmt =
	"\n"
	"ExInput = \"ex/%s.x\"\n"
	"ExName = \"%sEX.ex\"\n";

	struct port out = port_open_checked(pje_name);
	if (is_mod) {
		port_printf(&out, mod_pje_fmt, project_name, project_name, project_name,
				project_name, project_name, project_name);
		if (have_ex)
			port_printf(&out, mod_pje_ex_fmt, project_name, project_name);
	} else {
		port_printf(&out, pje_fmt, project_name, project_name, version);
	}
	port_close(&out);

	if (is_mod) {
		size_t len = strlen(project_name);
		char manifest_name[PATH_MAX];
		memcpy(manifest_name, project_name, len);

		memcpy(manifest_name+len, "ModCG.manifest", strlen("ModCG.manifest")+1);
		out = port_open_checked(manifest_name);
		port_printf(&out,
			"#BATCHPACK\n"
			"out/%sModCG.afa\n"
			"png,png,qnt,qnt\n",
			project_name);
		port_close(&out);

		memcpy(manifest_name+len, "ModFlat.manifest", strlen("ModCG.manifest")+1);
		out = port_open_checked(manifest_name);
		port_printf(&out,
			"#BATCHPACK\n"
			"out/%sModFlat.afa\n"
			"flatsrc,txtex,flat,flat\n",
			project_name);
		port_close(&out);

		memcpy(manifest_name+len, "ModSound.manifest", strlen("ModSound.manifest")+1);
		out = port_open_checked(manifest_name);
		port_printf(&out,
			"#BATCHPACK\n"
			"out/%sModSound.afa\n"
			"ogg,ogg,ogg,ogg\n"
			"timemap,txtex,timemap_ex,ex\n",
			project_name);
		port_close(&out);

		memcpy(manifest_name+len, "Pact.manifest", strlen("Pact.manifest")+1);
		out = port_open_checked(manifest_name);
		port_printf(&out,
			"#BATCHPACK\n"
			"out/%sPact.afa\n"
			"txtex,txtex,pactex,pactex\n",
			project_name);
		port_close(&out);
	}

	static const char *src_inc =
	"SystemSource = {\n"
	"    \"hll/hll.inc\",\n"
	"}\n"
	"\n"
	"Source = {\n"
	"    \"main.jaf\",\n"
	"}\n";
	out = port_open_checked("src/src.inc");
	port_printf(&out, "%s", src_inc);
	port_close(&out);

	static const char *basic_main_jaf =
	"int main(void)\n"
	"{\n" \
	"    system.MsgBox(\"Hello, world!\");\n"
	"    system.Exit(0);\n"
	"}\n"
	"\n"
	"void message(int nMsgNum, int nNumofMsg, string szText)\n"
	"{\n"
	"    system.Output(\"%d/%d: %s\" % nMsgNum % nNumofMsg % szText);\n"
	"}\n";
	static const char *v12_main_jaf =
	"float message::detail::GetMessageSpeedRate(void)\n"
	"{\n"
	"    return 1.0;\n"
	"}\n"
	"\n"
	"void message::detail::GetReadMessageTextColor(ref int Red, ref int Green, ref int Blue)\n"
	"{\n"
	"    Red = 255;\n"
	"    Green = 0;\n"
	"    Blue = 0;\n"
	"}\n";
	static const char *mod_main_jaf =
	"override void game_main(void)\n"
	"{\n"
	"    //AFL_AFA_CG_Add(\"%sModCG\");\n"
	"    //AFL_AFA_Flat_Add(\"%sModCG\");\n"
	"    //AFL_AFA_Sound_Add(\"%sModCG\");\n"
	"    super();\n"
	"}\n";
	// XXX: in Rance 10, we hook a different function so that the mod's AFA files
	//      are loaded last
	static const char *rance10_main_jaf =
	"override void LoadAdditionalAFAFile(void)\n"
	"{\n"
	"    super();\n"
	"    //AFL_AFA_CG_Add(\"%sModCG\");\n"
	"    //AFL_AFA_Flat_Add(\"%sModCG\");\n"
	"    //AFL_AFA_Sound_Add(\"%sModCG\");\n"
	"}\n";

	out = port_open_checked("src/main.jaf");
	if (is_mod) {
		if (ain_get_function(ain, "LoadAdditionalAFAFile") > 0)
			port_printf(&out, rance10_main_jaf, project_name, project_name, project_name);
		else
			port_printf(&out, mod_main_jaf, project_name, project_name, project_name);
	} else {
		if (AIN_VERSION_GTE(ain, 12, 0))
			port_printf(&out, "%s\n%s", basic_main_jaf, v12_main_jaf);
		else
			port_printf(&out, "%s", basic_main_jaf);
	}
	port_close(&out);

#ifdef _WIN32
	out = port_open_checked("build.bat");
	port_printf(&out,
		"alice.exe project build %s\n"
		"pause\n",
		pje_name);
	port_close(&out);
#else
	out = port_open_checked("build.sh");
	port_printf(&out,
		"#!/bin/sh\n"
		"${ALICE:-alice} project build \"%s\"\n",
		pje_name);
	port_close(&out);

	out = port_open_checked("run.sh");
	port_printf(&out, "%s",
		"#!/bin/sh\n"
		"cd out\n"
		"env LANG=ja_JP.UTF-8 wine System40.exe\n");
	port_close(&out);

	out = port_open_checked("xrun.sh");
	port_printf(&out, "%s",
		"#!/bin/sh\n"
		"cd out\n"
		"${XSYSTEM4:-xsystem4}\n");
	port_close(&out);

	NOTICE("chmod 0755 build.sh run.sh xrun.sh");
	mode_t mode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
	chmod("build.sh", mode);
	chmod("run.sh", mode);
	chmod("xrun.sh", mode);
#endif

	free(ex_name);
	free(pje_name);
	free(project_name);
	free(dir);
	free(base);
	ain_free(ain);
	return 0;
}

struct command cmd_project_init = {
	.name = "init",
	.usage = "[options...] <input-file>",
	.description = "Initialize a .pje project",
	.parent = &cmd_project,
	.fun = command_project_init,
	.options = {
		{ "mod", 'm', "Initialize a mod project", no_argument, LOPT_MOD },
		{ 0 }
	}
};
