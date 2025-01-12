#include <stdio.h>
#include <math.h>

typedef struct {
    float range;
    float azimuth; // 以角度為單位
} Point;

void testConversion(Point *points, int numPoints) {
    for (int i = 0; i < numPoints; i++) {
        float azimuth_rad = points[i].azimuth * (M_PI / 180.0f); // 角度轉弧度
        float x = points[i].range * cosf(azimuth_rad);
        float y = points[i].range * sinf(azimuth_rad);

        printf("Point %d: Range = %.2f, Azimuth = %.2f°\n", i, points[i].range, points[i].azimuth);
        printf("          X = %.2f, Y = %.2f\n", x, y);
    }
}

int main() {
    Point points[] = {
        {10.0, 0.0},    // 在 x 軸上
        {10.0, 90.0},   // 在 y 軸上
        {10.0, 180.0},  // 在負 x 軸上
        {10.0, 270.0},  // 在負 y 軸上
        {10.0, 45.0},   // 在第一象限
        {10.0, 135.0},  // 在第二象限
        {10.0, 225.0},  // 在第三象限
        {10.0, 315.0},  // 在第四象限
    };
    int numPoints = sizeof(points) / sizeof(points[0]);

    testConversion(points, numPoints);

    return 0;
}
