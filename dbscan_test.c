#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>

#define MAX_POINTS 1000
#define UNVISITED -1
#define NOISE -2

// 定義 GTRACK_measurementPoint 結構
typedef struct {
    union {
        struct {
            float range;
            float azimuth;  // 方位角，單位為度
            float elev;
            float doppler;
        };
        float array[4];
    } vector;
    float snr;
} GTRACK_measurementPoint;

typedef struct {
    int cluster[MAX_POINTS];
    int visited[MAX_POINTS];
} DBSCANResult;

float calculateDistance(GTRACK_measurementPoint p1, GTRACK_measurementPoint p2) {
    float azimuth1_rad = p1.vector.azimuth * (M_PI / 180.0f);
    float azimuth2_rad = p2.vector.azimuth * (M_PI / 180.0f);
    return sqrtf(
        powf(p1.vector.range * sinf(azimuth1_rad) - p2.vector.range * sinf(azimuth2_rad), 2) +
        powf(p1.vector.range * cosf(azimuth1_rad) - p2.vector.range * cosf(azimuth2_rad), 2));
}

int findNeighbors(GTRACK_measurementPoint *points, int numPoints, int index, float eps, int *neighbors) {
    int count = 0;
    for (int i = 0; i < numPoints; i++) {
        if (i != index && calculateDistance(points[index], points[i]) <= eps) {
            neighbors[count++] = i;
        }
    }
    return count;
}

void pointDbscan(GTRACK_measurementPoint *points, int numPoints, float eps, int minSamples, DBSCANResult *result) {
    int clusterId = 0;
    int neighbors[MAX_POINTS];

    for (int i = 0; i < numPoints; i++) {
        result->visited[i] = UNVISITED;
        result->cluster[i] = UNVISITED;
    }

    for (int i = 0; i < numPoints; i++) {
        if (result->visited[i] != UNVISITED) continue;

        result->visited[i] = 1;
        int neighborCount = findNeighbors(points, numPoints, i, eps, neighbors);

        if (neighborCount < minSamples) {
            result->cluster[i] = NOISE;
        } else {
            clusterId++;
            result->cluster[i] = clusterId;

            for (int j = 0; j < neighborCount; j++) {
                int neighborIdx = neighbors[j];
                if (result->visited[neighborIdx] == UNVISITED) {
                    result->visited[neighborIdx] = 1;
                    int nextNeighbors[MAX_POINTS];
                    int nextNeighborCount = findNeighbors(points, numPoints, neighborIdx, eps, nextNeighbors);
                    if (nextNeighborCount >= minSamples) {
                        for (int k = 0; k < nextNeighborCount; k++) {
                            neighbors[neighborCount++] = nextNeighbors[k];
                        }
                    }
                }
                if (result->cluster[neighborIdx] == UNVISITED) {
                    result->cluster[neighborIdx] = clusterId;
                }
            }
        }
    }
}

void findMaxClusterBounds(GTRACK_measurementPoint *points, int numPoints, DBSCANResult *result,
                          float *xmin, float *ymin, float *xmax, float *ymax) {
    int clusterCounts[MAX_POINTS] = {0};
    int maxCluster = -1;
    int maxClusterSize = 0;

    for (int i = 0; i < numPoints; i++) {
        if (result->cluster[i] >= 0) {
            clusterCounts[result->cluster[i]]++;
            if (clusterCounts[result->cluster[i]] > maxClusterSize) {
                maxClusterSize = clusterCounts[result->cluster[i]];
                maxCluster = result->cluster[i];
            }
        }
    }
    printf("maxCluster : %d\n", maxCluster);

    *xmin = FLT_MAX;
    *ymin = FLT_MAX;
    *xmax = -FLT_MAX;
    *ymax = -FLT_MAX;

    for (int i = 0; i < numPoints; i++) {
        if (result->cluster[i] == maxCluster) {
            float azimuth_rad = points[i].vector.azimuth * (M_PI / 180.0f);
            float x = points[i].vector.range * sinf(azimuth_rad);
            float y = points[i].vector.range * cosf(azimuth_rad);

            if (x < *xmin) *xmin = x;
            if (y < *ymin) *ymin = y;
            if (x > *xmax) *xmax = x;
            if (y > *ymax) *ymax = y;
            printf("i: %d, ymin: %.2f, xmax: %.2f, ymax: %.2f\n", i, *xmin, *ymin, *xmax, *ymax);
        }
    }
}

// 主函數
int main() {
    int mNum = 16;
    GTRACK_measurementPoint points[] = {
        {{0.640625, -68.318503, 0.0, 0.0}, 0},
        {{0.625, -61.100577, 0.0, 0.0}, 0},
        {{4.859375, -21.597823, 0.0, 0.0}, 0},
        {{4.828125, -19.023989, 0.0, 0.0}, 0},
        {{17.375, 10.127477, 0.0, 0.0}, 0},
        {{17.15625, 13.876322, 0.0, 0.0}, 0},
        {{17.609375, 15.778721, 0.0, 0.0}, 0},
        {{17.578125, 19.639471, 0.0, 0.0}, 0},
        {{5.875, 24.507374, 0.0, 0.0}, 0},
        {{5.875, 27.360973, 0.0, 0.0}, 0},
        {{5.875, 27.081208, 0.0, 0.0}, 0},
        {{6.265625, 33.347934, 0.0, 0.0}, 0},
        {{22.375, 32.732452, 0.0, 0.0}, 0},
        {{5.71875, 34.411039, 0.0, 0.0}, 0},
        {{6.28125, 37.040826, 0.0, 0.0}, 0},
        {{22.40625, 35.977721, 0.0, 0.0}, 0}
    };

    float eps = 2.0;
    int minSamples = 3;

    DBSCANResult result;
    pointDbscan(points, mNum, eps, minSamples, &result);
    for (int i = 0; i < mNum; i++) {
        float azimuth_rad = points[i].vector.azimuth * (M_PI / 180.0f);
        float x = points[i].vector.range * sinf(azimuth_rad);
        float y = points[i].vector.range * cosf(azimuth_rad);
        printf("Point %d: cluster =%d, range=%.2f, azi=%.2f(degree), x=%.2f, y=%.2f\n",
               i, result.cluster[i], points[i].vector.range, 
               points[i].vector.azimuth, x, y);
    }


    float xmin, ymin, xmax, ymax;
    findMaxClusterBounds(points, mNum, &result, &xmin, &ymin, &xmax, &ymax);
    printf("==================== \n");

    printf("xmin: %.2f, ymin: %.2f, xmax: %.2f, ymax: %.2f\n", xmin, ymin, xmax, ymax);

    return 0;
}
