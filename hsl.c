
double hue_to_rgb(double p, double q, double t)
{
	if (t < 0)
		t += 1;
	if (t > 1)
		t -= 1;
	if (t < 1.0 / 6.0) {
		return p + (q - p) * 6 * t;
	} else if (t < 1.0 / 2.0) {
		return q;
	} else if (t < 2.0 / 3.0) {
		return p + (q - p) * (2.0 / 3.0 - t) * 6;
	} else {
		return p;
	}
}

uint64_t hsl_to_rgb(double h, double s, double l)
{
	double r, g, b;
	if (s == 0) {
		r = l;
		g = l;
		b = l;
	} else {
		double q, p;
		if (l < 0.5) {
			q = l * (1 + s);
		} else {
			q = l + s - l * s;
		}
		p = 2 * l - q;
		r = hue_to_rgb(p, q, h + 1.0 / 3.0);
		g = hue_to_rgb(p, q, h);
		b = hue_to_rgb(p, q, h - 1.0 / 3.0);
	}
	unsigned char ret[4];
	ret[0] = r * 255;
	ret[1] = g * 255;
	ret[2] = b * 255;
	ret[3] = 255;
	return *(uint64_t *)ret;
}
