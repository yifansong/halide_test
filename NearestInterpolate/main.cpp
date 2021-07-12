#include "Halide.h"
#include <iostream>
#include <iomanip>
void my_nearest_interpolation(float* src, int src_width, int src_height, float* sample_xy, int sample_height, int sample_width, float* dst) {

	Halide::Buffer<float> image(src, { src_width, src_height }, "Image");

	Halide::Var x("x"), y("y");
	Halide::Func interpolate("interpolate");
	interpolate(x, y) = image(x, y);

	int dst_index = 0;
	for (int index = 0; index < sample_height; index++)
	{
		float samplex = sample_xy[index * sample_width];
		float sampley = sample_xy[(index * sample_width)+ 1];
		if ((samplex < 0) || (samplex >= image.width()) || (sampley < 0) || (sampley >= image.height())) {
			dst[dst_index] = 0.f;
		}
		else {
			Halide::Buffer<float> sample(1, 1);
			const int closest_x = static_cast<int>(std::floor(samplex));
			const int closest_y = static_cast<int>(std::floor(sampley));

			sample.set_min(closest_x, closest_y);

			interpolate.realize(sample);
			dst[dst_index] = sample(closest_x, closest_y);

		}
		dst_index++;
	}
}

void my_bilinear_interpolation(std::vector<float>& src, int src_width, int src_height, std::vector<float>& dst) {

	Halide::Buffer<float> image(src.data(), { src_width, src_height }, "Image");
	const int maxx = src_width - 1;
	const int maxy = src_height - 1;

	Halide::Var x("x"), y("y");
	Halide::Func interpolate("interpolate");
	Halide::Expr factor1 = (image(0, maxy) * (maxx - x) * (y));
	Halide::Expr factor2 = (image(maxx, maxy) * (x) * (y));
	Halide::Expr factor3 = (image(0, 0) * (maxx - x) * (maxy - y));
	Halide::Expr factor4 = (image(maxx, 0) * (x) * (maxy - y));
	Halide::Expr c = (1.f / (maxx * maxy));
	interpolate(x, y) = c * (factor1 + factor2 + factor3 + factor4);

	Halide::Var x_outer, x_inner;
	interpolate.split(x, x_outer, x_inner, 5);
	interpolate.vectorize(x_inner);

	Halide::Buffer<float> sample(src_width, src_height);

	interpolate.realize(sample);
	for (int y = 0; y < src_height; y++) {
		for (int x = 0; x < src_width; x++) {
			dst.push_back(sample(x, y));
		}
	}
}

void test_bilinear_interpolation() {
	std::vector<float> src = {
		1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.5f,
		0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
		0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
		0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
		0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
		0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
		0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
		0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
		0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
		0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 1.f
	};


	std::vector<float> res;
	res.reserve(10 * 10);

	my_bilinear_interpolation(src, 10, 10, res);

	std::cout << std::fixed << std::setw(11) << std::setprecision(6);
	std::cout << "image\n";
	for (int i = 0; i < 10; i++) {
		for (int j = 0; j < 10; j++) {
			std::cout << res[i * 10 + j] << " ";
		}
		std::cout << "\n";
	}
}

void test_nearest_interpolation() {
	std::vector<float> src = {
		1.f, 2.f, 3.f,
		4.f, 5.f, 6.f
	};
	int src_width = 3;
	int src_height = 2;

	std::vector<float> sample_xy = {
		// 1) sample at exact center of pixel (x, y) = (1, 0)
		1.5f, 0.5f,

		// 2) near corner of pixel (x, y) = (1, 0)
		1.01f, 0.01f,

		// 3) at the exact intersection of 4 pixels
		1.f, 1.f,

		// 4) out of bounds
		-0.1f, 0.5f,

		// 5) out of bounds
		src_width + 0.1f, 0.5f,

		// 6) out of bounds
		0.5f,-0.1f,

		// 7) out of bounds
		0.5f, src_height + 0.1f,

		// 8) regular point
		2.6f, 1.4f,

		// 9) regular point
		1.3f, 1.75f,

		// 10) regular point
		0.25f, 1.75f
	};
	int sample_width = 2;
	int sample_height = 10;

	std::vector<float> dst;
	dst.resize(sample_height);

	my_nearest_interpolation(src.data(), src_width, src_height, sample_xy.data(), sample_height, sample_width, dst.data());

	// 1)
	assert(dst[0] == 2.f);

	// 2)
	assert(dst[1] == 2.f);

	// 3) 
	// it is hard to say what the correct answer should be here
	// will accept any of the surrounding 4 pixels
	assert(dst[2] == 1.f || dst[2] == 2.f || dst[2] == 4.f || dst[2] == 5.f);

	// 4) ... 7)
	assert(dst[3] == 0);
	assert(dst[4] == 0);
	assert(dst[5] == 0);
	assert(dst[6] == 0);

	// 8)
	assert(dst[7] == 6.f);

	// 9)
	assert(dst[8] == 5.f);

	// 10)
	assert(dst[9] == 4.f);
}


int main() 
{
	test_nearest_interpolation();
	test_bilinear_interpolation();

	return 0;
}