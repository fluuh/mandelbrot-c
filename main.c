#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <string.h>
#include <sys/time.h>
#include "mandel.h"
#include "hsl.c"

#ifndef NUM_THREADS
#define NUM_THREADS 8
#endif

#define MAX_SAMPLES 100
/* more than 8 doesn't seem to improve performance */
#define PARALLEL_INCREMENT 8

/* random number generator */

static uint64_t next;

// uint64_t rand_uint64(void)
// {
// 	next = ((next ^ (next << 13)) ^ (next >> 7)) ^ (next << 17);
// 	return next;
// }

static uint64_t rand_uint64(void)
{
	next = next * 1103515245 + 12345;
	return (uint64_t)(next / 65536) % 32768;
}

static double rand_double()
{
	double val = (double)(rand_uint64());
	val = val / 32768;
	// printf("f: %lf\n", val);
	return val;
}

static double rd_pred[MAX_SAMPLES];

static double mandel_iter(int *n, double px, double py, int iter)
{
	double x, y, xx, yy, xy;
	x = 0;
	y = 0;

	for (int i = 0; i < iter; i++) {
		xx = x * x;
		yy = y * y;
		xy = x * y;
		if (xx + yy > 4) {
			*n = i;
			return xx + yy;
		}
		x = xx - yy + px;
		y = 2 * xy + py;
	}

	*n = iter;
	return xx + yy;
}

uint64_t mandel_paint(double r, int n)
{
	if (r > 4) {
		return hsl_to_rgb((double)n / 800 * r, 1, 0.5);
	}

	return 0xFFFFFFFFFFFFFFF;
}

void mandel_basic(int y, unsigned char *row, const struct mandel_spec *spec)
{
	for (int x = 0; x < spec->width; x++) {
		int r = 0;
		int g = 0;
		int b = 0;
		for (int i = 0; i < spec->samples; i++) {
			double nx = spec->ph * spec->ratio *
			                ((((double)x) + rd_pred[i]) /
			                 ((double)spec->width)) +
			            spec->px;
			double ny = spec->ph * ((((double)y) + rd_pred[i]) /
			                        ((double)spec->height)) +
			            spec->py;
			int n;
			unsigned char c[4];
			double r = mandel_iter(&n, nx, ny, spec->iter);
			*(uint64_t *)c = mandel_paint(r, n);
			r += c[0];
			g += c[1];
			b += c[2];
		}
		unsigned char cr = (double)r / (double)spec->samples;
		unsigned char cg = (double)g / (double)spec->samples;
		unsigned char cb = (double)b / (double)spec->samples;
		row[x * 3 + 0] = cr;
		row[x * 3 + 1] = cg;
		row[x * 3 + 2] = cb;
	}
}

void mandel_single(unsigned char *img, struct mandel_spec *spec)
{
	for (int y = 0; y < spec->height; y++) {
		mandel_basic(y, &img[y * spec->width * 3], spec);
	}
}

/* multi-threading */

struct mandel_shared {
	const struct mandel_spec *spec;
	unsigned char *img;
	/* the next row to generate */
	int row;
	pthread_mutex_t lock; /* row */
};

void mandel_parallel_fetch(struct mandel_shared *shared)
{
	while (1) {
		pthread_mutex_lock(&shared->lock);

		int row = shared->row;
		shared->row += PARALLEL_INCREMENT;

		pthread_mutex_unlock(&shared->lock);

		// if (row >= shared->spec->height) {
		// 	break;
		// }

		// mandel_basic(row, &shared->img[row * shared->spec->width *
		// 3], shared->spec);

		int i = row;
		while (i - row < PARALLEL_INCREMENT &&
		       i <= shared->spec->height) {
			mandel_basic(i,
			             &shared->img[i * shared->spec->width * 3],
			             shared->spec);
			i++;
		}

		if (i < row + PARALLEL_INCREMENT) {
			/* finished */
			break;
		}

#ifdef SHOW_PROGRESS
		fprintf(
		    stdout, "\r%d/%d (%d%%)", i, shared->spec->height,
		    (int)(100 * ((double)i / (double)shared->spec->height)));
		fflush(stdout);
#endif
	}
}

int mandel_parallel(unsigned char *img, struct mandel_spec *spec)
{
	pthread_t threads[NUM_THREADS - 1];
	/* this function won't return before all other threads
	 * have exited, so this is safe. */
	struct mandel_shared shared = {
	    .spec = spec,
	    .img = img,
	    .row = 0,
	};

	if (pthread_mutex_init(&shared.lock, NULL) != 0) {
		fputs("error: could not initialize mutex\n", stderr);
		return -1;
	}

	for (int i = 0; i < NUM_THREADS - 1; i++) {
		int err =
		    pthread_create(&threads[i], NULL,
		                   ((void *)&mandel_parallel_fetch), &shared);
		if (err) {
			fprintf(stderr, "can't create thread: %s\n",
			        strerror(err));
			return -1;
		}
	}

	/* use current thread */
	mandel_parallel_fetch(&shared);

	// while (1) {
	// 	pthread_mutex_lock(&shared.lock);
	// 	if (shared.row >= shared.spec->height) {
	// 		break;
	// 	}
	// 	fprintf(stdout, "\r%d/%d (%d%%)", shared.row,
	// shared.spec->height, (int)(100 * ((double)shared.row /
	// (double)shared.spec->height))); 	fflush(stdout);
	// 	pthread_mutex_unlock(&shared.lock);
	// 	sleep(1);
	// }

	for (int i = 0; i < NUM_THREADS - 1; i++) {
		pthread_join(threads[i], NULL);
	}
	fprintf(stdout, "\r%d/%d (100%%)\n", shared.spec->height,
	        shared.spec->height);

	return 0;
}

int main(int argc, char **argv)
{
	/* seed rand_uint64 */
	next = time(NULL);
	/* TODO: command line options for these */
	struct mandel_spec spec = {
	    .px = -0.5557506,
	    .py = -0.55560,
	    .ph = 0.000000001,
	    // .px = -2,
	    // .py = -1.2,
	    // .ph = 2.5,

	    .width = 512,
	    .height = 512,
	    .ratio = spec.width / spec.height,
	    .iter = 1500,
	    .samples = 50,
	};
	/* initialize *random* numbers for sampling */
	for (int i = 0; i < spec.samples; i++) {
		rd_pred[i] = rand_double();
	}
	int n;
	unsigned char *img = malloc(spec.width * spec.height * 4);
	if (img == NULL) {
		return -1;
	}
	int status = mandel_parallel(img, &spec);
	if (status > 0) {
		return status;
	}
	/* write to file */
	FILE *file = fopen("result.pbm", "w");
	if (file == NULL) {
		return -1;
	}
	fprintf(file, "P6\n%d %d\n%d\n", spec.width, spec.height, 255);
	fwrite(img, spec.width * spec.height, 3, file);
	fclose(file);
	free(img);
	return 0;
}
