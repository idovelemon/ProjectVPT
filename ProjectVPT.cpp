#include <omp.h>

#include <ctime>
#include <fstream>
#include <iostream>
#include <random>

//-----------------------------------------------------------
// Constant
static const float kEP = 0.00001f;
static const float kPI = 3.1415926f;

//-----------------------------------------------------------
// Random generator
float vtRandom()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    float v = std::generate_canonical<float, 10>(gen);
    return v;
}

//-----------------------------------------------------------
// Bitmap
#pragma pack(2)
struct VTBitmapFileHeader
{
    uint16_t type;
    uint32_t size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t offBits;
};
#pragma pack()

#pragma pack(2)
struct VTBitmapInfoHeader
{
    uint32_t size;
    uint32_t width;
    uint32_t height;
    uint16_t planes;
    uint16_t bitCount;
    uint32_t compression;
    uint32_t sizeImage;
    uint32_t xPelsPerMeter;
    uint32_t yPelsPerMeter;
    uint32_t clrUsed;
    uint32_t clrImportant;
};
#pragma pack()

void vtSavePixelDataToFile(const std::string fileName, uint32_t width, uint32_t height, char* data)
{
    std::ofstream output;
    output.open(fileName.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
    if (output.is_open())
    {
        VTBitmapFileHeader bitmapFileHeader{};
        bitmapFileHeader.type = 0x4d42;
        bitmapFileHeader.size = sizeof(VTBitmapFileHeader) + sizeof(VTBitmapInfoHeader) + (width * height * 4);
        bitmapFileHeader.reserved1 = 0;
        bitmapFileHeader.reserved2 = 0;
        bitmapFileHeader.offBits = sizeof(VTBitmapFileHeader) + sizeof(VTBitmapInfoHeader);

        VTBitmapInfoHeader bitmapInfoHeader{};
        bitmapInfoHeader.size = sizeof(VTBitmapInfoHeader);
        bitmapInfoHeader.width = width;
        bitmapInfoHeader.height = height;
        bitmapInfoHeader.planes = 1;
        bitmapInfoHeader.bitCount = 32;
        bitmapInfoHeader.compression = 0;
        bitmapInfoHeader.sizeImage = (width * height * 4);
        bitmapInfoHeader.xPelsPerMeter = 0;
        bitmapInfoHeader.yPelsPerMeter = 0;
        bitmapInfoHeader.clrUsed = 0;
        bitmapInfoHeader.clrImportant = 0;

        output.write((const char*)&bitmapFileHeader, sizeof(VTBitmapFileHeader));
        output.write((const char*)&bitmapInfoHeader, sizeof(VTBitmapInfoHeader));
        output.write(data, (width * height * 4));
    }
    output.close();
}

//-----------------------------------------------------------
// Vector3
struct VTVec3
{
    union
    {
        struct
        {
            float x;
            float y;
            float z;
        };

        float v[3];
    };

    VTVec3(float x, float y, float z)
        : x(x)
        , y(y)
        , z(z)
    {}

    VTVec3()
        : x(0.0f)
        , y(0.0f)
        , z(0.0f)
    {}

    static VTVec3 Cross(const VTVec3& a, const VTVec3& b)
    {
        VTVec3 result;

        result.x = a.y * b.z - a.z * b.y;
        result.y = a.z * b.x - a.x * b.z;
        result.z = a.x * b.y - a.y * b.x;

        return result;
    }

    static float Dot(const VTVec3& a, const VTVec3& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    static float Length(const VTVec3& a)
    {
        float sqrLength = a.x * a.x + a.y * a.y + a.z * a.z;
        if (sqrLength > kEP)
            return std::sqrtf(sqrLength);
        else
            return 0.0f;
    }

    static VTVec3 Normalize(const VTVec3& a)
    {
        float l = Length(a);
        if (l < kEP)
            return a;
        else
            return VTVec3(a.x / l, a.y / l, a.z / l);
    }
};

VTVec3 operator*(const VTVec3 a, VTVec3 v)
{
    return VTVec3(a.x * v.x, a.y * v.y, a.z * v.z);
}

VTVec3 operator*(const VTVec3 a, float v)
{
    return VTVec3(a.x * v, a.y * v, a.z * v);
}

VTVec3 operator-(const VTVec3 a, VTVec3 b)
{
    return VTVec3(a.x - b.x, a.y - b.y, a.z - b.z);
}

VTVec3 operator+(const VTVec3 a, VTVec3 b)
{
    return VTVec3(a.x + b.x, a.y + b.y, a.z + b.z);
}

//-----------------------------------------------------------
// Triangle
struct VTTriangle
{
    union
    {
        struct
        {
            VTVec3 v0;
            VTVec3 v1;
            VTVec3 v2;
        };
        VTVec3 v[3];
    };

    VTTriangle()
        : v0()
        , v1()
        , v2()
    {
    }

    static bool IntersectOutside(const VTTriangle& tri, const VTVec3& p, const VTVec3& d, float& t)
    {
        VTVec3 q = p + d * 10000.0f;
        VTVec3 ab = tri.v1 - tri.v0;
        VTVec3 ac = tri.v2 - tri.v0;
        VTVec3 qp = p - q;
        VTVec3 n = VTVec3::Cross(ab, ac);
        float s = VTVec3::Dot(qp, n);
        if (s <= 0.0f) return false;

        VTVec3 ap = p - tri.v0;
        t = VTVec3::Dot(ap, n);
        if (t < 0.0f)
        {
            t = 0.0f;
            return false;
        }

        VTVec3 e = VTVec3::Cross(qp, ap);
        float v = VTVec3::Dot(ac, e);
        if (v < 0.0f || v > s) return false;
        float w = -VTVec3::Dot(ab, e);
        if (w < 0.0f || v + w > s) return false;

        float odd = 1.0f / s;
        t = t * odd;

        t = t * 10000.0f;

        return true;
    }

    static bool IntersectInside(const VTTriangle& tri, const VTVec3& p, const VTVec3& d, float& t)
    {
        VTVec3 q = p + d * 10000.0f;
        VTVec3 ab = tri.v2 - tri.v0;
        VTVec3 ac = tri.v1 - tri.v0;
        VTVec3 qp = p - q;
        VTVec3 n = VTVec3::Cross(ab, ac);
        float s = VTVec3::Dot(qp, n);
        if (s <= 0.0f) return false;

        VTVec3 ap = p - tri.v0;
        t = VTVec3::Dot(ap, n);
        if (t < 0.0f)
        {
            t = 0.0f;
            return false;
        }

        VTVec3 e = VTVec3::Cross(qp, ap);
        float v = VTVec3::Dot(ac, e);
        if (v < 0.0f || v > s) return false;
        float w = -VTVec3::Dot(ab, e);
        if (w < 0.0f || v + w > s) return false;

        float odd = 1.0f / s;
        t = t * odd;

        t = t * 10000.0f;

        return true;
    }
};

//-----------------------------------------------------------
// Triangle Mesh
struct VTTriangleMesh
{
    uint32_t triNum;
    VTTriangle* tris;

    VTTriangleMesh()
        : triNum(0)
        , tris(nullptr)
    {
    }

    static bool IntersectOutside(const VTTriangleMesh& mesh, const VTVec3& p, const VTVec3& d, float& t)
    {
        if (mesh.tris == nullptr) return false;

        for (uint32_t triIndex = 0; triIndex < mesh.triNum; triIndex++)
        {
            if (VTTriangle::IntersectOutside(mesh.tris[triIndex], p, d, t))
            {
                return true;
            }
        }

        t = 0.0f;
        return false;
    }

    static bool IntersectInside(const VTTriangleMesh& mesh, const VTVec3& p, const VTVec3& d, float& t)
    {
        if (mesh.tris == nullptr) return false;

        for (uint32_t triIndex = 0; triIndex < mesh.triNum; triIndex++)
        {
            if (VTTriangle::IntersectInside(mesh.tris[triIndex], p, d, t))
            {
                return true;
            }
        }

        t = 0.0f;
        return false;
    }
};

VTTriangleMesh vtLoadMeshFromObjFile(std::string file)
{
    VTTriangleMesh mesh;

    std::ifstream input;
    input.open(file.c_str(), std::ios::in);
    if (input.is_open())
    {
        std::vector<VTVec3> vertex;
        std::vector<VTTriangle> triangles;

        while (!input.eof())
        {
            char v = 0;
            input >> v;
            if (v == 'v')
            {
                float v0 = 0.0f, v1 = 0.0f, v2 = 0.0f;
                input >> v0 >> v1 >> v2;
                vertex.push_back(VTVec3(v0, v1, v2));
            }
            else if (v == 'f')
            {
                int v0 = 0, v1 = 0, v2 = 0;
                input >> v0 >> v1 >> v2;

                VTTriangle tri;
                tri.v0 = vertex[v0 - 1];
                tri.v1 = vertex[v1 - 1];
                tri.v2 = vertex[v2 - 1];
                triangles.push_back(tri);
            }
            else
            {
                const uint32_t kStrLength = 128;
                char str[kStrLength];
                input.getline(str, kStrLength);
            }
        }

        mesh.triNum = triangles.size();
        mesh.tris = new VTTriangle[mesh.triNum];
        memcpy(mesh.tris, triangles.data(), sizeof(VTTriangle) * mesh.triNum);

        vertex.clear();
        triangles.clear();
    }
    input.close();

    return mesh;
}

void vtReleaseMesh(VTTriangleMesh& mesh)
{
    if (mesh.tris != nullptr)
    {
        delete[] mesh.tris;
        mesh.tris = nullptr;
    }
}

//-----------------------------------------------------------
// Medium
struct VTMedium
{
    VTVec3 volumeExtent;
    float albedo;
    float maxExtinction;

    int32_t size;
    float* density;

    float Density(const VTVec3& pos) const
    {
        float stepx = volumeExtent.x / size;
        float stepy = volumeExtent.y / size;
        float stepz = volumeExtent.z / size;

        float fx = (pos.x + volumeExtent.x * 0.5f) / stepx;
        float fy = (pos.y + volumeExtent.y * 0.5f) / stepy;
        float fz = (pos.z + volumeExtent.z * 0.5f) / stepz;

        int32_t vx = static_cast<int32_t>(fx);
        vx = std::min(size - 1, vx);
        int32_t vx1 = std::min(size - 1, vx + 1);
        fx = fx - vx;

        int32_t vy = static_cast<int32_t>(fy);
        vy = std::min(size - 1, vy);
        int32_t vy1 = std::min(size - 1, vy + 1);
        fy = fy - vy;

        int32_t vz = static_cast<int32_t>(fz);
        vz = std::min(size - 1, vz);
        int32_t vz1 = std::min(size - 1, vz + 1);
        fz = fz - vz;

        float d0 = density[vz * size * size + vy * size + vx];
        float d1 = density[vz * size * size + vy * size + vx1];
        float d2 = density[vz * size * size + vy1 * size + vx];
        float d3 = density[vz1 * size * size + vy * size + vx];
        d0 = d0 + (d1 - d0) * fx;
        d0 = d0 + (d2 - d0) * fy;
        d0 = d0 + (d3 - d0) * fz;

        return d0;
    }
};

void vtTriangleMeshToMedium(const VTTriangleMesh& mesh, VTMedium& medium)
{
    int32_t size = medium.size;
    int32_t halfSize = size / 2;
    float stepx = medium.volumeExtent.x / size;
    float stepy = medium.volumeExtent.y / size;
    float stepz = medium.volumeExtent.z / size;

    medium.density = new float[size * size * size];

    #pragma omp parallel for num_threads(4)
    for (int32_t z = 0; z < size; z++)
    {
        for (int32_t y = 0; y < size; y++)
        {
            for (int32_t x = 0; x < size; x++)
            {
                VTVec3 p = VTVec3(stepx * (x - halfSize + 0.5f), stepy * (y - halfSize + 0.5f), stepz * (z - halfSize + 0.5f));
                float t = 0.0f;
                if (VTTriangleMesh::IntersectInside(mesh, p, VTVec3(0.0f, 0.0f, -1.0f), t))
                {
                    medium.density[z * size * size + y * size + x] = medium.maxExtinction;
                }
                else
                {
                    medium.density[z * size * size + y * size + x] = 0.0f;
                }
            }
        }
    }
}

void vtReleaseMedium(VTMedium& medium)
{
    if (medium.density)
    {
        delete[] medium.density;
        medium.density = nullptr;
    }
}

float vtGridProceduralMedium(const VTMedium& medium, const VTVec3& pos)
{
    VTVec3 halfVolumeExtent = medium.volumeExtent * 0.5f;
    VTVec3 s = pos + halfVolumeExtent;

    for (uint32_t step = 0; step < 3; step++)
    {
        s = s * 3.0f;
        const uint32_t t = ((uint32_t)s.x & 1) + ((uint32_t)s.y & 1) + ((uint32_t)s.z & 1);
        if (t >= 2) return 0.0f;
    }

    return medium.maxExtinction;
}

float vtVoxelProceduralMedium(const VTMedium& medium, const VTVec3& pos)
{
    const float r = 0.5f * (0.5f - std::fabsf(pos.y));
    const float a = (float)(kPI * 8.0f) * pos.y;
    const float dx = (std::cosf(a) * r - pos.x) * 2.0f;
    const float dy = (std::sinf(a) * r - pos.z) * 2.0f;
    return std::powf(std::fmaxf((1.0f - dx * dx - dy * dy), 0.0f), 8.0f) * medium.maxExtinction;
}

//-----------------------------------------------------------
// Volume path tracing context
struct VTContext
{
    VTVec3 cameraPos;
    VTVec3 cameraDir;
    float znear;
    float fov;

    VTVec3 ambient;

    VTMedium medium;

    uint32_t maxInteractions;

    uint32_t imageWidth;
    uint32_t imageHeight;
    uint32_t samplePerPixel;
};

//-----------------------------------------------------------
// Core method of volume path tracing
bool vtIntersectVolume(const VTContext& context, const VTVec3& p, const VTVec3& d, float& tmin)
{
    VTVec3 halfVolumeExtent = context.medium.volumeExtent * 0.5f;
    const float x0 = (-halfVolumeExtent.x - p.x) / d.x;
    const float y0 = (-halfVolumeExtent.y - p.y) / d.y;
    const float z0 = (-halfVolumeExtent.z - p.z) / d.z;
    const float x1 = (halfVolumeExtent.x - p.x) / d.x;
    const float y1 = (halfVolumeExtent.y - p.y) / d.y;
    const float z1 = (halfVolumeExtent.z - p.z) / d.z;

    tmin = fmaxf(fmaxf(fmaxf(fminf(z0, z1), fminf(y0, y1)), fminf(x0, x1)), 0.0f);
    const float tmax = fminf(fminf(fmaxf(z0, z1), fmaxf(y0, y1)), fmaxf(x0, x1));
    return (tmin < tmax);
}

bool vtInVolume(const VTContext& context, const VTVec3& pos)
{
    VTVec3 halfVolumeExtent = context.medium.volumeExtent * 0.5f;
    if (-halfVolumeExtent.x > pos.x || halfVolumeExtent.x < pos.x) return false;
    if (-halfVolumeExtent.y > pos.y || halfVolumeExtent.y < pos.y) return false;
    if (-halfVolumeExtent.z > pos.z || halfVolumeExtent.z < pos.z) return false;
    return true;
}

float vtExtinction(const VTContext& context, const VTVec3& pos, const VTVec3& d)
{
    //return context.medium.Density(pos);
    //return vtGridProceduralMedium(context.medium, pos);
    return vtVoxelProceduralMedium(context.medium, pos);
}

bool vtTraceInteraction(const VTContext& context, VTVec3& p, const VTVec3& d)
{
    float t = 0.0f;
    VTVec3 s = p;

    do
    {
        t = t - logf(1.0f - vtRandom()) / context.medium.maxExtinction;

        s = p + d * t;
        if (!vtInVolume(context, s))
        {
            return false;
        }
    } while (vtExtinction(context, s, d) < vtRandom() * context.medium.maxExtinction);

    p = s;
    return true;
}

VTVec3 vtTraceSample(const VTContext& context, uint32_t px, uint32_t py, const VTVec3& p, const VTVec3& d)
{
    float tmin = 0.0f;
    float weight = 1.0f;
    float totalWeight = 0.0f;

    if (vtIntersectVolume(context, p, d, tmin))
    {
        VTVec3 rayPos = p + d * (tmin + 0.01f);

        VTVec3 rayDir = d;
        uint32_t interactions = 0;
        while (vtTraceInteraction(context, rayPos, rayDir))
        {
            // Is the path length exceeded?
            if (interactions++ >= context.maxInteractions)
            {
                return VTVec3(0.0f, 0.0f, 0.0f);
            }

            // Russian roulette absorption
            weight = weight * context.medium.albedo;
            if (weight < 0.2f)
            {
                if (vtRandom() > weight * 5.0f)
                {
                    return VTVec3(0.0f, 0.0f, 0.0f);
                }
                weight = 0.2f;
            }

            // Sample isotropic phase function
            const float phi = (float)(2.0f * kPI) * vtRandom();
            const float cos_theta = 1.0f - 2.0f * vtRandom();
            const float sin_theta = sqrtf(1.0f - cos_theta * cos_theta);
            rayDir = VTVec3(cosf(phi) * sin_theta, sinf(phi) * sin_theta, cos_theta);

            // Accumulate
            totalWeight = totalWeight + weight;
        }

        if (interactions == 0)
        {
            weight = 1.0f;
        }
    }
    else
    {
        weight = 1.0f;
    }

    return context.ambient * weight;
}

void vtVolumePathTrace(const VTContext& context, uint32_t px, uint32_t py, char* data)
{
    float halfImageWidth = context.imageWidth / 2.0f;
    float halfImageHeight = context.imageHeight / 2.0f;

    float cameraSize = context.znear * tanf(context.fov) * 2.0f;

    float widthPerPixel = cameraSize / context.imageWidth;
    float heightPerPixel = cameraSize / context.imageHeight;
    float halfWidthDist = halfImageWidth * widthPerPixel;
    float halfHeightDist = halfImageHeight * heightPerPixel;

    VTVec3 h = VTVec3::Cross(VTVec3(0.0f, 1.0f, 0.0f), context.cameraDir);
    h = VTVec3::Normalize(h);

    VTVec3 u = VTVec3::Cross(context.cameraDir, h);
    u = VTVec3::Normalize(u);

    VTVec3 lb = (context.cameraPos + context.cameraDir * context.znear) - ((const VTVec3)h) * halfWidthDist;
    lb = lb - u * halfHeightDist;

    float sppWidth = widthPerPixel / context.samplePerPixel;
    float sppHeight = heightPerPixel / context.samplePerPixel;

    VTVec3 accumColor;

    for (uint32_t x = 0; x < context.samplePerPixel; x++)
    {
        for (uint32_t y = 0; y < context.samplePerPixel; y++)
        {
            float rx = (px + (x + vtRandom()) * sppWidth) * widthPerPixel;
            float ry = (py + (y + vtRandom()) * sppHeight) * heightPerPixel;

            VTVec3 start = lb + h * rx;
            start = start + u * ry;

            VTVec3 dir = start - context.cameraPos;
            dir = VTVec3::Normalize(dir);

            accumColor = accumColor + vtTraceSample(context, px, py, start, dir);
        }
    }

    accumColor = accumColor * (1.0f / (context.samplePerPixel * context.samplePerPixel));

    accumColor.x = accumColor.x * (1.0f + accumColor.x * 0.1f) / (1.0f + accumColor.x);
    accumColor.y = accumColor.y * (1.0f + accumColor.y * 0.1f) / (1.0f + accumColor.y);
    accumColor.z = accumColor.z * (1.0f + accumColor.z * 0.1f) / (1.0f + accumColor.z);
    const uint32_t r = static_cast<uint32_t>(255u * fminf(powf(fmaxf(accumColor.x, 0.0f), 1.0f / 2.2f), 1.0f));
    const uint32_t g = static_cast<uint32_t>(255u * fminf(powf(fmaxf(accumColor.y, 0.0f), 1.0f / 2.2f), 1.0f));
    const uint32_t b = static_cast<uint32_t>(255u * fminf(powf(fmaxf(accumColor.z, 0.0f), 1.0f / 2.2f), 1.0f));

    data[py * context.imageWidth * 4 + px * 4 + 0] = b;
    data[py * context.imageWidth * 4 + px * 4 + 1] = g;
    data[py * context.imageWidth * 4 + px * 4 + 2] = r;
    data[py * context.imageWidth * 4 + px * 4 + 3] = 255u;
}

//-----------------------------------------------------------
// Entry
int main()
{
    // Prepare Medium
    VTMedium medium;
    medium.albedo = 0.8f;
    medium.maxExtinction = 200.0f;
    medium.size = 100;
    medium.volumeExtent = VTVec3(1.0f, 1.0f, 1.0f);

    std::cout << "Prepare medium begin" << std::endl;

    //VTTriangleMesh mesh = vtLoadMeshFromObjFile("monkey.obj");

    //VTTriangleMesh mesh = vtLoadMeshFromObjFile("sphere.obj");
    //vtTriangleMeshToMedium(mesh, medium);

    std::cout << "Prepare medium finish" << std::endl;

    VTVec3 cameraPos = VTVec3(0.0f, 0.1f, -1.2f);
    VTVec3 cameraDir = VTVec3() - cameraPos;
    cameraDir = VTVec3::Normalize(cameraDir);

    VTContext context{};
    context.ambient = VTVec3(2.0f, 2.0f, 2.0f);
    context.cameraDir = cameraDir;
    context.cameraPos = cameraPos;
    context.fov = 0.25f * kPI;
    context.imageHeight = 512;
    context.imageWidth = 512;
    context.maxInteractions = 1024;
    context.medium = medium;
    context.samplePerPixel = 32;
    context.znear = 0.01f;

    char* image = new char[context.imageWidth * context.imageHeight * 4];

    clock_t start = clock();

    #pragma omp parallel for num_threads(4)
    for (int32_t py = 0; py < static_cast<int>(context.imageHeight); py++)
    {
        for (int32_t px = 0; px < static_cast<int>(context.imageWidth); px++)
        {
            vtVolumePathTrace(context, px, py, image);

            std::cout << "Traced (" << px << "," << py << ") OK" << std::endl;
        }
    }
    clock_t end = clock();

    int sec = (end - start) / CLOCKS_PER_SEC;
    std::cout << "Total Rendering Time: " << sec << " ms" << std::endl;

    vtSavePixelDataToFile("volume.bmp", context.imageWidth, context.imageHeight, image);

    //vtReleaseMesh(mesh);
    //vtReleaseMedium(medium);

    delete[] image;
    image = nullptr;

    system("pause");

    return 0;
}