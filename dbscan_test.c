#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>

#define MAX_POINTS 1000
#define UNVISITED -1
#define NOISE -2

typedef struct {
    union {
        struct {
            float range;
            float azimuth;  
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
        if (i != index && points[i].snr >= 25 && calculateDistance(points[index], points[i]) <= eps) {
            neighbors[count++] = i;
            // float azimuth_rad = points[i].vector.azimuth * (M_PI / 180.0f);
            // float x = points[i].vector.range * sinf(azimuth_rad);
            // float y = points[i].vector.range * cosf(azimuth_rad);
            // printf("Point %d: range=%.2f, azi=%.2f(degree), x=%.2f, y=%.2f\n",
            //    i, points[i].vector.range, 
            //    points[i].vector.azimuth, x, y);
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
            printf("i: %d, xmin: %.2f,ymin: %.2f, xmax: %.2f, ymax: %.2f\n", i, *xmin, *ymin, *xmax, *ymax);
        }
    }
}

// 主函數
int main() {
    int mNum = 16;
    // 317
    // GTRACK_measurementPoint points[] = {
    //     {{0.640625, -68.318503, 0.0, 0.0}, 12},
    //     {{0.625, -61.100577, 0.0, 0.0}, 12},
    //     {{4.859375, -21.597823, 0.0, 0.0}, 14},
    //     {{4.828125, -19.023989, 0.0, 0.0}, 14},
    //     {{17.375, 10.127477, 0.0, 0.0}, 8},
    //     {{17.15625, 13.876322, 0.0, 0.0}, 9},
    //     {{17.609375, 15.778721, 0.0, 0.0}, 8},
    //     {{17.578125, 19.639471, 0.0, 0.0}, 8},
    //     {{5.875, 24.507374, 0.0, 0.0}, 24},
    //     {{5.875, 27.360973, 0.0, 0.0}, 22},
    //     {{5.875, 27.081208, 0.0, 0.0}, 23},
    //     {{6.265625, 33.347934, 0.0, 0.0}, 13},
    //     {{22.375, 32.732452, 0.0, 0.0}, 8},
    //     {{5.71875, 34.411039, 0.0, 0.0}, 13},
    //     {{6.28125, 37.040826, 0.0, 0.0}, 12},
    //     {{22.40625, 35.977721, 0.0, 0.0}, 8}
    // };

    // 320
    // GTRACK_measurementPoint points[] = {
    //     {{0.640625, -63.78631592, 0.0, 0.0}, 8},
    //     {{0.609375, -49.9099960, 0.0, 0.0}, 9},
    //     {{0.609375, -46.72068024, 0.0, 0.0}, 9},
    //     {{0.59375, -40.901577, 0.0, 0.0}, 9},
    //     {{4.796875, -5.763149738, 0.0, 0.0}, 22},
    //     {{4.90625, -5.595291138, 0.0, 0.0}, 25},
    //     {{4.90625, -5.595291138, 0.0, 0.0}, 25},
    //     {{5.15625, -4.196468353, 0.0, 0.0}, 20},
    //     {{4.34375, -1.902398944, 0.0, 0.0}, 12},
    //     {{4.234375, 0, 0.0, 0.0}, 12},
    //     {{4.234375, 1.846446037, 0.0, 0.0}, 12},
    //     {{4.234375, 2.741692543, 0.0, 0.0}, 12},
    //     {{6.28125, 18.6323185, 0.0, 0.0}, 11},
    //     {{6.265625, 18.6323185, 0.0, 0.0}, 11},
    //     {{6.203125, 24.50737381, 0.0, 0.0}, 10},
    //     {{9.453125, 31.66934776, 0.0, 0.0}, 9},
    //     {{9.453125, 32.73245239, 0.0, 0.0}, 9},
    //     {{9.09375, 35.97772217, 0.0, 0.0}, 9},
    //     {{6.125, 36.53725052, 0.0, 0.0}, 16},
    //     {{9.09375, 35.97772217, 0.0, 0.0}, 9},
    //     {{9.46875, 39.72656631, 0.0, 0.0}, 9},
    //     {{9.71875, 46.32901001, 0.0, 0.0}, 8},
    //     {{0.59375, 56.45648575, 0.0, 0.0}, 8},
    //     {{6.09375, 56.45648575, 0.0, 0.0}, 14}
    // };

    // 326
    GTRACK_measurementPoint points[] = {
        {{0.625, -59.47794342, 0.0, 0.0}, 13},
        {{0.625, -59.47794342, 0.0, 0.0}, 13},
        {{5.125, -22.43711662, 0.0, 0.0}, 22},
        {{4.859375, -14.82752132, 0.0, 0.0}, 17},
        {{4.84375, -14.82752132, 0.0, 0.0}, 17},
        {{5.6875, 11.52629948, 0.0, 0.0}, 27},
        {{5.875, 11.97392273, 0.0, 0.0}, 29},
        {{5.875, 11.97392273, 0.0, 0.0}, 28},
        {{6.640625, 13.87632179, 0.0, 0.0}, 11},
        {{6.640625, 13.87632179, 0.0, 0.0}, 10},
        {{6.046875, 16.72991943, 0.0, 0.0}, 28},
        {{6.265625, 18.91208267, 0.0, 0.0}, 22},
        {{6.828125, 21.87758827, 0.0, 0.0}, 10},
        {{7.890625, 22.54902267, 0.0, 0.0}, 7},
        {{6.84375, 26.85739708, 0.0, 0.0}, 10}
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
