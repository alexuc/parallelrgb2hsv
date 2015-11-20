#include <stdio.h>
#include <opencv2/opencv.hpp>
#include <omp.h>
#include "timer.h"


using namespace std;
using namespace cv;


// Routine to convert a RGB pixel to HSV. This function is based on fixed point data types and short-circuit comparisons
// to improve performance
static void RGB2HSV(unsigned char r, unsigned char g, unsigned char b, unsigned char &h, unsigned char &s, unsigned char &v)
{
    unsigned char minRGB, maxRGB;

    minRGB = r;
    maxRGB = r;
    (minRGB > g) && (minRGB = g); //these are short-circuit comparisons
    (minRGB > b) && (minRGB = b); //these are short-circuit comparisons
    (maxRGB < g) && (maxRGB = g); //these are short-circuit comparisons
    (maxRGB < b) && (maxRGB = b); //these are short-circuit comparisons

    v = maxRGB;
	
	// if value is 0, no need to calculate anything else
    if (v == 0)
    {
        h = 0;
        s = 0;
        return;
    }

	// find the S value, according to native formula
	// we know v is not zero
    s = 255 * (maxRGB - minRGB) / v;
    if (s == 0)
    {
        h = 0;
        return;
    }

	// find the H value, according to native formula
    if (r == maxRGB)
        h = 0 + 43 * (g - b) / (maxRGB - minRGB);
    else if (g == maxRGB)
        h = 85 + 43 * (b - r) / (maxRGB - minRGB);
    else
        h = 171 + 43 * (r - g) / (maxRGB - minRGB);
}

// Routine to convert a RGB pixel to HSV. This function lacks the short-circuit comparison optimization
static void RGB2HSVbasic(unsigned char r, unsigned char g, unsigned char b, unsigned char &h, unsigned char &s, unsigned char &v)
{
    unsigned char minRGB, maxRGB;

    minRGB = r;
    maxRGB = r;
	// comparisons using condition-based statements
    minRGB = r < g ? (r < b ? r : b) : (g < b ? g : b);
    maxRGB = r > g ? (r > b ? r : b) : (g > b ? g : b);

    v = maxRGB;
	
	// if value is 0, no need to calculate anything else
    if (v == 0)
    {
        h = 0;
        s = 0;
        return;
    }

	// find the S value, according to native formula
	// we know v is not zero
    s = 255 * (maxRGB - minRGB) / v;
    if (s == 0)
    {
        h = 0;
        return;
    }

	// find the H value, according to native formula
    if (r == maxRGB)
        h = 0 + 43 * (g - b) / (maxRGB - minRGB);
    else if (g == maxRGB)
        h = 85 + 43 * (b - r) / (maxRGB - minRGB);
    else
        h = 171 + 43 * (r - g) / (maxRGB - minRGB);
}


// Routine to convert a RGB pixel to HSV using float types. This function is the most straight form of implementation
// as it lacks all optimizations, however it keeps the deepest precision by using float types
static void RGB2HSVf(float r, float g, float b, float &h, float &s, float &v)
{
    float minRGB, maxRGB;

    minRGB = r;
    maxRGB = r;
	// comparisons using condition-based statements
    minRGB = r < g ? (r < b ? r : b) : (g < b ? g : b);
    maxRGB = r > g ? (r > b ? r : b) : (g > b ? g : b);

    v = maxRGB;
	
	// if value is 0, no need to calculate anything else
    if (v == 0.f)
    {
        h = 0.f;
        s = 0.f;
        return;
    }

	// find the S value, according to native formula
	// we know v is not zero
    s = 255.f * (maxRGB - minRGB) / v;
    if (s == 0.f)
    {
        h = 0.f;
        return;
    }

	// find the H value, according to native formula
    if (r == maxRGB)
        h = 43.f * (g - b) / (maxRGB - minRGB);
    else if (g == maxRGB)
        h = 128.f * (b - r) / (maxRGB - minRGB);
    else
        h = 214.f * (r - g) / (maxRGB - minRGB);
}


