#pragma once

#include <Ole2.h>
#include <Shlobj.h>
#include <NuiApi.h>

#include <string>
#include <iostream>
#include <sstream>

#define width 640
#define height 480

// Kinect variables
HANDLE depthStream; // The identifier of the Kinect's RGB Camera
INuiSensor* sensor; // The kinect sensor
					//Stores the coordinates of each joint
Vector4 skeletonPosition[NUI_SKELETON_POSITION_COUNT];
float depthBuf[height][width];

bool initKinect() {
	// Get a working kinect sensor
	int numSensors;
	if (NuiGetSensorCount(&numSensors) < 0 || numSensors < 1) return false;
	if (NuiCreateSensorByIndex(0, &sensor) < 0) return false;

	// Initialize sensor
	sensor->NuiInitialize(NUI_INITIALIZE_FLAG_USES_DEPTH | NUI_INITIALIZE_FLAG_USES_COLOR | NUI_INITIALIZE_FLAG_USES_SKELETON);
	sensor->NuiImageStreamOpen(
		NUI_IMAGE_TYPE_COLOR,            // Depth camera or rgb camera?
		NUI_IMAGE_RESOLUTION_640x480,    // Image resolution
		0,      // Image stream flags, e.g. near mode
		2,      // Number of frames to buffer
		NULL,   // Event handle
		&depthStream);
	return sensor;
}

void getDepthData() {
	NUI_IMAGE_FRAME imageFrame;
	NUI_LOCKED_RECT LockedRect;

	if (sensor->NuiImageStreamGetNextFrame(depthStream, 0, &imageFrame) < 0) return;
	//do something with imageFrame
	INuiFrameTexture* texture = imageFrame.pFrameTexture;
	texture->LockRect(0, &LockedRect, NULL, 0);
	if (LockedRect.Pitch != 0)
	{
		const BYTE* curr = (const BYTE*)LockedRect.pBits;

		for (int i = 0; i < width*height; i++) {
			// Get depth in millimeters
			USHORT depth = NuiDepthPixelToDepth(*curr++);

			depthBuf[i / width][i%width] = float(depth);
			//TODO: make sure this isnt wrong 
		}
	}
	texture->UnlockRect(0);
}

void getSkeletonData() {
	NUI_SKELETON_FRAME skeletonFrame = { 0 };
	if (sensor->NuiSkeletonGetNextFrame(0, &skeletonFrame) >= 0) {
		sensor->NuiTransformSmooth(&skeletonFrame, NULL);
		// Process skeletal frame (see below)...
		// Loop over all sensed skeletons
		for (int z = 0; z < NUI_SKELETON_COUNT; ++z) {
			const NUI_SKELETON_DATA& skeleton = skeletonFrame.SkeletonData[z];
			// Check the state of the skeleton
			if (skeleton.eTrackingState == NUI_SKELETON_TRACKED) {
				// Copy the joint positions into our array
				for (int i = 0; i < NUI_SKELETON_POSITION_COUNT; ++i) {
					skeletonPosition[i] = skeleton.SkeletonPositions[i];
					if (skeleton.eSkeletonPositionTrackingState[i] == NUI_SKELETON_POSITION_NOT_TRACKED) {
						skeletonPosition[i].w = 0;
					}
				}
			}
		}
	}
}

std::string v4tos(Vector4 v4)
{
	std::stringstream stream;
	stream << "[" << v4.x << "," << v4.y << "," << v4.z << "," << v4.w << "]";
	return stream.str();
}

int counter;

float mapRange(float in, float min, float max, float range) {
	return (in - min)*range / (max - min);
}

