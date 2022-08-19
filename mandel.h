#ifndef MANDEL_H_
#define MANDEL_H_

struct mandel_spec {
	double px;
	double py;
	double ph;

	int width;
	int height;
	double ratio;
	int iter;
	int samples;
};

#endif