// Routine to convert a RGB pixel to HSV using float types. This function is optimized using floating point spaces
// code taken from: http://lolengine.net/blog/2013/01/13/fast-rgb-to-hsv
static void RGB2HSV2(float r, float g, float b, float &h, float &s, float &v)
{
    float K = 0.f;

    if (g < b)
    {
        std::swap(g, b);
        K = -1.f;
    }

    if (r < g)
    {
        std::swap(r, g);
        K = -2.f / 6.f - K;
    }

    float chroma = r - std::min(g, b);
    h = fabs(K + (g - b) / (6.f * chroma + 1e-20f));
    s = chroma / (r + 1e-20f);
    v = r;
}



int main(int argc, char** argv )
{
    double elapsedTime;
	// declare input and output file names, according to user input
    const char* inputFileName = argv[0];
    const char* outputFileName = argv[1];

    // starting timer
    timerStart();

	// declare OpenCV's video capture
    VideoCapture cap(inputFileName);

	// detect codec ID from video file
    int ex = static_cast<int>(cap.get(CV_CAP_PROP_FOURCC));

    printf("CodecID: %i\n", ex);

	// declare OpenCV's video writer object
    VideoWriter out;
	
	// Acquire input size from video metadata
    Size size = Size((int) cap.get(CV_CAP_PROP_FRAME_WIDTH), (int) cap.get(CV_CAP_PROP_FRAME_HEIGHT));

	// check if video was successfully opened
    if(!cap.isOpened())
        return -1;

    int cols = size.width;
    int rows = size.height;

    printf("Size %i - %i\n", rows, cols);

	// open the video writer onto the output file, using the codec ID identified
    out.open(outputFileName, ex, cap.get(CV_CAP_PROP_FPS), size, true);

	// check if output file was opened
    if (!out.isOpened())
    {
        printf("Could not open the output video for write.\n");
        return -1;
    }

	// read number of frames in video file
    long count = (long)cap.get(CV_CAP_PROP_FRAME_COUNT);

    printf("Frames %d\n", (int)count);

	// declare matrix source and target
    Mat targetFrame;
    Mat sourceFrame;

	// main loop to go through each of the frames in the video
    for(int counter = 0; counter < count; counter++)
    {
		// acquire frame from input video capture
        cap >> sourceFrame;

		// prevent code from reading an empty frame
        if (sourceFrame.empty()) break;
		
		// create a copy of the frame just read
        sourceFrame.copyTo(targetFrame);

		// obtain the number of channels, this is only needed to access the right channel
		// when fetching the frame, as the file could have other channels than audio and video
		// this is only needed to make the code agnostic to multi-channel videos
        int channels = targetFrame.channels();

		// loop to go through each pixel in the frame. parallel area initiates
		// 2nd level loop, access matrix rows
        #pragma omp parallel for
        for(int j = 0;j < rows;j++) {
			// 3rd level loop, access matrix columns
            for(int i = 0;i < cols;i++) {
				// calculate the index of the pixel in the frame
                int index = channels * (cols * j + i);
				
				// obtain RGB values from video
                unsigned char b = targetFrame.data[index], g = targetFrame.data[index + 1], r = targetFrame.data[index + 2]
				// declare target HSV variables
				unsigned char h,s,v;
				
				// code commented, needed only if we want the program use floating space mode
                //float b = targetFrame.data[index], g = targetFrame.data[index + 1], r = targetFrame.data[index + 2], h,s,v;
                //RGB2HSVf(r, g, b, h, s, v);
				
				// perform the actual conversion of the pixel
                RGB2HSV(r, g, b, h, s, v);
				
				// store converted pixel into target frame
                targetFrame.data[index] = h;
                targetFrame.data[index + 1] = s;
                targetFrame.data[index + 2] = v;
            }
        }

		// write the frame already converted to HSV into the output file
        out.write(targetFrame);
    }

	// close all captures opened in OpenCV
    destroyAllWindows();

    // stopping timer
    elapsedTime = timerStop();

    printf("Total duration: %f\n", elapsedTime);

    return 0;
}
