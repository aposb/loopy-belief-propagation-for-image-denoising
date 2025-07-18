// Loopy Belief Propagation for Image Denoising
//

#include <iostream>
#include <vector>
#include <chrono>
#include <opencv2/opencv.hpp>

using namespace std;
using namespace cv;

typedef vector<int> IntRow;
typedef vector<IntRow> IntTable;
typedef vector<int> Msg;
typedef vector<Msg> MsgRow;
typedef vector<MsgRow> MsgTable;

Mat inputImage, outputImage;
IntTable smoothnessCost;
MsgTable dataCost, msgUp, msgDown, msgRight, msgLeft;
int width, height, lambda, iterations, levels;

void sendMessageUp(int x, int y);
void sendMessageDown(int x, int y);
void sendMessageRight(int x, int y);
void sendMessageLeft(int x, int y);
void createMessage(Msg &msgData, Msg &msgIn1, Msg &msgIn2, Msg &msgIn3, Msg &msgOut);
int computeDataCost(int x, int y, int label);
int computeSmoothnessCost(int label1, int label2);
int findBestAssignment(int x, int y);
int computeBelief(int x, int y, int label);
int computeEnergy();

int main()
{
	lambda = 1;
	iterations = 2;
	levels = 256; // for grayscale image

	// Start timer
	auto start = chrono::steady_clock::now();

	// Read noisy image
	inputImage = imread("input.png", IMREAD_GRAYSCALE);

	// Get image size
	width = inputImage.cols;
	height = inputImage.rows;

	// Cache data cost
	dataCost = MsgTable(height, MsgRow(width, Msg(levels)));
	for (int y = 0; y < height; y++)
		for (int x = 0; x < width; x++)
			for (int i = 0; i < levels; i++)
				dataCost[y][x][i] = computeDataCost(x, y, i);

	// Cache smoothness cost
	smoothnessCost = IntTable(levels, IntRow(levels));
	for (int i = 0; i < levels; i++)
		for (int j = 0; j < levels; j++)
			smoothnessCost[i][j] = computeSmoothnessCost(i, j);

	// Initialize denoised image
	outputImage = Mat::zeros(height, width, CV_8U);
	
	// Initialize messages
	msgUp = MsgTable(height, MsgRow(width, Msg(levels, 0)));
	msgDown = MsgTable(height, MsgRow(width, Msg(levels, 0)));
	msgRight = MsgTable(height, MsgRow(width, Msg(levels, 0)));
	msgLeft = MsgTable(height, MsgRow(width, Msg(levels, 0)));

	cout << "Iter.\tEnergy" << endl;
	cout << "--------------" << endl;

	// Start iterations
	for (int iter = 0; iter <= iterations; iter++)
	{
		// Update messages
		if (iter >= 1)
		{
			for (int y = 0; y < height; y++)
				for (int x = 0; x < width - 1; x++)
					sendMessageRight(x, y);
			for (int y = 0; y < height; y++)
				for (int x = width - 1; x >= 1; x--)
					sendMessageLeft(x, y);
			for (int x = 0; x < width; x++)
				for (int y = 0; y < height - 1; y++)
					sendMessageDown(x, y);
			for (int x = 0; x < width; x++)
				for (int y = height - 1; y >= 1; y--)
					sendMessageUp(x, y);
		}

		// Update denoised image
		for (int y = 0; y < height; y++)
			for (int x = 0; x < width; x++)
			{
				int label = findBestAssignment(x, y);
				outputImage.at<uchar>(y, x) = label;
			}

		// Show energy
		int energy = computeEnergy();
		cout << iter << "\t" << energy << endl;

		// Show denoised image
		namedWindow("Denoised Image", WINDOW_NORMAL);
		imshow("Denoised Image", outputImage);
		waitKey(1);
	}

	// Save denoised image
	bool flag = imwrite("output.png", outputImage);

	// Stop timer
	auto end = chrono::steady_clock::now();
	auto diff = end - start;
	cout << "\nRunning Time: " << chrono::duration<double, milli>(diff).count() << " ms" << endl;

	waitKey(0);

	return 0;
}

void sendMessageUp(int x, int y)
{
	Msg &msgOut = msgDown[y - 1][x];
	createMessage(dataCost[y][x], msgDown[y][x], msgRight[y][x], msgLeft[y][x], msgOut);
}

void sendMessageDown(int x, int y)
{
	Msg &msgOut = msgUp[y + 1][x];
	createMessage(dataCost[y][x], msgUp[y][x], msgRight[y][x], msgLeft[y][x], msgOut);
}

void sendMessageRight(int x, int y)
{
	Msg &msgOut = msgLeft[y][x + 1];
	createMessage(dataCost[y][x], msgUp[y][x], msgDown[y][x], msgLeft[y][x], msgOut);
}

void sendMessageLeft(int x, int y)
{
	Msg &msgOut = msgRight[y][x - 1];
	createMessage(dataCost[y][x], msgUp[y][x], msgDown[y][x], msgRight[y][x], msgOut);
}

void createMessage(Msg &msgData, Msg &msgIn1, Msg &msgIn2, Msg &msgIn3, Msg &msgOut)
{
	// Create message
	for (int i = 0; i < levels; i++)
	{
		int min = INT_MAX;
		for (int j = 0; j < levels; j++)
		{
			int cost = msgData[j] + smoothnessCost[i][j]
				+ msgIn1[j] + msgIn2[j] + msgIn3[j];
			if (cost < min)
				min = cost;
		}
		msgOut[i] = min;
	}

	// Normalize message
	int min = INT_MAX;
	for (int i = 0; i < levels; i++)
		if (msgOut[i] < min)
			min = msgOut[i];
	for (int i = 0; i < levels; i++)
		msgOut[i] -= min;
}

int computeDataCost(int x, int y, int label)
{
	int pixel = inputImage.at<uchar>(y, x);
	int cost = abs(pixel - label);

	return cost;
}

int computeSmoothnessCost(int label1, int label2)
{
	int cost = lambda * abs(label1 - label2);

	return cost;
}

int findBestAssignment(int x, int y)
{
	int label, min = INT_MAX;
	for (int i = 0; i < levels; i++)
	{
		int cost = computeBelief(x, y, i);
		if (cost < min)
		{
			label = i;
			min = cost;
		}
	}

	return label;
}

int computeBelief(int x, int y, int label)
{
	int cost = dataCost[y][x][label] + msgUp[y][x][label] + msgDown[y][x][label] + msgRight[y][x][label] + msgLeft[y][x][label];

	return cost;
}

int computeEnergy()
{
	int energy = 0;
	for (int y = 0; y < height; y++)
		for (int x = 0; x < width; x++)
		{
			int label1 = outputImage.at<uchar>(y, x);
			energy += dataCost[y][x][label1];
			if (x < width - 1)
			{
				int label2 = outputImage.at<uchar>(y, x + 1);
				energy += smoothnessCost[label1][label2];
			}
			if (y < height - 1)
			{
				int label3 = outputImage.at<uchar>(y + 1, x);
				energy += smoothnessCost[label1][label3];
			}
		}

	return energy;
}
