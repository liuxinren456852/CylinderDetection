#include <stdio.h>
#include <vector>
#include <set> 
#include <map>
#include <string>
#include <iostream>
#include <cmath>
#include <limits>
#include <algorithm>

struct Point
{
	float x;
	float y;
	float z;
	Point (float x, float y, float z)
		: x(x)
		, y(y)
		, z(z)
	{
	}
	
	Point()
	{
		
	}
	
	static Point Constant(float v)
	{
		return Point(v, v, v);
	}
};

struct PointCloud 
{
	std::vector<Point> points;
	
	float size() const 
	{
		Point min = Point::Constant(std::numeric_limits<float>::max());
		Point max = Point::Constant(-std::numeric_limits<float>::max());
		for (size_t i = 0; i < points.size(); i++)
		{
			Point p = points[i];
			min.x = std::min(min.x, p.x);
			min.y = std::min(min.y, p.y);
			min.z = std::min(min.z, p.z);
			max.x = std::max(max.x, p.x);
			max.y = std::max(max.y, p.y);
			max.z = std::max(max.z, p.z);
		}
		return std::max(max.x - min.x, std::max(max.y - min.y, max.z - min.z));
	}
};

struct Cylinder
{
	std::set<size_t> inliers;
	float center[3];
	float normal[3];
	float radius;
};

class Octree
{
public:
	Octree(const PointCloud *pointCloud)
		: mPointCloud(pointCloud)
		, mRoot(this)
		, mParent(NULL)
		, mLevel(0)
		, mLeaves(std::vector<Octree*>(pointCloud->points.size(), this))
	{
		Point min = Point::Constant(std::numeric_limits<float>::max());
		Point max = Point::Constant(-std::numeric_limits<float>::max());
		for (size_t i = 0; i < pointCloud->points.size(); i++)
		{
			Point p = pointCloud->points[i];
			min.x = std::min(min.x, p.x);
			min.y = std::min(min.y, p.y);
			min.z = std::min(min.z, p.z);
			max.x = std::max(max.x, p.x);
			max.y = std::max(max.y, p.y);
			max.z = std::max(max.z, p.z);
			mPoints.push_back(i);
		}
		mCenter = Point((min.x + max.x) / 2, (min.y + max.y) / 2, (min.z + max.z) / 2);
		mSize = std::max(max.x - min.x, std::max(max.y - min.y, max.z - min.z)) / 2;
		for (size_t i = 0; i < 8; i++)
		{
			mChild[i] = NULL;
		}
	}
	
	~Octree()
	{
		for (size_t i = 0; i < 8; i++)
		{
			if (mChild[i] != NULL)
			{
				delete mChild[i];
			}
		}
	}
	
	void create(float minSize)
	{
		//if (mSize < minSize) return; 
		if (mPoints.empty() || mLevel > 9) return;
		float newSize = mSize / 2;
		Point newCenters[8] = {
			Point(mCenter.x - newSize, mCenter.y - newSize, mCenter.z - newSize),
			Point(mCenter.x - newSize, mCenter.y - newSize, mCenter.z + newSize),
			Point(mCenter.x - newSize, mCenter.y + newSize, mCenter.z - newSize),
			Point(mCenter.x - newSize, mCenter.y + newSize, mCenter.z + newSize),
			Point(mCenter.x + newSize, mCenter.y - newSize, mCenter.z - newSize),
			Point(mCenter.x + newSize, mCenter.y - newSize, mCenter.z + newSize),
			Point(mCenter.x + newSize, mCenter.y + newSize, mCenter.z - newSize),
			Point(mCenter.x + newSize, mCenter.y + newSize, mCenter.z + newSize)
		};
		for (size_t i = 0; i < 8; i++)
		{
			mChild[i] = new Octree(this, newCenters[i], newSize);
		}
		for (const size_t &p : mPoints)
		{
			Point point = mPointCloud->points[p];
			size_t index = ((point.x > mCenter.x) << 2) | ((point.y > mCenter.y) << 1) | (point.z > mCenter.z);
			mChild[index]->mPoints.push_back(p);
			mRoot->mLeaves[p] = mChild[index];
		} 
		mPoints.clear();
		for (size_t i = 0; i < 8; i++)
		{
			mChild[i]->create(minSize);
		}
	}
	
	bool isLeaf() const 
	{
		for (size_t i = 0; i < 8; i++)
		{
			if (mChild[i] != NULL) return false;
		}
		return true;
	}
	
	const Octree* findLeaf(size_t index) const 
	{
		return mLeaves[index];
	}
	
