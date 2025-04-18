#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <stdbool.h>

#define MAX_POINTS 50
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
    float range;
    float azimuth;
    float doppler;
} Point;

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
        if (i != index && points[i].snr >= 25 && abs(points[i].vector.doppler) < 0.2 && calculateDistance(points[index], points[i]) <= eps) {
            printf(" with %d point dis is : %.2f \n", i, calculateDistance(points[index], points[i]));
            neighbors[count++] = i;
        }
    }
    printf(" neighborCount : %d\n", count);
    return count;
}

void pointDbscan(GTRACK_measurementPoint *points, int numPoints, float eps, int minSamples, DBSCANResult *result) {
    int clusterId = 0;
    int neighbors[MAX_POINTS];


    for (int i = 0; i < numPoints; i++) {        
        result->visited[i] = UNVISITED;
        result->cluster[i] = UNVISITED;
        printf("init %d %d %d \n", i, result->visited[i], result->cluster[i]);
    }

    for (int i = 0; i < numPoints; i++) {
        printf("points %d : it is visited : %d \n", i, result->visited[i]);
        if (result->visited[i] != UNVISITED) continue;

        printf("go head \n");
        result->visited[i] = 1;
        int neighborCount = findNeighbors(points, numPoints, i, eps, neighbors);        

        // if (neighborCount < minSamples  || points[i].snr < 25 || points[i].snr >= 30  || abs(points[i].doppler) >= 0.2f) {   // 20250217 update
        if (neighborCount < minSamples || points[i].snr < 25) {
            printf("    points %d : is noise\n", i);
            result->cluster[i] = NOISE;
        } 
        else 
        {            
            clusterId++;
            result->cluster[i] = clusterId;

            for (int j = 0; j < neighborCount; j++) {
                printf("    in for %d neighborIdx : %d \n", j, neighbors[j]);
                int neighborIdx = neighbors[j];
                // if (points[neighborIdx].snr > 25) continue;
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
                if (result->cluster[neighborIdx] == UNVISITED || result->cluster[neighborIdx] == NOISE) {
                    result->cluster[neighborIdx] = clusterId;
                }
            }
        }
    }
}

bool checkCondition(GTRACK_measurementPoint *cluster, int clusterSize) {
    if (clusterSize == 0) return false;
    int countPoints = 0;
    for (int i = 0; i < clusterSize; i++) {
        if (fabs(cluster[i].vector.doppler) < 0.3f) countPoints++;
    }
    return countPoints > (0.5 * clusterSize);
}

void getLargestCluster(GTRACK_measurementPoint *points, int numPoints, DBSCANResult *result, int *clusterSize, float *xmin, float *ymin, float *xmax, float *ymax) {
    int clusterCounts[MAX_POINTS] = {0};
    int sortedClusters[MAX_POINTS];
    int uniqueClusters = 0;
    
    for (int i = 0; i < numPoints; i++) {
        if (result->cluster[i] >= 0) {
            int clusterId = result->cluster[i];
            if (clusterCounts[clusterId] == 0) {
                sortedClusters[uniqueClusters++] = clusterId;
            }
            clusterCounts[clusterId]++;
        }
    }

    if (uniqueClusters == 0) {
        *clusterSize = 0;
        return;
    }

    for (int i = 0; i < uniqueClusters - 1; i++) {
        for (int j = i + 1; j < uniqueClusters; j++) {
            if (clusterCounts[sortedClusters[j]] > clusterCounts[sortedClusters[i]]) {
                int temp = sortedClusters[i];
                sortedClusters[i] = sortedClusters[j];
                sortedClusters[j] = temp;
            }
        }
    }


    int targetCluster = -1;
    for (int k = 0; k < uniqueClusters; k++) {
        int clusterId = sortedClusters[k];
        int currentSize = clusterCounts[clusterId];
        GTRACK_measurementPoint *candidateCluster = (GTRACK_measurementPoint*) malloc(currentSize * sizeof(GTRACK_measurementPoint));
        int index = 0;
        
        for (int i = 0; i < numPoints; i++) {
            if (result->cluster[i] == clusterId) {
                candidateCluster[index++] = points[i];
            }
        }

        if (checkCondition(candidateCluster, currentSize)) {
            targetCluster = clusterId;
            free(candidateCluster);
            break;
        }
        free(candidateCluster);
    }
    
    if (targetCluster == -1) {
        *clusterSize = 0;
        return;
    }

    int finalSize = clusterCounts[targetCluster];
    GTRACK_measurementPoint *finalCluster = (GTRACK_measurementPoint*) malloc(finalSize * sizeof(GTRACK_measurementPoint));
    int index = 0;
    *xmin = 99.0f;
    *ymin = 99.0f;
    *xmax = -99.0f;
    *ymax = -99.0f;
    
    for (int i = 0; i < numPoints; i++) {
        if (result->cluster[i] == targetCluster) {
            finalCluster[index++] = points[i];
            float azimuth_rad = points[i].vector.azimuth * (M_PI / 180.0f);
            float x = points[i].vector.range * sinf(azimuth_rad);
            float y = points[i].vector.range * cosf(azimuth_rad);
            int x_scaled = (int)(x * 100);
            int y_scaled = (int)(y * 100);

            if (x < *xmin) *xmin = x;
            if (y < *ymin) *ymin = y;
            if (x > *xmax) *xmax = x;
            if (y > *ymax) *ymax = y;
            printf("i: %d, x: %.d,y: %.d\n", i, x_scaled, y_scaled);
        }
    }

    *clusterSize = finalSize;
    return;
}


