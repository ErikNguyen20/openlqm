/*
* Copyright 2025 Noblis, Inc.
* https://fingerprint.nist.gov/openlqm
*
* Licensed under the Apache License, Version 2.0 (the "License"); you may not
* use this file except in compliance with the License. You may obtain a copy of
* the License at https://www.apache.org/licenses/LICENSE-2.0.
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
* WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
* License for the specific language governing permissions and limitations under
* the License.
*
* OpenLQM was developed by Noblis, Inc. under contract to the National
* Institute of Standards and Technology in 2024-2025, as a port of "LQMetric,"
* which was developed by Noblis, Inc. under contract to the Federal Bureau of
* Investigation's Criminal Justice Information Services Division in 2012-2014.
*/

#include <openlqm/openlqm.hpp>
#include <iostream>
#include <sstream>
#include <array>
#include <algorithm>
#include <opencv2/opencv.hpp>

const std::string LICENSE_TEXT = \
"Open Latent Quality Metric (OpenLQM), Version " + OpenLQM::VERSION + \
R"(
Copyright 2025, Noblis, Inc.
https://fingerprint.nist.gov/openlqm

Licensed under the Apache License, Version 2.0 (the "License"); you may not
use this file except in compliance with the License. You may obtain a copy of
the License at https://www.apache.org/licenses/LICENSE-2.0.

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
License for the specific language governing permissions and limitations under
the License.

OpenLQM was developed by Noblis, Inc. under contract to the National
Institute of Standards and Technology in 2024-2025, as a port of "LQMetric,"
which was developed by Noblis, Inc. under contract to the Federal Bureau of
Investigation's Criminal Justice Information Services Division in 2012-2014.
)";

const std::string USAGE_TEXT = \
R"(Usage:  openlqm [-v | --verbose] [-h | --headers] [(-p | --ppi) <ppi>] [(-o | --output) <outputImage>] InputFile
        openlqm [--version]

    Analyzes a friction ridge image and returns latent quality metric(s).
    For normal use, call without options with the name of a 500 or 1000ppi
    grayscale friction ridge image file, and it will return latent quality,
    VID and VCMP values in the range 0...100. Errors return values < 0.

    -v: Verbose mode:  Prints detailed quality metrics as a single line of
        tab-separated text
    -h: Prints descriptive headers for the output metrics. The headers and
        metrics are each one line of tab-separated text.
    --version: Prints version and license information.
    -p: If and only if the input image has an invalid, missing, or
	    unsupported ppi value, then the value provided with this flag is
		used as the image's ppi.
		Valid values are 500, 1000, 2000, and 4000.
    -o: Output path for a colorized clarity map image (for example, .png).
        The image will be written at the same dimensions as the input image.
)";

bool SaveColorizedClarityMap(const OpenLQM::Fingerprint& inpImg, const OpenLQM::QualityMap& qualityMap, const std::string& outputPath) {
	if (qualityMap.buffer.empty() || qualityMap.width == 0 || qualityMap.height == 0) {
		return false;
	}

	cv::Mat qualityIndexMap(static_cast<int>(qualityMap.height), static_cast<int>(qualityMap.width), CV_8UC1);
	memcpy(qualityIndexMap.data, qualityMap.buffer.data(), qualityMap.buffer.size());

	cv::Mat colorMap(static_cast<int>(qualityMap.height), static_cast<int>(qualityMap.width), CV_8UC3, cv::Scalar(0, 0, 0));
	for (int y = 0; y < colorMap.rows; ++y) {
		for (int x = 0; x < colorMap.cols; ++x) {
			unsigned char q = qualityIndexMap.at<unsigned char>(y, x);
			cv::Vec3b color(0, 0, 0); // BGR
			switch (q) {
				case 1: color = cv::Vec3b(0, 0, 255); break;     // red
				case 2: color = cv::Vec3b(0, 255, 255); break;   // yellow
				case 3: color = cv::Vec3b(0, 255, 0); break;     // green
				case 4: color = cv::Vec3b(255, 0, 0); break;     // blue
				case 5: color = cv::Vec3b(255, 255, 0); break;   // cyan
				default: break;                                  // black
			}
			colorMap.at<cv::Vec3b>(y, x) = color;
		}
	}

	cv::Mat output;
	cv::resize(
		colorMap,
		output,
		cv::Size(static_cast<int>(inpImg.width), static_cast<int>(inpImg.height)),
		0.0,
		0.0,
		cv::INTER_NEAREST
	);

	return cv::imwrite(outputPath, output);
}

