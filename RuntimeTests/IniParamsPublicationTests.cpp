#include "../DirectX11/IniParamsPublication.h"

#include <array>
#include <iostream>
#include <stdexcept>

namespace
{
using Row = std::array<float, 4>;

void require(bool condition, const char *message)
{
	if (!condition)
		throw std::runtime_error(message);
}

void failed_upload_retries_the_latest_cpu_values()
{
	IniParamsPublication publication;
	Row gpu{1, 2, 3, 4};
	Row cpu{5, 6, 7, 8};
	bool available = false;
	auto upload = [&]()
	{
		if (!available)
			return false;
		gpu = cpu;
		return true;
	};
	require(!publication.Publish(true, upload), "failed upload was acknowledged");
	require(gpu == Row{1, 2, 3, 4}, "failure changed the previous GPU image");
	cpu = Row{9, 10, 11, 12};
	available = true;
	require(publication.Publish(false, upload), "pending upload was not retried");
	require(gpu == Row{9, 10, 11, 12}, "retry lost the newest CPU image");

	// A completed request must not publish unrelated later CPU changes.
	cpu = Row{13, 14, 15, 16};
	require(publication.Publish(false, upload), "clean request failed");
	require(gpu == Row{9, 10, 11, 12}, "clean invocation unexpectedly uploaded");
}

void missing_resources_retain_publication_until_recreated()
{
	IniParamsPublication publication;
	const Row cpu{2, 4, 6, 8};
	require(!publication.Publish(true, []() { return false; }), "missing resource cleared publication");
	require(!publication.Publish(false, []() { return false; }), "repeated failure cleared publication");
	Row recreated{0, 0, 0, 0};
	require(publication.Publish(false,
	                            [&]()
	                            {
		                            recreated = cpu;
		                            return true;
	                            }),
	        "recreated resource was not updated");
	require(recreated == Row{2, 4, 6, 8}, "recreation lost pending contents");
}

void distinct_publication_points_preserve_both_images()
{
	IniParamsPublication publication;
	Row gpu{0, 0, 0, 0};
	Row cpu{1, 2, 3, 4};
	auto upload = [&]()
	{
		gpu = cpu;
		return true;
	};
	require(publication.Publish(true, upload), "first publication failed");
	const Row first_draw = gpu;
	cpu = Row{4, 3, 2, 1};
	require(publication.Publish(true, upload), "second publication failed");
	require(first_draw == Row{1, 2, 3, 4}, "first publication point lost its image");
	require(gpu == Row{4, 3, 2, 1}, "second publication point lost its image");
}
} // namespace

int main()
{
	try
	{
		failed_upload_retries_the_latest_cpu_values();
		missing_resources_retain_publication_until_recreated();
		distinct_publication_points_preserve_both_images();
		std::cout << "PASS context-owned publication policy (three behaviors)\n";
		return 0;
	}
	catch (const std::exception &error)
	{
		std::cerr << "FAIL " << error.what() << '\n';
		return 1;
	}
}
