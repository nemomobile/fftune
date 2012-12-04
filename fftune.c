/*
 * Linux force feedback effect creation and testing tool
 * Forked from fftest tool.
 * Copyright 2001-2002 Johann Deneux <deneux@ifrance.com>
 * Copyright (C) 2012 Jolla Ltd.
 * Contact: Kalle Jokiniemi <kalle.jokiniemi@jollamobile.com>
 */

/*
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA.
 *
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/input.h>

#define BITS_PER_LONG (sizeof(long) * 8)
#define OFF(x)  ((x)%BITS_PER_LONG)
#define BIT(x)  (1UL<<OFF(x))
#define LONG(x) ((x)/BITS_PER_LONG)
#define test_bit(bit, array)    ((array[LONG(bit)] >> OFF(bit)) & 1)

#define N_EFFECTS 32
#define R_QUIT	1
#define R_CONT	0

#define UINT16_MAX	0xffff
#define INT16_MAX	32767
#define INT16_MIN	(-32768)

static struct ff_tune {
	struct ff_effect 	effects[N_EFFECTS];
	int			count;
	int			fd;
} fftune;

char* effect_names[] = {
	"FF_RUMBLE",
	"FF_PERIODIC",
	"FF_CONSTANT",
	"FF_SPRING",
	"FF_FRICTION",
	"FF_DAMPER",
	"FF_INERTIA",
	"FF_RAMP"
};



static const char *main_help = {
	"\n  help       \tPrint available commands\n"
	"  list        \tList available effects\n"
	"  run <id>    \tRun effect number <id>\n"
	"  stop <id>   \tStop effect number <id>\n"
	"  add <params>\tAdd effect with given parameters\n\n"
	"              \tadd r: <ms> <strong_magn [0:0xffff]> <weak_magn [0:0xffff]>\n"
	"              \tadd p: <sine|triangle|square> <ms> <period_ms> <magnitude [0:0x8000]>\\\n"
	"              \t       <offset [0:0x8000]> <phase> <attack_length> <attack_level>\\\n"
	"              \t       <fade_length> <fade_level>\n\n"
	"              \tExample, maximum force rumble effect:\n"
	"              \tadd r: 500 0xffff 0xffff\n\n"
	"              \tExample, sine effect with attack and fade envelope\n"
	"              \tadd p: sine 4000 500 0x7fff 0 0 1000 0 1000 0\n\n"
	"  remove <id> \tRemove and unload effect number <id>\n"
	"  quit        \tQuit the program\n"
};

enum cmd {
	HELP,
	LIST,
	RUN,
	STOP,
	ADD,
	REMOVE,
	QUIT,
	MAX_COMMANDS,
};

char *commands[] = {
	"help",
	"list",
	"run",
	"stop",
	"add",
	"remove",
	"quit"
};

static void print_cmd_help(void)
{
	printf("%s",main_help);
}

static void print_add_help(void)
{
	printf("  Invalid parameters for add.\n  usage:\n  "
		"\"add r: <ms> <strong_magn> <weak_magn>\" OR\n"
		"  \"add p: <sine|triangle|square> <ms> <period"
		"_ms> <magnitude [0:0x8000]> <offset [0:0x8000]>"
		" <phase> <attack_length> <attack_level> "
		"<fade_length> <fade_level>\"\n");
}

static int decode_cmd(char *cmd_str)
{
	int i;
	for (i = 0; i < MAX_COMMANDS; i++)
		if (!strcmp(commands[i], cmd_str))
			return i;
	return HELP;
}

static int remove_effect(int id)
{
	if (id < 0 && id < fftune.count)
		return -1;
	if (ioctl(fftune.fd, EVIOCRMFF, fftune.effects[id].id) < 0) {
		perror("Ioctl remove effect");
		return -1;
	}
	fftune.count--;

	return 0;
}

static void list_effects(void)
{
	int i, type;

	printf("fftune effects:\n");
	for(i = 0; i < fftune.count; i++) {
		printf("  Effect [%d]: ", i);
		type = fftune.effects[i].type - FF_RUMBLE;
		printf("%s | ", effect_names[type]);
		printf("%dms\n", fftune.effects[i].replay.length);
	}
}

static void play_effect(int id, int play)
{
	struct input_event event;

	if (id >= fftune.count || id < 0) {
		printf("No such effect %d\n", id);
		return;
	}

	event.type = EV_FF;
	event.code = fftune.effects[id].id;
	event.value = play;

	if (write(fftune.fd, (const void*) &event, sizeof(event)) == -1) {
		if (play)
			perror("Playback start failed\n");
		else
			perror("Stopping playback failed\n");
		return;
	}
	if (play)
		printf("Playing effect %d\n", id);
	else
		printf("Stopping effect %d\n", id);
}

static int32_t parse_int(char *token, int32_t max, int32_t min)
{
	char *endptr;
	int32_t value = 0;

	errno = 0;
	if (token == NULL)
		return 0;

	value = strtol(token, &endptr, 0);
	if (errno) {
		perror("strtol");
		return 0;
	}
	if (value > max) {
		printf("%s too high, changing to maximum value %d\n",
				token, max);
		value = max;
	} else if (value < min) {
		printf("%s too small, changing to min value %d\n",
				token, max);
		value = min;
	}
	return value;
}

static int add_rumble(struct ff_effect *effect)
{
	char *token;

	memset(effect, 0, sizeof(struct ff_effect));
	effect->id = -1;
	effect->type = FF_RUMBLE;
	token = strtok(NULL, " ");
	if (token == NULL)
		return -1;
	effect->replay.length = parse_int(token, 0x7fffffff, 0);

	token = strtok(NULL, " ");
	if (token == NULL)
		return -1;
//	printf("rumble token1: %s\n", token);
	effect->u.rumble.strong_magnitude = parse_int(token, 0xffff, 0);

	token = strtok(NULL, " ");
	if (token == NULL)
		return -1;
//	printf("rumble token2: %s\n", token);
	effect->u.rumble.weak_magnitude = parse_int(token, 0xffff, 0);

	printf("Adding rumble effect with type = 0x%x id = %d, length = %dms, "
		"strong = 0x%x, weak = 0x%x\n", effect->type, effect->id,
		effect->replay.length, effect->u.rumble.strong_magnitude,
		effect->u.rumble.weak_magnitude);
	return 0;
}

static int add_periodic(struct ff_effect *effect)
{
	char *token;
	struct ff_envelope *env;

	memset(effect, 0, sizeof(struct ff_effect));
	effect->id = -1;
	effect->type = FF_PERIODIC;
	token = strtok(NULL, " ");
	if (token == NULL)
		return -1;

	if (!strcmp(token, "sine")) {
		effect->u.periodic.waveform = FF_SINE;
	} else if (!strcmp(token, "triangle")) {
		effect->u.periodic.waveform = FF_TRIANGLE;
	} else if (!strcmp(token, "square")){
		effect->u.periodic.waveform = FF_SQUARE;
	} else {
		printf("Incorrect waveform, correct options are:\n"
			"\"sine\", \"triangle\", and \"square\"\n");
		return -1;
	}

	token = strtok(NULL, " ");
	if (token == NULL)
		return -1;
	effect->replay.length = parse_int(token, 0x7fff, 0);

	token = strtok(NULL, " ");
	effect->u.periodic.period = parse_int(token, UINT16_MAX, 0);
	token = strtok(NULL, " ");
	effect->u.periodic.magnitude = parse_int(token, INT16_MAX, INT16_MIN);
	token = strtok(NULL, " ");
	effect->u.periodic.offset = parse_int(token, INT16_MAX, INT16_MIN);
	token = strtok(NULL, " ");
	effect->u.periodic.phase = parse_int(token, INT16_MAX, INT16_MIN);

	env = &effect->u.periodic.envelope;

	token = strtok(NULL, " ");
	env->attack_length = parse_int(token, UINT16_MAX, 0);
	token = strtok(NULL, " ");
	env->attack_level = parse_int(token, UINT16_MAX, 0);
	token = strtok(NULL, " ");
	env->fade_length = parse_int(token, UINT16_MAX, 0);
	token = strtok(NULL, " ");
	env->fade_level = parse_int(token, UINT16_MAX, 0);
	return 0;
}

static int process_cmd(char *input)
{
	char *token;
	int cmd;
	struct ff_effect *effect;

	if (input == NULL)
		return R_CONT;

	token = strtok(input, " \n");
	if (!token)
		return R_CONT;

	cmd = decode_cmd(token);

	switch (cmd) {
	case LIST:
		list_effects();
		break;
	case RUN:
		token = strtok(NULL, " ");
		if (token == NULL) {
			printf("No such effect\n");
			break;
		}
		play_effect(atoi(token), 1);
		break;
	case STOP:
		token = strtok(NULL, " ");
		if (token == NULL) {
			printf("No such effect\n");
			break;
		}
		play_effect(atoi(token), 0);
		break;
	case ADD:
		if (!(token = strtok(NULL, " "))) {
			print_add_help();
			break;
		}

		if (!strcmp(token, "r:")) {
			effect = &fftune.effects[fftune.count];
			if (add_rumble(effect)) {
				print_add_help();
				break;
			}
		} else if (!strcmp(token, "p:")){
			effect = &fftune.effects[fftune.count];
			if (add_periodic(effect)) {
				print_add_help();
				break;
			}
		} else {
			print_add_help();
			break;
		}
		printf("Added effect:\n"
			"type = 0x%x\n"
			"id = %d\n"
			"length = %dms\n"
			"strong rumble magnitude = 0x%x\n"
			"weak rumble magnitude = 0x%x\n"
			"period = %dms\n"
			"periodic magnitude = 0x%x\n"
			"offset = %d\n"
			"phase = %d\n"
			"att = %ums\n"
			"att_lev = 0x%x\n"
			"fade = %ums\n"
			"fade_lev = 0x%x\n",
			effect->type,
			effect->id,
			effect->replay.length,
			effect->u.rumble.strong_magnitude,
			effect->u.rumble.weak_magnitude,
			effect->u.periodic.period,
			effect->u.periodic.magnitude,
			effect->u.periodic.offset,
			effect->u.periodic.phase,
			effect->u.periodic.envelope.attack_length,
			effect->u.periodic.envelope.attack_level,
			effect->u.periodic.envelope.fade_length,
			effect->u.periodic.envelope.fade_level);
		if (ioctl(fftune.fd, EVIOCSFF, effect) < 0) {
			perror("Upload effect");
			break;
		}
		fftune.count++;
		break;
	case REMOVE:
		token = strtok(NULL, " ");
		remove_effect(atoi(token));
		break;
	case QUIT:
		return R_QUIT;
	case HELP:
		print_cmd_help();
		break;
	default:
		break;
	}
	return R_CONT;
}

static int test_interfaces(void)
{
	int result, i = 0;
	int fp = 1;
	char device_file_name[64];
	unsigned long features[4];

	printf("Testing for FF interface that supports FF_RUMBLE and ");
	printf("FF_PERIODIC\n");
	/* fail safe stop at 256 devices */
	while (fp && i < 256) {
		sprintf(device_file_name, "/dev/input/event%d", i);
		printf("Opening %s\n", device_file_name);
		fp = open(device_file_name, O_RDWR);
		if (fp == -1) {
			perror("test file open");
			goto no_device;
		}
		/* Query device */
		if (ioctl(fp, EVIOCGBIT(EV_FF, sizeof(unsigned long) * 4),
								features) < 0) {
			perror("Ioctl query failed");
			goto next_iteration;
		}
		result = test_bit(FF_RUMBLE, features);
		result = result && test_bit(FF_PERIODIC, features);
		if (result) {
			printf("Device %s supports FF_RUMBLE and FF_PERIODIC\n",
							device_file_name);
			return i;
		}
next_iteration:
		i++;
	}