void computePairwiseDistanceMatrix(GTRACK_measurementPoint *points, int numPoints) {
    printf("Pairwise Distance Matrix:\n");
    for (int i = 0; i < numPoints; i++) {
        // if (i == 0 || i == 3 || i == 5 || i == 10 || i == 12){
        for (int j = 0; j < numPoints; j++) {
            if (i != j) {
                printf("%6.2f ", calculateDistance(points[i], points[j]));
            } else {
                printf("%6.2f ", 0.0);
            }
        } 
        printf("\n");            
        // }       
    }
}

int main() {    
    // 317
    // int mNum = 16;
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
    // GTRACK_measurementPoint points[] = {
    //     {{0.625, -59.47794342, 0.0, 0.0}, 13},
    //     {{0.625, -59.47794342, 0.0, 0.0}, 13},
    //     {{5.125, -22.43711662, 0.0, 0.0}, 22},
    //     {{4.859375, -14.82752132, 0.0, 0.0}, 17},
    //     {{4.84375, -14.82752132, 0.0, 0.0}, 17},
    //     {{5.6875, 11.52629948, 0.0, 0.0}, 27},
    //     {{5.875, 11.97392273, 0.0, 0.0}, 29},
    //     {{5.875, 11.97392273, 0.0, 0.0}, 28},
    //     {{6.640625, 13.87632179, 0.0, 0.0}, 11},
    //     {{6.640625, 13.87632179, 0.0, 0.0}, 10},
    //     {{6.046875, 16.72991943, 0.0, 0.0}, 28},
    //     {{6.265625, 18.91208267, 0.0, 0.0}, 22},
    //     {{6.828125, 21.87758827, 0.0, 0.0}, 10},
    //     {{7.890625, 22.54902267, 0.0, 0.0}, 7},
    //     {{6.84375, 26.85739708, 0.0, 0.0}, 10}
    // };

    // run 2 101
    // int mNum = 21;
    // GTRACK_measurementPoint points[] = {
    //     {{8.203125, -35.47, 0.0, 0.15}, 11},
    //     {{6.9375, -5.595, 0.0, 0.15}, 18},
    //     {{4.859375, -3.9726, 0.0, 0.15}, 22},
    //     {{4.875, -4.08456, 0.0, 0.15}, 25},
    //     {{4.4375, -1.902, 0.0, 0.15}, 8},
    //     {{4.765625, 0.223, 0.0, 0.15}, 25},
    //     {{6.6875, -0.2797, 0.0, 0.15}, 18},
    //     {{6.65625, 3.189, 0.0, 0.15}, 18},
    //     {{5.453125, 10.127, 0.0, 0.15}, 20},
    //     {{5.46875, 10.127, 0.0, 0.15}, 20},
    //     {{6.359375, 11.97, 0.0, 0.15}, 25},
    //     {{6.171875, 13.204, 0.0, 0.15}, 24},
    //     {{6.34375, 11.973, 0.0, 0.15}, 25},
    //     {{6.32125, 16.7299, 0.0, 0.15}, 24},
    //     {{8.890625, 31.669, 0.0, 0.15}, 13},
    //     {{9.078125, 33.795, 0.0, 0.15}, 8},
    //     {{8.890625, 34.858, 0.0, 0.15}, 13},
    //     {{9.078125, 33.795, 0.0, 0.15}, 8},
    //     {{0.59375, 52.539, 0.0, 0.15}, 10},
    //     {{0.609375, 61.772, 0.0, 0.15}, 10},
    //     {{0.609375, 61.772, 0.0, 0.15}, 10},        
    // };

    // run 2 frame : 113
    // int mNum = 28;
    // GTRACK_measurementPoint points[] = {
    //     {{0.640625,-69.32565308,0.0, 0.15625}, 11},
    //     {{0.640625,-63.78631592,0.0, 0.15625}, 11},
    //     {{0.625,-55.44933319,0.0, 0.15625}, 11},
    //     {{4.515625,-14.82752132,0.0, 0.15625}, 11},
    //     {{5.078125,-16.67396736,0.0, 0.15625}, 28},
    //     {{4.515625,-14.82752132,0.0, 0.15625}, 11},
    //     {{4.875,-12.2536869,0.0, 0.15625}, 26},
    //     {{4.953125,-12.47749901,0.0, 0.15625}, 29},
    //     {{5.34375,0.895246565,0.0, -0.15625}, 27},
    //     {{5.390625,3.636939049,0.0, -0.15625}, 26},
    //     {{5.765625,6.71434927,0.0, -0.15625}, 25},
    //     {{6.125,8.281030655,0.0, -0.15625}, 19},
    //     {{6.140625,10.79891109,0.0, -0.15625}, 19},
    //     {{7.203125,23.83593941,0.0, -0.15625}, 13},
    //     {{7.25,26.52167892,0.0, -0.15625}, 12},
    //     {{8.53125,33.23602676,0.0, -0.15625}, 13},
    //     {{9.125,33.79555893,0.0, -0.15625}, 18},
    //     {{1.765625,38.21583557,0.0, 9.25} ,8},
    //     {{8.546875,38.21583557,0.0, -0.15625}, 12},
    //     {{9.125,38.21583557,0.0, -0.15625}, 18},
    //     {{9.3125,37.82416534,0.0, -0.15625}, 18},
    //     {{8.5625,38.21583557,0.0, -0.15625}, 12},
    //     {{9.125,38.21583557,0.0, -0.15625}, 18},
    //     {{9.71875,39.33489609,0.0, -0.15625}, 11},
    //     {{1.78125,42.85992813,0.0, 9.25} ,8},
    //     {{9.3125,41.29324722,0.0, -0.15625}, 18},
    //     {{1.78125,42.85992813,0.0, 9.25} ,8},
    //     {{9.96875,45.48971558,0.0, -0.15625}, 10}            
    // };

    int mNum = 28;
    GTRACK_measurementPoint points[] = {
        {{0.640625,-69.32565308,0.0, 0.15625}, 11},
        {{0.640625,-63.78631592,0.0, 0.15625}, 11},
        {{0.625,-55.44933319,0.0, 0.15625}, 11},
        {{4.515625,-14.82752132,0.0, 0.15625}, 11},
        {{5.078125,-16.67396736,0.0, 0.65625}, 28},
        {{4.515625,-14.82752132,0.0, 0.15625}, 11},
        {{4.875,-12.2536869,0.0, 0.65625}, 26},
        {{4.953125,-12.47749901,0.0, 0.65625}, 29},
        {{5.34375,0.895246565,0.0, -0.15625}, 27},
        {{5.390625,3.636939049,0.0, -0.15625}, 26},
        {{5.765625,6.71434927,0.0, -0.15625}, 25},
        {{6.125,8.281030655,0.0, -0.15625}, 19},
        {{6.140625,10.79891109,0.0, -0.15625}, 19},
        {{7.203125,23.83593941,0.0, -0.15625}, 13},
        {{7.25,26.52167892,0.0, -0.15625}, 12},
        {{8.53125,33.23602676,0.0, -0.15625}, 13},
        {{9.125,33.79555893,0.0, -0.15625}, 18},
        {{8.765625,38.21583557,0.0, 9.25} ,29},
        {{8.546875,38.21583557,0.0, -0.15625}, 29},
        {{9.125,38.21583557,0.0, -0.15625}, 29},
        {{9.3125,37.92416534,0.0, -0.15625}, 29},
        {{8.5625,38.21583557,0.0, -0.15625}, 29},
        {{9.125,38.21583557,0.0, -0.15625}, 29},
        {{9.71875,38.33489609,0.0, -0.15625}, 18},
        {{9.78125,38.85992813,0.0, 9.25} ,29},
        {{9.3125,41.29324722,0.0, -0.15625}, 18},
        {{1.78125,42.85992813,0.0, 9.25} ,8},
        {{9.96875,45.48971558,0.0, -0.15625}, 10}            
    };

    float eps = 2.0f;
    int minSamples = 3;

    DBSCANResult result;
    
    pointDbscan(points, mNum, eps, minSamples, &result);
    for (int i = 0; i < mNum; i++) {        
        float azimuth_rad = points[i].vector.azimuth * (M_PI / 180.0f);
        float x = points[i].vector.range * sinf(azimuth_rad);
        float y = points[i].vector.range * cosf(azimuth_rad);
        int x_scaled = (int)(x * 100);
        int y_scaled = (int)(y * 100);
        printf("Point %d: cluster =%d, range=%.3f, azi=%.3f(degree), x=%.3f, y=%.3f\n",
               i, result.cluster[i], points[i].vector.range, 
               points[i].vector.azimuth, x, y);        
    }


    float xmin, ymin, xmax, ymax;
    int clusterSize;
    getLargestCluster(points, mNum, &result, &clusterSize, &xmin, &ymin, &xmax, &ymax);

    printf("====================\n");

    if (clusterSize > 0) {
        printf("Largest Cluster Size: %d\n", clusterSize);
        printf("xmin: %.2f, ymin: %.2f, xmax: %.2f, ymax: %.2f\n", xmin, ymin, xmax, ymax);
    } else {
        printf("No valid cluster found.\n");
    }

    // computePairwiseDistanceMatrix(points, mNum);

    return 0;
}