	float size() const 
	{
		return mSize; 
	}
	
private:
	const PointCloud *mPointCloud;
	Octree *mRoot;
	Octree *mParent;
	Octree *mChild[8];
	std::vector<size_t> mPoints;
	Point mCenter;
	float mSize;
	size_t mLevel;
	std::vector<Octree*> mLeaves;
	
	Octree(Octree *parent, Point center, float size)
		: mPointCloud(parent->mPointCloud)
		, mRoot(parent->mRoot)
		, mParent(parent)
		, mCenter(center)
		, mSize(size)
		, mLevel(parent->mLevel + 1)
	{
		for (size_t i = 0; i < 8; i++)
		{
			mChild[i] = NULL;
		}
	}
	
};

void readPointCloud(const std::string &filename, PointCloud &pointCloud) 
{
	FILE *fp = fopen(filename.c_str(), "rb");
	if (fp == NULL)
	{
		std::cerr << "Could not open file: " << filename << std::endl;
		getchar(); 
		exit(-1);
	}
	size_t size, mode;
	fread(&size, sizeof(size_t), 1, fp);
	fread(&mode, sizeof(size_t), 1, fp);
	for (size_t i = 0; i < size; i++) 
	{
		if (i % 1000 == 0)
		{
			std::cout << i / float(size) * 100 << "..." << std::endl; 
		}
		float position[3], color[3], intensity, normal[3], normalConfidence, curvature;
		fread(position, sizeof(float), 3, fp);
		if (mode & 1)
			fread(color, sizeof(float), 3, fp);
		if (mode & 2)
			fread(&intensity, sizeof(float), 1, fp);
		if (mode & 4)
			fread(normal, sizeof(float), 3, fp);
		if (mode & 8)
			fread(&normalConfidence, sizeof(float), 1, fp);
		if (mode & 16)
			fread(&curvature, sizeof(float), 1, fp); 
		Point p(position[0], position[1], position[2]);
		pointCloud.points.push_back(p); 
	}

	fclose(fp);
}

void readCylinders(const std::string &file, Octree &octree, std::vector<Cylinder*> &cylinders)
{
	FILE *fp = fopen(file.c_str(), "rb");
	if (fp == NULL)
	{
		std::cerr << "Could not open file " << file << std::endl;
		exit(-1);
	}
	size_t numCircles;
	fread(&numCircles, sizeof(size_t), 1, fp);
	size_t numPlanes;
	fread(&numPlanes, sizeof(size_t), 1, fp);
	size_t numCylinders;
	fread(&numCylinders, sizeof(size_t), 1, fp);
	for (size_t i = 0; i < numCylinders; i++)
	{
		Cylinder *cylinder = new Cylinder;
		float color[3];
		float height;
		fread(color, sizeof(float), 3, fp);
		fread(cylinder->center, sizeof(float), 3, fp);
		fread(cylinder->normal, sizeof(float), 3, fp);
		fread(&cylinder->radius, sizeof(float), 1, fp);
		fread(&height, sizeof(float), 1, fp);
		size_t numInliers;
		fread(&numInliers, sizeof(size_t), 1, fp);
		size_t *p = new size_t[numInliers];
		fread(p, sizeof(size_t), numInliers, fp);
		for (size_t j = 0; j < numInliers; j++)
		{
			cylinder->inliers.insert(p[j]);
		}
		delete[] p;
		cylinders.push_back(cylinder);
	}
	fclose(fp);
}
 
bool isCylinderEquivalent(Cylinder *a, Cylinder *b)
{
	float dot = std::abs(a->normal[0] * b->normal[0] + a->normal[1] * b->normal[1] + a->normal[2] * b->normal[2]);
	if (dot < 0.86) return false; // 30 degrees
	if (std::max(a->radius, b->radius) / std::min(a->radius, b->radius) > 2) return false;
	return true;
}

Cylinder* findCorrespondence(std::vector<Cylinder*> &groundtruth, Cylinder *testCylinder)
{
	std::map<Cylinder*, size_t> countInliers;
	for (const size_t &inlier : testCylinder->inliers)
	{
		bool found = false;
		for (Cylinder *cylinder : groundtruth)
		{
			if (cylinder->inliers.find(inlier) != cylinder->inliers.end())
			{
				countInliers[cylinder] += 1;
				found = true;
				break;
			}
		}
		if (!found)
			countInliers[NULL] += 1;
	}
	Cylinder *bestCylinder = NULL;
	for (auto it = countInliers.begin(); it != countInliers.end(); ++it)
	{
		if (it->second > countInliers[bestCylinder])
		{
			bestCylinder = it->first;
		}
	} 
	if (bestCylinder == NULL || countInliers[bestCylinder] < testCylinder->inliers.size() / 2 || !isCylinderEquivalent(bestCylinder, testCylinder))
		return NULL; 
	return bestCylinder;
}

