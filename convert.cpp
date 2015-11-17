#include <stdio.h>
#include <opencv2/opencv.hpp>
#include <omp.h>
#include "timer.h"


using namespace std;
using namespace cv;



static void RGB2HSV(unsigned char r, unsigned char g, unsigned char b, unsigned char &h, unsigned char &s, unsigned char &v)
{
    unsigned char minRGB, maxRGB;

    minRGB = r;
    maxRGB = r;
    //minRGB = r < g ? (r < b ? r : b) : (g < b ? g : b);
    //maxRGB = r > g ? (r > b ? r : b) : (g > b ? g : b);
    (minRGB > g) && (minRGB = g); //these are not conditional statements.
    (minRGB > b) && (minRGB = b); //these are just boolean expressions.
    (maxRGB < g) && (maxRGB = g); //these are not conditional statements.
    (maxRGB < b) && (maxRGB = b); //these are just boolean expressions.

    v = maxRGB;
    if (v == 0)
    {
        h = 0;
        s = 0;
        return;
    }

    s = 255 * (maxRGB - minRGB) / v;
    if (s == 0)
    {
        h = 0;
        return;
    }

    if (r == maxRGB)
        h = 0 + 43 * (g - b) / (maxRGB - minRGB);
    else if (g == maxRGB)
        h = 85 + 43 * (b - r) / (maxRGB - minRGB);
    else
        h = 171 + 43 * (r - g) / (maxRGB - minRGB);

    return;
}


static void RGB2HSVf(float r, float g, float b, float &h, float &s, float &v)
{
    float minRGB, maxRGB;

    minRGB = r;
    maxRGB = r;
    //minRGB = r < g ? (r < b ? r : b) : (g < b ? g : b);
    //maxRGB = r > g ? (r > b ? r : b) : (g > b ? g : b);
    (minRGB > g) && (minRGB = g); //these are not conditional statements.
    (minRGB > b) && (minRGB = b); //these are just boolean expressions.
    (maxRGB < g) && (maxRGB = g); //these are not conditional statements.
    (maxRGB < b) && (maxRGB = b); //these are just boolean expressions.

    v = maxRGB;
    if (v == 0.f)
    {
        h = 0.f;
        s = 0.f;
        return;
    }

    s = 255.f * (maxRGB - minRGB) / v;
    if (s == 0.f)
    {
        h = 0.f;
        return;
    }

    if (r == maxRGB)
        h = 43.f * (g - b) / (maxRGB - minRGB);
    else if (g == maxRGB)
        h = 128.f * (b - r) / (maxRGB - minRGB);
    else
        h = 214.f * (r - g) / (maxRGB - minRGB);

    return;
}


static void RGB2HSV2(float r, float g, float b,
                    float &h, float &s, float &v)
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
    const char* inputFileName = "soccer.avi";
    const char* outputFileName = "soccer2.avi";

    // starting timer
    timerStart();

    VideoCapture cap(inputFileName);

    int ex = static_cast<int>(cap.get(CV_CAP_PROP_FOURCC));

    printf("CodecID: %i\n", ex);

    VideoWriter out;
    Size size = Size((int) cap.get(CV_CAP_PROP_FRAME_WIDTH),    // Acquire input size
      (int) cap.get(CV_CAP_PROP_FRAME_HEIGHT));

    if(!cap.isOpened())  // check if we succeeded
        return -1;

    int cols = size.width;
    int rows = size.height;

    printf("Video size: %i x %i\n", rows, cols);

    out.open(outputFileName, ex, cap.get(CV_CAP_PROP_FPS), size, true);

    if (!out.isOpened())
    {
        cout  << "Could not open the output video for write: " << endl;
        return -1;
    }

    long count = (long)cap.get(CV_CAP_PROP_FRAME_COUNT);

    printf("Frames %d\n", (int)count);

    Mat edges;
    Mat frame;
    Vec3b color;

    for(int counter = 0; counter < count; counter++)
    //for(;;)
    {
        cap >> frame;

        if (frame.empty()) break;
        frame.copyTo(edges);

        int channels = edges.channels();

        #pragma omp parallel for
        for(int j = 0;j < rows;j++) {
            for(int i = 0;i < cols;i++) {
                int index = channels * (cols * j + i);
                unsigned char b = edges.data[index], g = edges.data[index + 1], r = edges.data[index + 2], h,s,v;
                //float b = edges.data[index], g = edges.data[index + 1], r = edges.data[index + 2], h,s,v;
                //RGB2HSVf(r, g, b, h, s, v);
                RGB2HSV(r, g, b, h, s, v);
                edges.data[index] = h;
                edges.data[index + 1] = s;
                edges.data[index + 2] = v;
            }
        }

        out.write(edges);
    }

    destroyAllWindows();

    // stopping timer
    elapsedTime = timerStop();

    printf("Total duration: %f\n", elapsedTime);

    return 0;
}