void ProcessAll(const std::string& filePath, bool verbose, bool printHeaders, unsigned int validatedPpi, const std::string& outputPath) {
	OpenLQM::Fingerprint inpImg;
	try {
		inpImg.LoadFromFilePath(filePath, static_cast<OpenLQM::PixelDensity>(validatedPpi));
	} catch (...) {
		std::cerr << "Failed to load file (" + filePath + ")" << std::endl;
		return;
	}

	OpenLQM::Metrics metrics;
	OpenLQM::QualityMap qualityMap;
	OpenLQM::GetAllMetricsAndQualityMapFromFingerprint(inpImg, metrics, qualityMap);

	OpenLQM::Supplement::PrintMetrics(metrics, filePath, verbose, printHeaders);

	if (!outputPath.empty()) {
		if (!SaveColorizedClarityMap(inpImg, qualityMap, outputPath)) {
			std::cerr << "Failed to write output clarity map image (" << outputPath << ")" << std::endl;
		}
	}
}

int main(int argc, char* argV[]) {
	if (argc > 1) {
		std::string filePath;

		bool verbose = false;
		bool printHeaders = false;
		bool printVersion = false;
		std::string outputPath;
		unsigned int ppi = 0;
		std::vector<std::string> args;
		for (int i = 1; i < argc; ++i) {
			std::string arg = argV[i];
			if (arg.empty()) {
				continue;
			}

			if (static_cast<int>(arg.size()) > 2 && arg[0] == '-' && arg[1] != '-') {
				for (int j = 1; j < static_cast<int>(arg.size()); ++j) {
					args.push_back(std::string("-") + arg[static_cast<std::size_t>(j)]);
				}
			} else {
				args.push_back(arg);
			}
		}
		for (int i = 0; i < static_cast<int>(args.size()); ++i) {
			std::string arg = args[static_cast<std::size_t>(i)];
			if (arg == "-v" || arg == "--verbose") {
				verbose = true;
			} else if (arg == "-h" || arg == "--headers") {
				printHeaders = true;
			} else if (arg == "--version") {
				printVersion = true;
			} else if (arg == "-p" || arg == "--ppi") {
				if (i == static_cast<int>(args.size()) - 1) {
					std::cerr << "Argument " << arg << " missing value" << std::endl;
					return -1;
				}

				++i;
				arg = args[static_cast<std::size_t>(i)];

				bool validPpi = false;
				if (std::stringstream(arg) >> ppi) {
					std::array supportedPpis{500u, 1000u, 2000u, 4000u};
					if (std::find(supportedPpis.begin(), supportedPpis.end(), ppi) != supportedPpis.end()) {
						validPpi = true;
					}
				}

				if (!validPpi) {
					std::cerr << "Unsupported ppi value (" << arg << "). Valid values are 500, 1000, 2000, and 4000." << std::endl;
					return -1;
				}
			} else if (arg == "-o" || arg == "--output") {
				if (i == static_cast<int>(args.size()) - 1) {
					std::cerr << "Argument " << arg << " missing value" << std::endl;
					return -1;
				}

				++i;
				outputPath = args[static_cast<std::size_t>(i)];
			} else if (!arg.empty() && arg[0] != '-' && filePath.empty()) {
				filePath = arg;
			} else {
				std::cerr << "Unrecognized/extraneous argument: " << arg << std::endl;
				return -1;
			}
		}

		if (printVersion) {
			std::cout << LICENSE_TEXT;
		} else {
			if (filePath.empty()) {
				std::cout << USAGE_TEXT;
				return 0;
			}
			ProcessAll(filePath, verbose, printHeaders, ppi, outputPath);
		}
	} else {
		std::cout << USAGE_TEXT;
	}

	return 0;
}
