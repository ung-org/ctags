/*
 * UNG's Not GNU
 *
 * Copyright (c) 2011-2017, Jakob Kaivo <jkk@ung.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#define _XOPEN_SOURCE 700
#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <search.h>
#include <unistd.h>

struct tag {
	const char *id;
	const char *file;
	char *text;
	int line;
};

void *tagroot = NULL;
FILE *output = NULL;

static int compar(const void *l, const void *r)
{
	const struct tag *left = l;
	const struct tag *right = r;
	return strcoll(left->id, right->id);
}

void addtag(const char *id, const char *file, char *text, int line)
{
	struct tag *t = calloc(1, sizeof(*t));
	if (t == NULL) {
		fprintf(stderr, "ctags: Out of memory\n");
		exit(1);
	}
	t->id = strdup(id);
	t->text = strdup(text);
	t->line = line;
	t->file = strdup(file);
	tsearch(t, &tagroot, compar);
}

static void checkcline(char *text, int line, const char *path)
{
	//static int comment = 0;
	static int bracket = 0;
	if (line == 1) {
		//comment = 0;
		bracket = 0;
	}

	if (strstr(text, "#define ")) {
		addtag("define", path, text, line);
	} else if (strstr(text, "typedef")) {
		addtag("typedef", path, text, line);
	} else if (bracket == 0) {
		addtag("identifier", path, text, line);
	}
}

static void checkfline(char *text, int line, const char *path)
{
	if (strstr(text, "FUNCTION")) {
		addtag("FUNCTION", path, text, line);
	}
}

static int addtags(const char *path)
{
	char *extension = strrchr(path, '.');
	enum { UNKNOWN, C, FORTRAN } filetype = UNKNOWN;

	if (extension == NULL) {
		fprintf(stderr, "ctags: Don't know how to process files without an extension (%s)\n", path);
		return 1;
	}

	if (!strcmp(extension, ".c") || !strcmp(extension, ".h")) {
		filetype = C;
	} else if (!strcmp(extension, ".f")) {
		filetype = FORTRAN;
	} else {
		fprintf(stderr, "ctags: Don't know how to process files with extension '%s'\n", extension);
		return 1;
	}

	FILE *f = fopen(path, "r");
	if (f == NULL) {
		fprintf(stderr, "ctags: Couldn't open %s: %s\n", path, strerror(errno));
		return 1;
	}

	int lineno = 0;

	while (!feof(f)) {
		char *line = NULL;
		size_t n = 0;
		getline(&line, &n, f);
		line[strlen(line)-1] = '\0';
		lineno++;

		if (filetype == FORTRAN) {
			checkfline(line, lineno, path);
		} else {
			checkcline(line, lineno, path);
		}

		free(line);
	}

	fclose(f);
	
	return 0;
}

static void writefile(const void *t, VISIT v, int level)
{
	(void)level;
	if (v != postorder && v != leaf) {
		return;
	}

	const struct tag *tag = *(const struct tag **)t;
	char *pattern = tag->text;
	fprintf(output, "%s\t%s\t/^%s$/\n", tag->id, tag->file, pattern);
}

static void writex(const void *t, VISIT v, int level)
{
	(void)level;
	if (v != postorder && v != leaf) {
		return;
	}

	const struct tag *tag = *(const struct tag **)t;
	printf("%s %d %s %s\n", tag->id, tag->line, tag->file, tag->text);
}

int main(int argc, char *argv[])
{
	setlocale(LC_ALL, "");

	char *tagsfile = "tags";
	int append = 0;
	int c;

	while ((c = getopt(argc, argv, "af:x")) != -1) {
		switch (c) {
		case 'a':	/** append to /tagsfile/ **/
			append = 1;
			break;

		case 'f':	/** write to /tagsfile/ instead of "tags" **/
			tagsfile = optarg;
			break;

		case 'x':	/** write to stdout **/
			tagsfile = NULL;
			break;

		default:
			return 1;
		}
	}

	if (optind >= argc) {
		fprintf(stderr, "ctags: At least one file must be specified\n");
		return 1;
	}

	if (tagsfile) {
		output = fopen(tagsfile, append ? "a" : "w");
		if (output == NULL) {
			fprintf(stderr, "ctags: Couldn't open %s: %s\n", tagsfile, strerror(errno));
			return 1;
		}
		setlocale(LC_COLLATE, "POSIX");
	}

	int r = 0;
	while (optind < argc) {
		r |= addtags(argv[optind++]);
	}

	twalk(tagroot, tagsfile ? writefile : writex);

	return r;
}