int xyData(Vector4 input) {
	//hard coded skeleton range for input 
	float minX = -2.2;
	float maxX = 2.2;
	float minY = -1.6;
	float maxY = 1.6;

	//hard coded range for output
	float xRange = 256;
	float yRange = 256;

	int x = mapRange(input.x, minX, maxX, xRange);
	int y = mapRange(input.y, minY, maxY, yRange);
	//std::cout << "MAPPED: " << x << " " << y << std::endl;
	return (x << 8) + y;
}
std::string processData() {
	std::stringstream other_stream;
		other_stream << v4tos(skeletonPosition[NUI_SKELETON_POSITION_HEAD]);
		other_stream << v4tos(skeletonPosition[NUI_SKELETON_POSITION_SPINE]);
		other_stream << v4tos(skeletonPosition[NUI_SKELETON_POSITION_ELBOW_LEFT]);
		other_stream << v4tos(skeletonPosition[NUI_SKELETON_POSITION_ELBOW_RIGHT]);
		other_stream << v4tos(skeletonPosition[NUI_SKELETON_POSITION_SHOULDER_CENTER]);
		other_stream << v4tos(skeletonPosition[NUI_SKELETON_POSITION_HIP_CENTER]);
		other_stream << v4tos(skeletonPosition[NUI_SKELETON_POSITION_KNEE_LEFT]);
		other_stream << v4tos(skeletonPosition[NUI_SKELETON_POSITION_KNEE_RIGHT]);
		other_stream << v4tos(skeletonPosition[NUI_SKELETON_POSITION_HAND_LEFT]);
		other_stream << v4tos(skeletonPosition[NUI_SKELETON_POSITION_HAND_RIGHT]);
		other_stream << v4tos(skeletonPosition[NUI_SKELETON_POSITION_FOOT_LEFT]);
		other_stream << v4tos(skeletonPosition[NUI_SKELETON_POSITION_FOOT_RIGHT]);
	
	std::cout << "UNPROCESSED:" << other_stream.str() << std::endl << std::endl;
	std::stringstream other_stream2;
	for (int i = 0; i < sizeof(skeletonPosition); i++)
	{
		other_stream2 << v4tos(skeletonPosition[i]);

	}
	std::cout << "DUMP:" << other_stream2.str() << std::endl << std::endl << std::endl;

	//basically we want to compress the essential data as much as possible
	//we can only put 1024 bits of data on the GPU (probably a bit more realistically but for now using mediump ints, lets say 1024)
	//and we want to filter it down on this side to decrease network latency over the websocket
	//structure of json is going to be {data: [0:[headXY,spineXY,elbowLXY, elbowRXY],
	//1:[shouldersXY,hipsXY,kneeLXY,kneeRXY],
	//2:[handLXY,handRXY,footLXY,footRXY]],
	//count:# }}
	//potential todo: not json but base64

	int headXY = xyData(skeletonPosition[NUI_SKELETON_POSITION_HEAD]);
	int spineXY = xyData(skeletonPosition[NUI_SKELETON_POSITION_SPINE]);
	int elbowLXY = xyData(skeletonPosition[NUI_SKELETON_POSITION_ELBOW_LEFT]);
	int elbowRXY = xyData(skeletonPosition[NUI_SKELETON_POSITION_ELBOW_RIGHT]);
	int shouldersXY = xyData(skeletonPosition[NUI_SKELETON_POSITION_SHOULDER_CENTER]);
	int hipsXY = xyData(skeletonPosition[NUI_SKELETON_POSITION_HIP_CENTER]);
	int kneeLXY = xyData(skeletonPosition[NUI_SKELETON_POSITION_KNEE_LEFT]);
	int kneeRXY = xyData(skeletonPosition[NUI_SKELETON_POSITION_KNEE_RIGHT]);
	int handLXY = xyData(skeletonPosition[NUI_SKELETON_POSITION_HAND_LEFT]);
	int handRXY = xyData(skeletonPosition[NUI_SKELETON_POSITION_HAND_RIGHT]);
	int footLXY = xyData(skeletonPosition[NUI_SKELETON_POSITION_FOOT_LEFT]);
	int footRXY = xyData(skeletonPosition[NUI_SKELETON_POSITION_FOOT_RIGHT]);

	std::stringstream stream;
	counter++;
	counter = counter % 600;
	stream << "{";// depth:";

	stream << "\"data\":";
	//skeleton data
	stream << "[[" << headXY << "," << spineXY << "," << elbowLXY << "," << elbowRXY << "],";
	stream << "[" << shouldersXY << "," << hipsXY << "," << kneeLXY << "," << kneeRXY << "],";
	stream << "[" << handLXY << "," << handRXY << "," << footLXY << "," << footRXY << "]],";

	stream << "\"count\":";
	stream << counter;
	stream << "}";

	std::cout << "PROCESSED:" << stream.str() << std::endl;
	return stream.str();
}