void findCorrespondences(std::vector<Cylinder*> &groundtruth, std::vector<Cylinder*> &test, std::map<Cylinder*, Cylinder*> &correspondences)
{
	for (Cylinder *testCylinder : test)
	{
		correspondences[testCylinder] = findCorrespondence(groundtruth, testCylinder);
	}
}

float getPrecision(Octree &octree, std::map<Cylinder*, Cylinder*> &correspondences)
{
	std::set<const Octree*> allLeaves;
	std::set<const Octree*> correctLeaves; 
	for (auto it = correspondences.begin(); it != correspondences.end(); ++it)
	{
		for (const size_t &inlier : it->first->inliers)
		{
			allLeaves.insert(octree.findLeaf(inlier));
			if (it->second != NULL && it->second->inliers.find(inlier) != it->second->inliers.end())
			{
				correctLeaves.insert(octree.findLeaf(inlier));
			}
		}
	}
	return correctLeaves.size() / float(allLeaves.size());
}

float getRecall(Octree &octree, std::vector<Cylinder*> &groundtruth, std::map<Cylinder*, Cylinder*> &correspondences)
{
	std::set<const Octree*> allLeaves;
	std::set<const Octree*> correctLeaves; 
	for (Cylinder *cylinder : groundtruth)
	{
		for (const size_t &inlier : cylinder->inliers)
		{
			allLeaves.insert(octree.findLeaf(inlier));
			for (auto it = correspondences.begin(); it != correspondences.end(); ++it)
			{
				if (it->second == cylinder && it->first->inliers.find(inlier) != it->first->inliers.end())
				{
					correctLeaves.insert(octree.findLeaf(inlier));
					break;
				}
			}
		}
	}
	return correctLeaves.size() / float(allLeaves.size());
}

int main(int argc, char **argv)
{
	if (argc < 4)
	{
		std::cerr << "Usage: compare <directory> <dataset_name> <technique_suffix>" << std::endl; 
		exit(-1);
	}
	PointCloud pointCloud;
	readPointCloud(std::string(argv[1]) + std::string(argv[2]) + "_" + std::string(argv[3]) + ".pcl", pointCloud);
	float minSize = pointCloud.size() * 0.01f;
	Octree octree(&pointCloud);
	octree.create(minSize);
	std::vector<Cylinder*> groundtruth, test;
	readCylinders(std::string(argv[1]) + std::string(argv[2]) + "_ground_truth.geo", octree, groundtruth);
	readCylinders(std::string(argv[1]) + std::string(argv[2]) + "_" + std::string(argv[3]) + "_" + std::string(argv[3]) + ".geo", octree, test);
	std::map<Cylinder*, Cylinder*> correspondences;
	findCorrespondences(groundtruth, test, correspondences); 
	float precision = getPrecision(octree, correspondences);
	float recall = getRecall(octree, groundtruth, correspondences);
	float f1 = 2 * (precision * recall) / (precision + recall);
	std::cout << "Precision:\t\t\t" << precision << std::endl
				<< "Recall:\t\t\t\t" << recall << std::endl
				<< "F1-Score:\t\t\t" << f1 << std::endl;
	std::set<Cylinder*> corrects;
	for (auto it = correspondences.begin(); it != correspondences.end(); ++it)
	{
		if (it->second != NULL) corrects.insert(it->second);
	}
	std::cout << corrects.size() << "/" << groundtruth.size() << std::endl;
	std::set<const Octree*> cylinderPoints, nonCylinderPoints;
	for (size_t i = 0; i < pointCloud.points.size(); i++)
	{
		bool isCylinder = false;
		for (const Cylinder *cylinder : groundtruth)
		{
			if (cylinder->inliers.find(i) != cylinder->inliers.end())
			{
				isCylinder = true;
				break;
			}
		}
		if (isCylinder)
		{
			cylinderPoints.insert(octree.findLeaf(i));
		}
		else
		{
			nonCylinderPoints.insert(octree.findLeaf(i));
		}
	}
	std::cout << "% cylinder regions = " << cylinderPoints.size() / float(cylinderPoints.size() + nonCylinderPoints.size()) << std::endl;
	return 0;
}