no_device:
	printf("No support found for FF_PERIODIC and FF_RUMBLE effects in ");
	printf("any device\n");
	return -1;
}

int main(int argc, char** argv)
{
	struct input_event stop;
	char device_file_name[64];
	char line[81];
	char *linep;
	unsigned long features[4];
	int n_effects;	/* Number of effects the device can play at the same time */
	int i;
	int user_dev_node = 0;

	printf("\nForce feedback effect tuning program\n\n");

	for (i=1; i<argc; ++i) {
		if (strncmp(argv[i], "--help", 64) == 0) {
			printf("Usage:\n");
			printf("  %s /dev/input/eventXX   Open fftune with given driver\n", argv[0]);
			printf("  %s -t                   Test for FF_RUMBLE and FF_PERIODIC\n", argv[0]);
			exit(1);
		} else if (strncmp(argv[i], "-t", 64) == 0) {
			if (test_interfaces() >= 0)
				exit(0);
			else
				exit(1);
		}
		else {
			strncpy(device_file_name, argv[i], 64);
			user_dev_node = 1;
		}
	}

	if (!user_dev_node)
		sprintf(device_file_name, "/dev/input/event%d",
						test_interfaces());

	/* Open device */
	fftune.fd = open(device_file_name, O_RDWR);
	if (fftune.fd == -1) {
		perror("Open device file");
		exit(1);
	}
	printf("Device %s opened\n", device_file_name);

	/* Query device */
	if (ioctl(fftune.fd, EVIOCGBIT(EV_FF, sizeof(unsigned long) * 4),
								features) < 0) {
		perror("Ioctl query");
		exit(1);
	}

	printf("Effects supported by %s:\n", device_file_name);

	if (test_bit(FF_CONSTANT, features)) printf("FF_CONSTANT | ");
	if (test_bit(FF_PERIODIC, features)) printf("FF_PERIODIC | ");
	if (test_bit(FF_SPRING, features)) printf("FF_SPRING | ");
	if (test_bit(FF_FRICTION, features)) printf("FF_FRICTION | ");
	if (test_bit(FF_RUMBLE, features)) printf("FF_RUMBLE | ");
	if (test_bit(FF_CUSTOM, features)) printf("FF_CUSTOM | ");

	printf("\n\nNumber of simultaneous effects: ");

	if (ioctl(fftune.fd, EVIOCGEFFECTS, &n_effects) < 0) {
		perror("Ioctl number of effects");
	}

	printf("%d\n\n", n_effects);

	printf("Enter command, type \"help\" for available commands\n");
	/* Ask user what effects to play */
	while (1) {
		printf(">> ");
		linep = fgets(line, 80, stdin);
		if (linep == NULL)
			continue;

		if (process_cmd(linep))
			exit(0);
	}

	/* Stop the effects */
	for (i = 0; i < N_EFFECTS; ++i) {
		stop.type = EV_FF;
		stop.code =  fftune.effects[i].id;
		stop.value = 0;

		if (write(fftune.fd, (const void*) &stop, sizeof(stop)) == -1) {
			perror("Stop effect");
			exit(1);
		}
	}

	exit(0);
}
