#include "Round.h"
#include <cmath>

#ifdef _MSC_VER
int roundToInt(float x)
{
    return static_cast<int>(floor(x + 0.5f));
}
#elif _WIN32
#include <cmath>
/*inline float round(float x) { return x < 0.0f ? ceil(x - 0.5f) : floor(x + 0.5f); }
inline double round(double x) { return x < 0.0 ? ceil(x - 0.5) : floor(x + 0.5); }*/
inline int roundToInt(float x) { return x < 0.0f ? ceil(x - 0.5f) : floor(x + 0.5f); }
#endif
